#!/usr/bin/env python
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Joshua Downer
# Creation Date: 2014/07/17

import argparse
import compileall
import concurrent.futures
import contextlib
import cStringIO
import functools
import os
import re
import shlex
import subprocess
import sys

class CommandError(Exception):
    pass


@contextlib.contextmanager
def redirect_stdout(buf):
    """A context manager that switches stdout for a buffer"""
    tmp, sys.stdout = sys.stdout, buf
    yield sys.stdout
    sys.stdout = tmp


@contextlib.contextmanager
def pushd(path):
    """A context manager that acts like pushd one enter and popd on exit

    Using this context manager will change the current working directory to the
    specified path. On exit, the context manager will change back to the
    original directory.

    """
    cwd = os.getcwd()
    os.chdir(path)
    yield
    os.chdir(cwd)


def command(cmd):
    """Executes a command in a separate process and returned the output

    The command is executed on a process and the output from the command is
    returned as a list of strings. Note that empty strings are not returned.

    """
    process = subprocess.Popen(shlex.split(cmd),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

    output, error = process.communicate()
    if process.returncode != 0:
        raise CommandError(error)

    return [s for s in output.split('\n') if s]


def top_level():
    """Returns the path of the top level directory of the repository."""
    git_dir = command("git rev-parse --git-dir")[0]
    git_dir = re.match('.*(?=\.git/?)', git_dir).group().rstrip('/')
    return git_dir if git_dir else '.'


def list_submodules():
    """Returns a list of the submodules in the current repository."""
    return command("git submodule -q foreach 'echo $path'")


def list_remote():
    """Returns a list of files that differ from the remote master."""
    return command('git diff remotes/origin/master --name-only')


def list_untracked():
    """Returns a list of untracked files."""
    return [f[3:] for f in command('git st --porcelain') if f.startswith('?? ')]


def list_added():
    """Returns a list of added files."""
    def added(path):
        try:
            return 'A' in path[:2]
        except:
            return False

    return [f[3:] for f in command('git st --porcelain') if added(f)]


def list_modified():
    """Returns a list of modified files."""
    def modified(path):
        try:
            return 'M' in path[:2]
        except:
            return False

    return [f[3:] for f in command('git st --porcelain') if modified(f)]


def list_range(commits):
    """Returns a list of files changed over the specified range of commits"""
    try:
        return command('git diff {commits} --name-only'.format(commits=commits))
    except CommandError:
        pass
    return []


def list_submodule(func, path):
    """Applies a function from within a submodule and returns the result."""
    with pushd(path):
        return [os.path.join(path, f) for f in func()]


class Repository(object):
    def __init__(self, root):
        """Create an object to represent the repository

        :root: the root of the repository

        """
        self._root = os.path.abspath(root)
        with pushd(self.root):
            self._submodules = list_submodules()

    @property
    def root(self):
        """The path to the root of the repository"""
        return self._root

    @property
    def submodules(self):
        """A list of submodules in the repository"""
        return self._submodules

    def foreach_submodule(self, func):
        """Applies a function to each of the submodules in the repository.

        :func: a function that returns a list of file paths

        The result of the provided function is required to be a list of paths to
        files within each of the submodules. The paths are relative to the root
        of the repository.

        """
        with concurrent.futures.ProcessPoolExecutor() as executor:
            paths = [os.path.join(self.root, s) for s in self.submodules]
            results = executor.map(func, paths)
            results = [u for v in results for u in v]
            return [os.path.relpath(f, self.root) for f in results]

    def forall(self, func):
        """Applies a function to the submodules and the top-level repo.

        :func: a function that returns a list of file paths

        The result of the provided function is required to be a list of paths to
        files within each of the submodules or the top-level repo. The paths are
        relative to the root of the repository.

        """
        files = []
        with pushd(self.root):
            files = [f for f in func() if os.path.isfile(f)]

        list_submodule_func = functools.partial(list_submodule, func)
        files.extend(self.foreach_submodule(list_submodule_func))

        return sorted(files)

    def remote(self):
        """Returns a list of files that differ from the remote/origin/master."""
        return self.forall(list_remote)

    def modified(self):
        """Returns a list of files have been modified in the repo and submodules"""
        return self.forall(list_modified)

    def untracked(self):
        """Returns a list of all the untracked files in the repo and submodules"""
        return self.forall(list_untracked)

    def range(self, commits):
        """Returns a list of files modified over the specified range"""
        return self.forall(functools.partial(list_range, commits))


class Lint(object):
    command = "pylint -E --rcfile={rcfile}"

    def __init__(self, exclude=None, rcfile=None):
        """
        Create a lint object.

        :exclude: a list of regular expressions used to exclude files
        :rcfile: a path to a pylintrc file

        """
        self._exclude = exclude
        self._command = Lint.command.format(rcfile=rcfile)

    def should_exclude(self, path):
        """Returns TRUE if this specified path should be excluded

        :path: the path to test

        """
        if not path.endswith('.py'):
            return True

        if not os.path.isfile(path):
            return True

        try:
            for rule in self._exclude:
                if rule.match(path) is not None:
                    return True
        except:
            pass

        return False

    def evaluate(self, path):
        """Applies pylint to the specified file.

        Pylint will only be applied to python scripts that have a '.py' suffix
        and that do not match any excluded paths.

        :path: the path of the file that is to be evaluated

        """
        if self.should_exclude(path):
            return []

        results = command('{cmd} {path}'.format(cmd=self._command, path=path))
        return [line for line in results if not line.startswith("***")]


def compile_target(target):
    """Generate bytecode for the specified target

    The :target: is a python script that get compiled into byte code.

    Returns a tuple (result, details), where result is a string that with one of
    the values: SKIPPED, SUCCESS, or FAILURE. The details provide information
    about any failure that has occurred.

    If there is already bytecode in the same directory as the :target:, the
    :target: is not compile and returns a result of 'SKIPPED'. If there is a
    syntax error in the script, 'FAILURE' is returned with the details of the
    compilation error. Otherwise, 'SUCCESS' is returned.

    """
    result = 'SUCCESS'
    details = []

    # The output to stdout is redirected to a buffer so that it can
    # be optionally reported in the case of a failure.
    with redirect_stdout(cStringIO.StringIO()):
        bytecode = target + 'c'
        if os.path.isfile(bytecode):
            return target, 'SKIPPED', details

        if compileall.compile_file(target, quiet=True):
            os.remove(bytecode)
        else:
            result = 'FAILURE'
            sys.stdout.seek(0)
            details = [line.rstrip() for line in sys.stdout.readlines() if line]

    return target, result, details

    # If there are any error messages, write them to stdout at this
    # time and then exit. Or, in verbose mode, write out a
    # success/failure mode for each file.
    if verbose:
        print('{result} {file}'.format(
            result='FAILURE' if failure else 'SUCCESS', file=target))
    else:
        for line in failure:
            print(line.rstrip())

    return target, result, details


def main(argv=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--compile',
            action='store_true',
            help="compile the python scripts to detect syntax errors")
    parser.add_argument('-e', '--exclude',
            help="a list of rules for excluding file paths")
    parser.add_argument('-l', '--list',
            action='store_true',
            help="list the files to be processed")
    parser.add_argument('-m', '--modified',
            action='store_true',
            help="list files that have been modified")
    parser.add_argument('-r', '--remote',
            action='store_true',
            help="list files that differ from the remote")
    parser.add_argument('-t', '--target',
            default=None,
            type=str,
            help="a directory to search (recursively) for python files")
    parser.add_argument('--range',
            help="list files that have changed over a specified range \
                    of commits (as understood by git)")
    parser.add_argument('-u', '--untracked',
            action='store_true',
            help="list files that are untracked")
    parser.add_argument('--rcfile',
            default=os.path.join(top_level(), 'etc/pylintrc'),
            help="specifies the path to a pylintrc file to use")
    parser.add_argument('-v', '--verbose',
            action='store_true',
            help="print out additional diagnostic information")
    parser.add_argument('files',
            nargs=argparse.REMAINDER,
            default=[],
            help="a list of additional files to process")

    args = parser.parse_args(argv)

    # If pylint is required, check that it is available
    if not args.compile:
        try:
            command('command -v pylint')
        except CommandError:
            print('Unable to find pylint on the PATH')
            exit(1)
        except Exception as e:
            print(str(e))
            exit(2)

    repo = Repository(top_level())

    # Construct the lint object using any rules provided by the caller
    exclude = args.exclude.split(":") if args.exclude else []
    lint = Lint(rcfile=args.rcfile, exclude=[re.compile(e) for e in exclude])

    # Construct a list of the required files
    files = args.files
    if args.modified:
        files.extend(repo.modified())
    if args.untracked:
        files.extend(repo.untracked())
    if args.remote:
        files.extend(repo.remote())
    if args.range:
        files.extend(repo.range(args.range))

    # If a target directory has been specified, recursively search for python
    # files
    if args.target is not None:
        if not os.path.isdir(args.target):
            print("The specified target directory does not exist!")
            exit(1)

        for root, _, names in os.walk(args.target):
            files.extend(os.path.join(root, n) for n in names if n.endswith('.py'))

    # Simply print out the paths of all of the files
    if args.list:
        for f in files:
            print(f)

    # Compile each of the specified files to determine if there are any syntax
    # errors.
    elif args.compile:
        files = [f for f in files if f.endswith('.py')]
        with pushd(repo.root):
            with concurrent.futures.ProcessPoolExecutor() as executor:
                futures = [executor.submit(compile_target, f) for f in files]
                concurrent.futures.wait(futures)

            results = [f.result() for f in futures]

            if args.verbose:
                failed = False
                for target, result, details in results:
                    failed = (failed or result == 'FAILURE')
                    print('PYCHECK {result} {target}'.format(result=result, target=target))
                    for line in details:
                        print(line)

                if failed:
                    exit(1)

            else:
                failures = [target for target, result, _ in results if result == 'FAILURE']
                for target in failures:
                    print('PYCHECK FAILURE {target}'.format(target=target))

                if failures:
                    exit(1)

    # Apply pylint to each of the files and report the result
    else:
        with pushd(repo.root):
            for f in files:
                for line in lint.evaluate(f):
                    print(line)

if __name__ == "__main__":
    main()
