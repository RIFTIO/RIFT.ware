
# STANDARD_RIFT_IO_COPYRIGHT

import abc
import collections
import inspect
import os
import tempfile
import subprocess
import weakref

class UnlockedError(Exception):
    pass

def start_perf(pids, frequency=100, perf_data='/tmp/rwperf.data'):
    """
    This function does the 'perf record'
    """

    pids_string = ','.join(pids)

    args = []
    uid=None
    if uid is not None:
        args.extend([
            'sudo',
            '--non-interactive',
            '--user', uid])
    else:
        args.extend([
            'sudo',
            '--non-interactive'])

    args.extend([
            '/usr/bin/perf', 'record',
            '-g', '-F', str(frequency),
            '-p', pids_string,
            '-o', perf_data,
    ])
    _log = tempfile.NamedTemporaryFile(
        prefix="rwperf.record.log",
        delete=False,
        dir='/tmp')
    os.fchmod(_log.fileno(), 0o666)

    proc = subprocess.Popen(
        args,
        close_fds=True,
        stdout=_log,
        stderr=_log,
        cwd='/tmp')

    return (' '.join(args), proc.pid)

def stop_perf(newpid):
    """
    This function does the 'kill' of the 'perf record'
    """

    command = "sudo kill " + str(newpid)
    #print(command)

    _log = tempfile.NamedTemporaryFile(
        prefix="rwperf.kill.log",
        delete=False,
        dir='/tmp')
    os.fchmod(_log.fileno(), 0o666)

    args = []
    uid=None
    if uid is not None:
        args.extend([
            'sudo',
            '--non-interactive',
            '--user', uid])
    else:
        args.extend([
            'sudo',
            '--non-interactive'])

    args.extend([
            '/usr/bin/kill', str(newpid),
    ])

    proc = subprocess.Popen(
        args,
        close_fds=True,
        stdout=_log,
        stderr=_log,
        cwd='/tmp')

    return ' '.join(args)

def report_perf(percent_limit=1.0, perf_data='/tmp/rwperf.data'):
    """
    This function does the 'perf report'
    """

    """
    args = []
    uid=None
    if uid is not None:
        args.extend([
            'sudo',
            '--non-interactive',
            '--user', uid])
    else:
        args.extend([
            'sudo',
            '--non-interactive'])

    args.extend([
            '/usr/bin/perf', 'report',
            '--sort', 'comm,dso',
            '--percent-limit', str(percent_limit),
            '--stdio',
            '-IUi', perf_data,
    ])
    """

    command = "sudo perf report --sort comm,dso --percent-limit " + str(percent_limit) + " --stdio -IUi " + perf_data
    #print(command)
    stream = os.popen(command)
    #print (stream)
    content = stream.read()
    return (command, content)

    _log = tempfile.NamedTemporaryFile(
        prefix="rwperf.report.log",
        delete=False,
        dir='/tmp')
    os.fchmod(_log.fileno(), 0o666)

    return subprocess.check_output(
        args,
        close_fds=True,
        stderr=_log,
        cwd='/tmp')

