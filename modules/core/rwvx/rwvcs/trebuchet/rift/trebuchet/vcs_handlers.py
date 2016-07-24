
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

import atexit
import errno
import logging
import json
import os
import subprocess
import tempfile
import time
import traceback

import tornado.gen
import tornado.web


class InvalidLauncherRequest(Exception):
    pass


class VMProcess(object):
    """
    Class representing a Rift.VM process.

    Each Rift.VM process is actually a native Linux process which runs the
    'rwmain' executable.  These are tracked by this class which provides
    mechanisms to check on the process status and pull stdout/stderr from the
    process.
    """
    IDLE = 0
    RUNNING = 1
    CRASHED = 2

    def __init__(self, name, instance):
        """
        Create an instance of a VMProcess

        @param name     - component name of the Rift.VM
        @param instance - componet instance
        """
        self.name = name
        self.instance = instance

        self._proc = None
        self._log = tempfile.NamedTemporaryFile(
                prefix='%s-%s-' % (self.name, self.instance),
                delete=False,
                dir='/tmp/')
        os.fchmod(self._log.fileno(), 0o666)

    def __eq__(self, o):
        return (self.name == o.name and self.instance == o.instance)

    def __neq__(self, o):
        return self != o

    @staticmethod
    def status_string(status):
        if status == VMProcess.IDLE:
            return 'IDLE'
        elif status == VMProcess.RUNNING:
            return 'RUNNING'
        elif status == VMProcess.CRASHED:
            return 'CRASHED'
        else:
            return 'UNKNOWN'

    @property
    def returncode(self):
        """
        Accessor to the subprocess.returncode property.  None if the
        process is still running and filled out with the exit status
        otherwise.
        """
        return self._proc.returncode

    @property
    def status(self):
        """
        Current state of the subprocess
        """
        if self._proc is None:
            return self.IDLE
        elif self._proc.poll() is None:
            return self.RUNNING
        else:
            return self.CRASHED

    @property
    def log(self):
        """
        Returns a string which contains all stdout/stderr printed by the
        subprocess to date.
        """
        if self._proc is None:
            return ''

        with open(self._log.name, 'r') as fp:
            return fp.read()

    def terminate(self):
        """
        Stop the subprocess and remove the stdout/stderr log
        """
        if self.status == self.RUNNING:
            self._proc.terminate()
            for i in range(100):
                if self.status != self.RUNNING:
                    break
                time.sleep(0.01)
            else:
                self._proc.kill()

        if os.path.exists(self._log.name):
            os.unlink(self._log.name)

    def start(self, rwmain, manifest_path, parent_id, ip_address, uid=None):
        """
        Start the subprocess which will represent a running Rift.VM

        @param rwmain         - path to the rwmain executable
        @param manifest_path  - path to the bootstrap manifest
        @param parent_id      - instance-name of the parent
        @param ip_address     - ip address of the Rift.VM
        @param uid            - if defined, uid that owns the Rift.VM process
        """
        rwmain = os.path.join(os.path.realpath(os.curdir), os.path.normpath(rwmain))
        wd = os.path.dirname(rwmain)

        args = []
        if uid is not None:
            args.extend([
                'sudo',
                '--non-interactive',
                '--user', uid])

        args.extend([
                rwmain,
                '--manifest', manifest_path,
                '--name', self.name,
                '--instance', str(self.instance),
                '--type', 'rwvm',
                '--parent', parent_id,
                '--ip_address', ip_address])
        self._proc = subprocess.Popen(
                args,
                stdout=self._log,
                stderr=subprocess.STDOUT,
                close_fds=True,
                cwd=wd)
        atexit.register(self.terminate)


class Ping(tornado.web.RequestHandler):
    """
    Ping-Pong REST API

    Simple handler which will respond to a ping request with a pong.

    Output:
        pong: {
            time: Time when ping was sent.
        }
    """
    def __init__(self, *args, **kwds):
        super(Ping, self).__init__(*args, **kwds)

    def get(self):
        """
        Respond to an http GET.
        """
        resp = {'pong': time.time()}
        logging.debug('PING sending %s' % (resp,))

        self.set_header('Content-type', 'application/json')
        self.write(json.dumps(resp))


class VMLauncher(tornado.web.RequestHandler):
    """
    Start VM REST API

    Start a Rift.VM process on the current physical machine.  Note that the
    bootstrap manifest can be specified in one of two ways.  All other
    arguments are required.

    manifest_data:  This may be filled with the contents of the bootstrap
        manifest.  In this case a unique location in /tmp will be chosen to
        write the bootstrap manifest and the path will be passed to the Rift.VM
        process.
    manifest_path:  If the bootstrap manifest is known to exist on the current
        system at a specific path, it may be specified here and will directly
        be passed to the Rift.VM process.

    Input:
        {
            manifest_data: contents of the bootstrap manifest
            manfiest_path: local path to the bootstrap manifest

            component_name: Rift.VM component name as defined in the bootstrap
                manifest
            instance_id:    Instance ID for this instance of the Rift.VM
                component
            parent_id:      Rift.VM instance name of the parent
            ip_address:     IP Address of the Rift.VM to be started
            uid:            If enabled at the server level, the uid that will
                            launch the Rift.VM process (via sudo)
        }
    Output:
        {
            ret:      0 on success or errno
            errmsg:   on error, more verbose error message
            log:      on error, stdout/stderr from launch attempt
        }
    """
    def __init__(self, *args, **kwds):
        super(VMLauncher, self).__init__(*args, **kwds)

    @staticmethod
    def validate_request(req):
        """
        Validate that a request has all the required fields filled.

        @raises InvalidLaunchRequest if the request is missing fields.
        """
        if not ('manifest_data' in req or 'manifest_path' in req):
            raise InvalidLauncherRequest('No manifest specified')

        keys = (
            'component_name',
            'instance_id',
            'parent_id',
            'ip_address',
            'uid')
        for key in keys:
            if not key in req:
                raise InvalidLauncherRequest('%s not specified' % (key,))

    @tornado.web.asynchronous
    def post(self, *args, **kwds):
        try:
            self._post(*args, **kwds)
        except:
            traceback.print_exc()
            self.send_error(500)
            self.finish()

    @tornado.web.asynchronous
    def _post(self, *args, **kwds):
        try:
            json_data = json.loads(self.request.body)
        except ValueError as e:
            self.send_error(400, message='JSON parsing failed: %s' % (e,))
            self.finish()
            return

        self.set_header('Content-type', 'application/json')

        try:
            self.validate_request(json_data)
        except InvalidLauncherRequest as e:
            ret = {
                'ret': errno.EINVAL,
                'errmsg': 'Invalid request: %s' % (e,),
                'log': '',
            }
            self.write(json.dumps(ret))
            self.finish()
            return

        manifest_path = ''
        if 'manifest_path' in json_data:
            manifest_path = json_data['manifest_path']
        else:
            try:
                with tempfile.NamedTemporaryFile(prefix='manifest', suffix='.xml', delete=False) as mf:
                    mf.write(json_data['manifest_data'])
                    manifest_path = mf.name
            except OSError as e:
                if manifest_path != '':
                    os.unlink(manifest_path)
                ret = {
                    'ret': e.errno,
                    'errmsg': 'Failed to write manifest file: %s' % (e.strerror,),
                    'log': None,
                }
                self.write(json.dumps(ret))
                self.finish()
                return


        vm = VMProcess(
                json_data['component_name'],
                json_data['instance_id'])

        if vm in self.application.vms:
            msg = 'Rift.VM %s-%s is already being managed' % (
                    json_data['component_name'],
                    json_data['instance_id'])
            ret = {
                'ret':  errno.EEXIST,
                'errmsg': msg,
                'log': None,
            }
            self.write(json.dumps(ret))
            self.finish()
            return

        vm.start(
                self.application.rwmain,
                manifest_path,
                json_data['parent_id'],
                json_data['ip_address'],
                json_data['uid'] if self.application.suid else None)

        @tornado.gen.coroutine
        def check_vm():
            ret = {}

            self.application.vms.add(vm)
            if vm.status == vm.RUNNING:
                ret = {
                    'ret': 0,
                    'errmsg': None,
                    'log': None,
                }
            else:
                ret = {
                    'ret': vm.returncode,
                    'errmsg': 'Rift.VM died after 1s',
                    'log': vm.log,
                }
            logging.debug('LAUNCH sending %s' % (ret,))
            self.write(json.dumps(ret))
            self.finish()


        self.application.loop.call_later(1, check_vm)


class VMStatus(tornado.web.RequestHandler):
    """
    VM Status REST API

    Get the status of a Rift.VM being mananged by this server.

    Output:
        {
            status: Status of the requested vm.  This is one of IDLE, RUNNING,
                    CRASHED or NOTFOUND.
        }
    """
    def get(self, name, instance):
        status = 'NOTFOUND'
        instance = int(instance)

        for vm in self.application.vms:
            if name == vm.name and instance == vm.instance:
                status = vm.status_string(vm.status)
                break

        ret = {
            'status': status,
        }
        logging.debug('STATUS sending %s' % (ret,))
        self.write(json.dumps(ret))


class VMTerminator(tornado.web.RequestHandler):
    """
    VM Terminator REST API

    Terminate a running Rift.VM process.

    Output:
        {
            ret: 0 on success, errno otherwise
        }
    """
    def delete(self, name, instance):
        status = errno.ENOENT
        instance = int(instance)

        for vm in self.application.vms:
            if name == vm.name and instance == vm.instance:
                vm.terminate()
                status = 0
                self.application.vms.remove(vm)
                break

        ret = {
            'status': status,
        }
        logging.debug('TERMINATE sending %s' % (ret,))
        self.write(json.dumps(ret))


# vim: sw=4
