#!/usr/bin/env python3
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

# file ssh.py
# author Karun Ganesharatnam (karun.ganesharatnam@riftio.com)
# date 04/27/2016
# brief module contains classes and functions providing functionality
#       for starting an SSH session and execute command interactively
#
import logging
import paramiko
import os
import re
import shutil
import time
import traceback

logger = logging.getLogger(__name__)

# Turn the debug log from paramiko.traspord module
paramiko.util.logging.getLogger('paramiko.transport').setLevel(logging.INFO)

class NoPromptError(Exception):
    pass

class SessionError(Exception):
    pass

class Session:
    """
    This class provides functionality to start a remote ssh session and
    run commands on the SSH shell
    """

    def __init__(self, remote_host, collect_output=True, prompt=None):
        """
        Arguments:
            remote_host - Host name or IP address of the SSH host
            collect_output - Flag denotes whether to save the command output
                        which can be retrieved via call to 'read_output'
                        function
            prompt - Optional. Regular expression matching the device prompt which is used
                     to verify the command completion by checking presence of the device prompt
        """
        self.remote_host = remote_host
        self.error = ""
        self.remote_conn_pre = paramiko.SSHClient()
        self.remote_conn_pre.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.remote_conn = None
        self.cum_ouput = ""
        self.last_read = ""
        self.cum_ouput_locked = False
        self.collect_output = collect_output
        self.prompt = prompt
        self.last_exit_code = -1

    def connect(self, username=None, key_filename=None, raise_except=True):
        """
        To start a new ssh session

        Arguments:
            username - authentication user name
            key_filename - file name or list of file names of private key(s)
            raise_except - Flag denoting whether to throw the connection exception instead
                        of the returning a False
        Return:
             True if the ssh is successful and False otherwise
        """
        try:
            logger.info("Staring the SSH to host '%s'", self.remote_host)
            self.remote_conn_pre.connect(self.remote_host, username=username, key_filename=key_filename)
            self.remote_conn = self.remote_conn_pre.invoke_shell()
            self.remote_conn.settimeout(None)
            time.sleep(1)
            output = self.remote_conn.recv(1000)
            self.remote_conn.set_combine_stderr(True)
            return True
        except Exception:
            if raise_except:
                raise
            self.error = traceback.format_exc()
            return False

    def close(self):
        """
        To terminate the ssh
        """
        logger.debug("Closing the ssh")
        self.remote_conn_pre.close()

    def run_command(self, command, max_wait=0, check_prompt=False,
                    new_prompt=None, prompt_error=True, new_line=True):
        """
        To execute a command on the ssh host and save the command output
        which can later be retrieved

        Arguments:
             command - command to run
             max_wait - maximum wait for command to be completed. 0 for waiting forever
             wait_complete - flag denoting whether to wait for command to complete
             check_prompt - Flag denoting whether to check the command completion by
                       checking presence of the device prompt
             new_prompt - A new prompt to use instead of the default device prompt (this will only
                         used for this instance of the call only)
             prompt_error - Flag denotes whether to raise NoPromptError exception
                          when check_prompt is True and no prompt is present within the
                          given max_wait period
             new_line - flag indicating that a new lines should be appended to the command string
                        unless it already ends with a new line
        Raise:
            NoPromptError - if prompt is not present while 'check_prompt' and 'prompt_error' are true

        Return:
             Exit code of the command (-1 if not waited for completion).
             A special exit code None will indicate command timeout (exceeded the max_wait)
        """
        if self.remote_conn is None:
            raise SessionError("Session is not active")
        if new_line:
            command = command.rstrip('\n') + '\n'
        self.cum_ouput = ""
        exit_status = -1
        t1 = time.time()
        current_prompt = self.prompt if new_prompt is None else new_prompt

        def _collect_output():
            self.cum_ouput_locked = True
            while self.remote_conn.recv_ready():
                last_read = self.remote_conn.recv(5000)
                try:
                    self.last_read = last_read.decode('utf-8')
                except UnicodeDecodeError:
                    self.last_read = str(last_read)
                if self.collect_output:
                   self.cum_ouput += self.last_read
            self.cum_ouput_locked = False

        def _check_command_done(exit_status):
            done = False
            if self.remote_conn.exit_status_ready():
                done = True
                exit_status = self.remote_conn.recv_exit_status()
                logger.debug("Command '%s' on '%s' exited with exit code: %s", command.strip(), self.remote_host, exit_status)
            elif max_wait > 0 and time.time() - t1 > max_wait:
                done = True
                logger.error("Command '%s' timed-out", command.strip())
                if check_prompt and prompt_error:
                    raise NoPromptError("No device prompt '(%s)' is present within the timeout %s "
                                        "following the command '%s'" % (current_prompt, max_wait, command))
                exit_status = None
            elif check_prompt and len(self.last_read) > 0:
                done = re.compile(current_prompt).search(self.last_read) is not None

            if not self.remote_conn.closed:
               _collect_output()

            return done, exit_status

        # clear any outstanding output
        self.last_read = ""
        if not self.remote_conn.closed:
           _collect_output()

        logger.debug("Sending the command '%s'", command.strip())
        self.remote_conn.send(command)

        while True:
            done, exit_status = _check_command_done(exit_status)
            if done:
                break
            time.sleep(1)

        self.last_exit_code = exit_status
        return exit_status

    def read_output(self, all=True):
        """
        To read the output from last command as it updated. That is this can
        be repeatedly called to retrieve the newly added (not the cumulative)
        output as the command progresses.

        Arguments:
            all - flag denotes whether to read the entire output or just the recently
                  collected output (last few lines)
                  NOTE: When this is set to True, the saved output will be cleared after the read
                       hence, subsequent call will return the newly collected output only
        """
        current = self.last_read
        if all:
            while self.cum_ouput_locked:
                time.sleep(0.5)
                pass
            if len(self.cum_ouput) > 0:
                current = self.cum_ouput
                self.cum_ouput = ""
        return current


class RiftRootSession(Session):
    """
    Specialized class of Session class used for starting ssh session as root.
    (Equivalent to SSH with 'ssh_root' command)
    """
    src_key_filename = '/usr/rift/bin/id_grunt'
    key_filename = os.path.join(os.environ.get('HOME'), '.ssh/id_grunt')

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.prompt = "#"

    def connect(self, raise_except=True):
        """
        To start a new root_ssh session

        Return:
             True if the ssh is successful and False otherwise
        """
        src_key_filename = RiftRootSession.src_key_filename
        key_filename = RiftRootSession.key_filename
        if not os.path.exists(key_filename):
           shutil.copy(src_key_filename, key_filename)
           os.chmod(key_filename, 666)
        return super().connect('root', key_filename, raise_except=raise_except)

    def read_output(self, all=True, trim=True):
        """
        To read the output from last command as it updated. That is this can
        be repeatedly called to retrieve the newly added (not the cumulative)
        output as the command progresses.

        Arguments:
            all - flag denotes whether to read the entire output or just the recently
                  collected output (last few lines)
                  NOTE: When this is set to True, the saved output will be cleared after the read
                       hence, subsequent call will return the newly collected output only
            trim - flag denotes whether to trim the first and last line of the output thus
                   removing the command line text and the prompt line or the last line
                   (This may be set to False if 'run_command()' is called with check_prompt False)
        """
        res = super().read_output(all)
        if trim:
            res = '\n'.join(res.splitlines()[1:-1])
        return res
