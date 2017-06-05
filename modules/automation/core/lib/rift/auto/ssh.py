#!/usr/bin/env python3
# STANDARD_RIFT_IO_COPYRIGHT #

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

class MaxAttemptsExceededException(Exception):
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


class SshSession(object):
    BUF_SIZE = 64 * 1024

    def __init__(self, remote_host):
        self.remote_host = remote_host
        self.remote_conn = paramiko.SSHClient()
        self.remote_conn.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.sftp = ''

    def connect(self, username=None, password=None, private_key_file=None, port=22, max_attempts=4):
        """Establishes SSH session to the remote host with given credentials.
        """
        number_of_attempts = 1
        while number_of_attempts <= max_attempts:
            logger.info('Establishing ssh session to host {0} with {1} attempt number:{2}'.format(self.remote_host, username, number_of_attempts))
            try:
                self.remote_conn.connect(self.remote_host, username=username, password=password,
                                         key_filename=private_key_file, port=port)
                return True
            except paramiko.AuthenticationException as err:
                logger.error("Authentication failed. Error details: {}".format(str(err)))
            except paramiko.SSHException as err:
                logger.error("Error in connecting or establishing ssh session to {0}. Error details: {1}".format(
                    self.remote_host, str(err)))
            except Exception as err:
                logger.error('Not able to connect to {0}. Error details: {1}'.format(self.remote_host, str(err)))
            time.sleep(30)
            number_of_attempts += 1
        #If it ever comes out the while loop, it means number_of_attempts has exceeded the limit and that should raise an exception.
        raise MaxAttemptsExceededException("Attempts to connect failed.")

    def close(self):
        try:
            self.remote_conn.close()
        except Exception:
            pass

    def run_command(self, command, max_wait=300, get_pty=False):
        """Executes command over a new channel in the existing SSH session.
        Enable get_pty for commands to be run in a tty e.g commands starting with sudo.
        It accepts timeout value through 'max_wait' argument and its default value is 300 seconds.

        Returns:
            A tuple carrying exit code of the command execution and the command output (including standard error
             in case of any error in command execution).
            exit code None will indicate command timeout (exceeded the max_wait)
        """
        self.output = ''
        self.err = ''
        self.exit_code = None
        timeout_flag = True

        if not self.remote_conn.get_transport():
            logger.error('SSH connection to {} not open. Try establishing ssh session using connect method'.format(
                self.remote_host))
            return 1, '', ''

        if not get_pty:
            if 'sudo ' in command:
                get_pty = True
        logger.info("{}   -  {}".format(self.remote_host, command))
        _, out, err = self.remote_conn.exec_command(command, timeout=max_wait, get_pty=get_pty)

        def _read_stdout():
            if out.channel.recv_ready():
                tmp_ = out.channel.recv(self.BUF_SIZE)
                try:
                    tmp_ = str(tmp_, 'utf-8')
                except Exception:
                    tmp_ = str(tmp_)
                logger.info(tmp_)
                self.output += tmp_
                # recv(<nBytes>) will block if no data available to read; always call recv_ready() before calling recv()
                # socket.timeout error will come after timeout secs (if timeout is set)

        def _read_stderr():
            if out.channel.recv_stderr_ready():
                tmp_ = out.channel.recv_stderr(self.BUF_SIZE)
                try:
                    tmp_ = str(tmp_, 'utf-8')
                except:
                    tmp_ = str(tmp_)
                logger.info(tmp_)
                self.err += tmp_

        start_time = time.time()
        while time.time() - start_time < max_wait:
            _read_stdout()
            _read_stderr()
            if out.channel.exit_status_ready():
                self.exit_code = out.channel.exit_status
                timeout_flag = False
                break
            time.sleep(1)

        if timeout_flag:
            logger.error('TIMEOUT in execution of {}'.format(command))
            # return None, '', ''
            # Implement if the running instance of the command needs to be killed because of timeout
            # self.output.close()

        # Read any remaining bytes from both stdout, stderr of the executed command
        _read_stdout()
        _read_stderr()

        return self.exit_code, self.output, self.err

    def get(self, remotepath, localpath):
        """To get a file from remote machine using existing ssh session"""
        logger.info("Getting file {} from {}".format(remotepath, self.remote_host))
        try:
            if not self.sftp:
                self.sftp = self.remote_conn.open_sftp()
            self.sftp.get(remotepath, localpath)
            if os.path.exists(localpath):
                return True
            return False
        except Exception as ex:
            logger.error("Failed to get remote file. Error: {}".format(ex))
            return False

    def put(self, localpath, remotepath):
        """To transfer a file to remote machine using existing ssh session"""
        logger.info("Putting file {} to {}".format(localpath, self.remote_host))
        try:
            if not self.sftp:
                self.sftp = self.remote_conn.open_sftp()
            self.sftp.put(localpath, remotepath)
            return True
        except Exception as ex:
            logger.error("Failed to copy local file to remote machine. Error: {}".format(ex))
            return False
