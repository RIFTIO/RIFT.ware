
import paramiko
import subprocess

from charmhelpers.core.hookenv import config


class NetNS(object):
    def __init__(self, name):
        pass

    @classmethod
    def create(cls, name):
        # @TODO: Need to check if namespace exists already
        try:
            ip('netns', 'add', name)
        except Exception as e:
            raise Exception('could not create net namespace: %s' % e)

        return cls(name)

    def up(self, iface, cidr):
        self.do('ip', 'link', 'set', 'dev', iface, 'up')
        self.do('ip', 'address', 'add', cidr, 'dev', iface)

    def add_iface(self, iface):
        ip('link', 'set', 'dev', iface, 'netns', self.name)

    def do(self, *cmd):
        ip(*['netns', 'exec', self.name] + cmd)


def ip(*args):
    return _run(['ip'] + list(args))


def _run(cmd, env=None):
    if isinstance(cmd, str):
        cmd = cmd.split() if ' ' in cmd else [cmd]

    cfg = config()
    if all(k in cfg for k in ['pass', 'vpe-router', 'user']):
        router = cfg['vpe-router']
        user = cfg['user']
        passwd = cfg['pass']

        if router and user and passwd:
            return ssh(cmd, router, user, passwd)

    p = subprocess.Popen(cmd,
                         env=env,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    retcode = p.poll()
    if retcode > 0:
        raise subprocess.CalledProcessError(returncode=retcode,
                                            cmd=cmd,
                                            output=stderr.decode("utf-8").strip())
    return (''.join(stdout), ''.join(stderr))


def ssh(cmd, host, user, password=None):
    ''' Suddenly this project needs to SSH to something. So we replicate what
        _run was doing with subprocess using the Paramiko library. This is
        temporary until this charm /is/ the VPE Router '''

    cmds = ' '.join(cmd)
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, port=22, username=user, password=password)

    stdin, stdout, stderr = client.exec_command(cmds)
    retcode = stdout.channel.recv_exit_status()
    client.close()  # @TODO re-use connections
    if retcode > 0:
        output = stderr.read().strip()
        raise subprocess.CalledProcessError(returncode=retcode, cmd=cmd,
                                            output=output)
    return (''.join(stdout), ''.join(stderr))
