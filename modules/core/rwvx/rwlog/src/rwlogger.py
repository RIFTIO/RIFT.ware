"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file rwlogger.py
@author Austin Cormier (austin.cormier@riftio.com)
@date 01/04/2014
@brief Python wrapper for RwLog GI

"""
import contextlib
import gi
import importlib
import logging
import mmap
import os
import struct
import sys
import threading
import traceback

gi.require_version('RwLog', '1.0')
gi.require_version('RwGenericLogYang', '1.0')

from gi.repository import RwLog
import six

# We really need to also import rwlog categories as defined in the
# original yang enum, but these are passed as a function argument to
# the rwlog API, and the Gi bindings can't do that yet.


logger = logging.getLogger(__name__)

def get_frame(frames_above=1):
    frame = sys._getframe(frames_above + 1)
    return frame


def get_caller_filename(frame):
    # Expects the caller to be two stack frames above us
    # The first stack from will be from a function in this module
    filename = frame.f_code.co_filename
    return os.path.basename(filename)


def get_caller_lineno(frame):
    # Expects the caller to be two stack frames above us
    # The first stack from will be from a function in this module
    line_number = frame.f_lineno
    return line_number

def get_module_name_from_log_category(log_category):
    """
    Function that converts category name to Python module name
    Eg. rw-generic-log to RwGenericLogYang
    """
    words = log_category.split('-')
    words.append('yang')
    return ''.join(word.capitalize() for word in words)

def add_root_handler_warn_wrapper():
    """ A useful function which can be called to help identify
        where root loggers are being added """
    def wrapper(*args, **kwargs):
        traceback.print_exc()

    logging.getLogger().addHandler = wrapper


class ThreadFilter(object):
    """ A simple logging filter which filters on thread ident """
    def __init__(self, thread_id):
        self._thread_id = thread_id

    @property
    def thread_id(self):
        return self._thread_id

    def __eq__(self, other):
        return self._thread_id == other._thread_id

    def __ne__(self, other):
        return not self.__eq__(other)

    def filter(self, record):
        return record.thread == self._thread_id

    @classmethod
    def from_current(cls):
        """  Create a LogThreadFilter using the current thread id """
        return cls(threading.current_thread().ident)


class RwLogger(logging.StreamHandler):
    level_event_cls_map = {
        logging.DEBUG: 'Debug',
        logging.INFO: 'Info',
        logging.WARN: 'Warn',
        logging.ERROR: 'Error',
        logging.CRITICAL: 'Critical',
        }

    #Map rw-log.yang defined log severity to Python logging level
    # Not sure how to get Yang defined enum in Gi and so using enum value 0-7 directly
    log_levels_map = {
        0: logging.CRITICAL,
        1: logging.CRITICAL,
        2: logging.CRITICAL,
        3: logging.ERROR,
        4: logging.WARNING,
        5: logging.INFO,
        6: logging.INFO,
        7: logging.DEBUG
        }

    # If we are running unit tests that involve tasklets, we don't want to have to include
    # rwlog-d in order to get rwlog message on the console.  To work around this, we can
    # put this class into a "utest" mode which will cause this to act as a StreamHandler.
    utest_mode = False
    utest_mode_log_level = logging.WARNING

    def __init__(self, category="rw-generic-log", subcategory="tasklet", log_hdl=None, file_name=None):
        """Create an instance of RwLogger

        Arguments:
            category - The category name to include in log events incase of Python logger.
            log_hdl - Use an existing rwlog handle instead of creating a new one.
            file_name - Pass in a filepath to use as the source file instead of
                        detecting it automatically.
        """
        super(RwLogger, self).__init__()
        self._category = None
        self._subcategory = None

        if RwLogger.utest_mode:
            fmt = logging.Formatter(
                    '%(asctime)-23s %(levelname)-5s  (%(name)s@%(process)d:%(filename)s:%(lineno)d) - %(message)s')
            self.setFormatter(fmt)
            self.setLevel(RwLogger.utest_mode_log_level)
        # GBoxed types don't accept constructors will arguments
        # RwLog.Ctx(file_name) will throw an error, so call
        # new directly
        if log_hdl is None:
            if file_name is None:
                frame = get_frame()
                file_name = get_caller_filename(frame)

            log_hdl = RwLog.Ctx.new(file_name)

        self._log_hdl = log_hdl

        self.set_subcategory(subcategory)
        self.set_category(category)

        self._shm_filename = os.path.join("/dev/shm", self._log_hdl.get_shm_filter_name())
        try:
            self._shm_fd = open(self._shm_filename,'rb')
            self._shm_data = mmap.mmap(
                    self._shm_fd.fileno(),
                    length=0,
                    flags=mmap.MAP_SHARED,
                    prot=mmap.PROT_READ
                    )

        except Exception as e:
            msg = "Failed to open shm file: %s with exception %s" % (self._shm_filename, repr(e))
            print(msg)
            logger.error(msg)

        self._log_serial_no = None
        self._log_severity = 7  # Default sev is debug
        self._group_id = None
        self._rwlogd_inited = False

    @classmethod
    def from_instance(cls, rwlog):
        return cls(
                   log_hdl=rwlog.log_hdl,
                   category=rwlog.category,
                   subcategory=rwlog.subcategory
                   )

    def close(self):
        try:
            self._shm_data.close()
            self._shm_fd.close()
        except Exception:
            pass

    def set_group_id(self, group_id):
        if not isinstance(group_id, int):
            logger.warning("Group id not an int, could not set rwlogger group id")
            return

        self._group_id = group_id

    def set_subcategory(self, subcategory):
        if subcategory is not None:
            if not isinstance(subcategory, six.string_types):
                raise TypeError("Category should be a string")

        self._subcategory = subcategory

    def set_category(self, category_name):
        """
           Set Log category name to be used.
           Arguments:
              category_name (String): Yang module name that has log notifications. module_name will be used as category_name
           Returns: None
        """
        if category_name == self._category:
            return

        try:
            module_name = get_module_name_from_log_category(category_name)

            gi.require_version(module_name, '1.0')
            log_yang_module = importlib.import_module('gi.repository.' + module_name)

            if not log_yang_module:
                logger.error("Module %s is not found to be added as log category for %s", module_name, category_name)
                print("Module %s is not found to be added as log category for %s", module_name, category_name)
                return
            for level in RwLogger.level_event_cls_map.values():
                if not hasattr(log_yang_module, level):
                    logger.error("Module %s does not have required log notification for %s", module_name, level)
                    print("Module %s does not have required log notification for %s", module_name, level)
                    return
            self._log_yang_module = log_yang_module
            self._category = category_name

        except Exception as e:
            logger.exception("Caught error %s when trying to set log category (%s)",
                             repr(e), category_name)

    @property
    def category(self):
        return self._category

    @property
    def subcategory(self):
        return self._subcategory

    @property
    def log_hdl(self):
        return self._log_hdl

    def log_event(self, event, frames_above=0):
        try:
            if not event.template_params.filename:
                frame = get_frame(frames_above + 1)
                event.template_params.filename = get_caller_filename(frame)
                event.template_params.linenumber = get_caller_lineno(frame)

            if self._group_id is not None:
                event.template_params.call_identifier.groupcallid = self._group_id

            pb = event.to_ptr()
            self._log_hdl.proto_send(pb)

        except Exception:
            logger.exception("Caught error when attempting log event (%s)", event)

    def emit(self, record):
        if RwLogger.utest_mode:
            super(RwLogger, self).emit(record)
            return

        # We check rwlog_shm pid is valid to infer shm is valid
        if self._rwlogd_inited is False:
            self._rwlogd_inited = True
            rwlogd_pid = struct.unpack('I', self._shm_data[16:20])[0]
            try:
                os.kill(rwlogd_pid, 0)
            except OSError as e:
                # Rwlogd is not yet initialised;
                self._rwlogd_inited = False
            except Exception:
                logger.exception("Caught error when attempting to check rwlogd pid is present for log record (%s)", record)

        if self._rwlogd_inited is True:
            #offset 4-8 in shm holds log serial no and so we check it to know if
            #there is any change in logging config
            serial_no = struct.unpack('I', self._shm_data[4:8])[0]
            # If serial no has changed; update severity level
            if serial_no != self._log_serial_no:
                self._log_serial_no = serial_no
                # Currently we direclty get enum value as interger in Gi
                # Need to find out how to get enum type
                self._log_severity = self._log_hdl.get_category_severity_level(self._category)

        try:
            if record.levelno < RwLogger.log_levels_map[self._log_severity]:
                return

            if record.levelno in RwLogger.level_event_cls_map:
                event_cls = RwLogger.level_event_cls_map[record.levelno]
            elif record.levelno < logging.DEBUG:
                event_cls = RwLogger.level_event_cls_map[logging.DEBUG]
            elif record.levelno > logging.CRITICAL:
                event_cls = RwLogger.level_event_cls_map[logging.CRITICAL]
            else:
                logger.error("Could not find a suitable rwlog event mapping")
                return

            event = getattr(self._log_yang_module, event_cls)()
            event.log = self.format(record)
            event.logger = record.name
            if self.subcategory is not None:
                event.category = self.subcategory
            event.template_params.linenumber = record.lineno
            event.template_params.filename = os.path.basename(record.pathname)

            self.log_event(event)

        except Exception:
            logger.exception("Caught error when attempting log record (%s)", record)


@contextlib.contextmanager
def rwlog_root_handler(rwlog):
    """ Add a thread-filtered RwLogger() handler to the root logger

    When interacting with libraries within a threaded-context, the library
    logger has no knowledge of the rwlog handler the tasklet is using.

    To address this, create a context which adds a rwlogger handler
    to the root logger so that logging messages that propogate up from
    non-tasklet Loggers (which have propagate set to False), will get handled.

    To prevent messages from other libraries (in other threads), add a ThreadFilter
    so only messages originating from the current thread will be handled.

    To support nested contexts, check for existing RwLogger handlers with
    equal ThreadFilter and save/restore as neccessary.

    Arguments:
        rwlog - An existing RwLogger handler instance to clone-from
                and add to the root logger.

    """
    rwlog = RwLogger.from_instance(rwlog)

    thread_filter = ThreadFilter.from_current()
    rwlog.addFilter(thread_filter)

    root_logger = logging.getLogger()

    # Save the existing root logger handlers support nested contexts
    rwlog_hdlrs = [h for h in root_logger.handlers if isinstance(h, RwLogger)]
    thread_hdlrs = [h for h in rwlog_hdlrs if thread_filter in h.filters]
    for h in thread_hdlrs:
        root_logger.removeHandler(h)

    root_logger.addHandler(rwlog)

    try:
        yield rwlog

    finally:
        rwlog.removeFilter(thread_filter)
        root_logger.removeHandler(rwlog)
        rwlog.close()

        for h in thread_hdlrs:
            root_logger.addHandler(h)
