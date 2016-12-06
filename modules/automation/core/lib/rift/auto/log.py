"""
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

@file log_scraper.py
@date 12/14/2015
@brief Simple scraper for scraping remote logs using ssh and tail
"""

import io
import logging
import subprocess

logger = logging.getLogger(__name__)


class LogManager(object):
    '''Log source and sink Manager
    '''

    REPORT_LOG_FMT='''\
|--|  BEGIN LOG CAPTURE - {test}
{data}\
|--|  END LOG CAPTURE - {test}
'''

    ENTRY_LOG_FMT='''\
|>>|    {source}
{data}
|<<|    {source}
'''

    def __init__(self):
        self.sources = []
        self.sinks = []
        self.current_test_name = "pre-test"

    def source(self, source):
        self.sources.append(source)

    def sink(self, sink):
        self.sinks.append(sink)

    def set_test_name(self, name):
        self.current_test_name = name

    def dispatch(self):
        data = ''
        for source in self.sources:
            source_data = source.read()
            if source_data:
                data += LogManager.ENTRY_LOG_FMT.format(source=source, data=source_data)

        if data:
            capture = LogManager.REPORT_LOG_FMT.format(test=self.current_test_name, data=data)
            [sink.write(capture) for sink in self.sinks]

class FileSource(object):
    '''Scraper for grabbing data from remote logs via ssh'''

    def __init__(self, host, path):
        '''Initializes a FileSource that scrapes the resource located at path

        Arguments:
            host - host on which the resource is located
            path - path to resource
        '''
        self.lines_scraped = 1
        self.log_size = 0
        self.host = host
        self.path = path
        self.ssh_cmd = ''
        if not self.host in ['localhost', '127.0.0.1']:
            self.ssh_cmd = 'ssh -q -n -o BatchMode=yes -o StrictHostKeyChecking=no -- {host} '.format(host=self.host)

    def __str__(self):
        return 'File Source {} {}'.format(self.host, self.path)

    def connect(self, host):
        '''Associate scraper with the specified host'''
        self.host = host
        self.lines_scraped = 1
        self.log_size = 0
        self.ssh_cmd = ''
        if not self.host in ['localhost', '127.0.0.1']:
            self.ssh_cmd = 'ssh -q -n -o BatchMode=yes -o StrictHostKeyChecking=no -- {host} '.format(host=self.host)

    def connected(self):
        '''Return whether or not the scraper has been assigned a host'''
        return self.host is not None

    def skip_to(self, string):
        if self.host is None:
            return 'Skipped skip - Source not connected'

        try:
            skip_cmd = ('{ssh_cmd} \"grep -n \'{string}\' {path} | tail -n 1 | cut -d : -f 1\"').format(
                            ssh_cmd=self.ssh_cmd,
                            path=self.path,
                            string=string,
                       )
            destination_line = subprocess.check_output(skip_cmd, shell=True)
            if destination_line:
                self.lines_scraped = int(destination_line)

        except subprocess.CalledProcessError:
            logger.info("Failed to skip to %s in FileSource %s:%s", string, self.host, self.path, exc_info=True)
            return ""


    def read(self):
        '''Retrieve new content from the resource'''
        if self.host is None:
            return 'Skipped scrape - Source not connected'

        stat_cmd = ('{ssh_cmd} stat --format=\"%s\" {path}').format(
                        ssh_cmd=self.ssh_cmd,
                        path=self.path
                    )
        try:
            log_size = subprocess.check_output(stat_cmd, shell=True)
            log_size = int(log_size)

            if log_size < self.log_size:
                msg = "FileSource %s:%s shrunk in size %d -> %d - Assuming rotated or truncated."
                logger.info(msg, self.host, self.path, self.log_size, log_size)
                self.reset()

            self.log_size = log_size

        except subprocess.CalledProcessError:
            logger.info("Failed to stat FileSource %s:%s", self.host, self.path)
            return ""

        tail_cmd = ('{ssh_cmd} tail -n +{lines_scraped} {path}').format(
                        ssh_cmd=self.ssh_cmd,
                        lines_scraped=self.lines_scraped,
                        path=self.path
                    )

        try :
            scraped = subprocess.check_output(tail_cmd, shell=True)
            self.lines_scraped += len(scraped.splitlines())
            try:
                return scraped.decode('utf-8')
            except UnicodeDecodeError:
                return scraped
        except subprocess.CalledProcessError:
            logger.info("Failed to scrape FileSource %s:%s", self.host, self.path, exc_info=True)
            return ""

    def reset(self):
        '''seek back to the beginning of the resource, should be used in the case
        that the targeted resource is expected to be truncated or if the resource
        being targeted behaves like a stream rather than a file
        '''
        self.lines_scraped = 1

class LoggerSource(object):
    '''Scraper for grabbing data from python logger'''

    def __init__(self, logger, level=logging.DEBUG):
        '''Initializes a LoggerSource that scrapes python log entries

        Arguments:
            logger - logger to attach log handler to
            level - logging level to capture at
        '''
        self.pos = 0
        self.logger = logger
        self.log_capture = io.StringIO()
        self.handler = logging.StreamHandler(self.log_capture)
        self.handler.setLevel(level)
        self.logger.addHandler(self.handler)
        self.log_capture.seek(0)

    def __str__(self):
        return 'LoggerSource {} {}'.format(logger, logger.name)

    def read(self):
        '''Retrieve new content from the resource'''
        self.log_capture.seek(self.pos)
        data = self.log_capture.read()
        self.pos = self.log_capture.tell()
        return data

    def reset(self):
        '''seek back to the beginning of the resource, should be used in the case
        that the targeted resource is expected to be truncated or if the resource
        being targeted behaves like a stream rather than a file
        '''
        self.pos = 0

class FileSink(object):
    def __init__(self, path, host=None):
        self.host = host
        self.path = path

    def write(self, data):
        if self.host in [None, 'localhost', '127.0.0.1']:
            with open(self.path, "a+") as out:
              out.write(data)

class MemorySink(object):
    def __init__(self):
        self.data = []

    def read(self):
        return "\n".join(self.data)

    def write(self, data):
        self.data.append(data)

    def truncate(self, generations):
        if generations == 0:
            self.data = []
        else:
            self.data = self.data[-generations:]

