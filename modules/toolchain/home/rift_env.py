#!/usr/bin/env python3

# STANDARD_RIFT_IO_COPYRIGHT


import contextlib
import concurrent.futures
import os
import re
import shlex
import socket
import subprocess
import sys


@contextlib.contextmanager
def pushd(path):
    """
    Simple context manager for changing directories.

    """
    current = os.getcwd()
    os.chdir(path)
    yield
    os.chdir(current)


def command(cmd):
    """
    Executes the specified command in a subprocess and returns the result.

    The result of the command is returned as a list of strings. Each string
    corresponds to a line of text (empty lines are ignored). If an error occurs
    during the execution of the command, a ValueError will be raised with the
    offending error information.

    """
    status, output = subprocess.getstatusoutput(cmd)
    
    if status != 0:
        raise ValueError(output)

    return [line for line in output.split() if line]


try:
    command('command -v git')
    command('git log')

    def list_git_submodules():
        """
        Returns a list of the submodules in the repository.

        """
        try:
            return command('git submodule -q foreach "echo \\$path"')
        except ValueError:
            return []


    def list_git_files():
        """
        Returns a list of files in the current git repository.

        """
        return command('git ls-files --exclude-standard -co')

except:
    print(' * NB: git not installed so using fallback functions (slower)')

    __submodule_pattern = re.compile('\[submodule "(.*)"\]')


    def list_git_submodules():
        modules = []
        root = os.environ['RIFT_ROOT']
        gitmodules = os.path.join(root, '.gitmodules')
        if not os.path.exists(gitmodules):
            return modules
        with open(gitmodules) as fd:
            for line in (line.strip() for line in fd.readlines() if line):
                result = __submodule_pattern.match(line)
                if result:
                    path = result.group(1)
                    if os.path.isdir(os.path.join(root, path)):
                        modules.append(path)

        return modules


    def list_git_files():
        return command('find . -type f')


def find_yang_dirs(top):
    """
    Returns a list of the directories containing YANG files.

    @param top - the directory to start searching from

    """
    with pushd(top):
        paths = set(os.path.dirname(f) for f in list_git_files() if f.endswith('.yang'))
        return sorted(os.path.join(top, p) for p in paths)


def find_vala_dirs(top):
    """
    Returns a list of VALA directories.

    @param top - the directory to start searching from

    """
    with pushd(top):
        paths = set(os.path.dirname(f) for f in list_git_files())
        return sorted(os.path.join(top, p) for p in paths if p.endswith('/vala'))


def make_build_path(submodules, build, path):
    """
    Returns a list of remapped build paths.

    The specified path must originate from within a submodule or a ValueError
    will be raised.

    @param submodules   - a list of the submodules in the repositiory
    @param path         - the path to map to the build area.

    @return a build path

    """
    for module in submodules:
        if path.startswith(module):
            target='_'.join(module.split('/')[1:])
            build_prefix = '{module}/src/{target}-build'.format(
                module=module,
                target=target)

            # If the submodule was an absolute path, then we added a standalone
            # submodule path.  In this case, there is no build prefix.
            if os.path.isabs(module):
                build_prefix = ""

            return os.path.join(build, build_prefix, path.replace(module, "")[1:])

    raise ValueError('ERROR: Unable to find the submodule of "{}"'.format(path))


def prepend_to_path_variable(path, values):
    """
    Prepends a value or list of values to a path variable.

    The path variable is assumed to be a colon delimited string of paths. The
    paths contained in the list of values will be prepended (in order) to the
    string of paths if they are not already contained in the path.

    @param path     - a colon delimited string of paths
    @param values   - a path or list of paths to prepend

    @return a colon delimited string of paths

    """
    path_list = path.split(':') if path else []
    if isinstance(values, list):
        valid_values = [v for v in values if v not in path_list]
    else:
        valid_values = [values]

    return ':'.join(valid_values + path_list)


def append_to_path_variable(path, values):
    """
    Appends a value or list of values to a path variable.

    The path variable is assumed to be a colon delimited string of paths. The
    paths contained in the list of values will be appended (in order) to the
    string of paths if they are not already contained in the path.

    @param path     - a colon delimited string of paths
    @param values   - a path or list of paths to prepend

    @return a colon delimited string of paths

    """
    path_list = path.split(':') if path else []
    if isinstance(values, list):
        valid_values = [v for v in values if v not in path_list]
    else:
        valid_values = [values]

    return ':'.join(path_list + valid_values)


def current_ip():
    """
    Returns the IP of the current machine.

    An empty string is returned if the IP cannot be determined.

    """
    ip = ""
    try:
        ip = socket.gethostbyname(socket.gethostname())
    except Exception as e:
        print(' * WARNING: unable to determine the public IP')

    return ip


def main(argv=sys.argv[1:]):
    try:
        root = os.environ['RIFT_ROOT']
    except KeyError:
        sys.stderr.write("ERROR: RIFT_ROOT has not been set")
        sys.stderr.flush()
        sys.exit(1)

    build = os.environ['RIFT_BUILD']
    install = os.environ['RIFT_INSTALL']
    artifacts = os.environ['RIFT_ARTIFACTS']
    platform = os.environ['RIFT_PLATFORM']

    os.chdir(root)

    print(" * Setting up base environment")

    # Create the base RIFT environment
    env = dict()
    env['RIFT_ENV'] = '1'
    env['RIFT_IP'] = current_ip()
    env['RIFT_BUILD'] = build
    env['RIFT_INSTALL'] = install
    env['RIFT_ARTIFACTS'] = artifacts
    env['RIFT_UNIT_TEST'] = os.path.join(env['RIFT_ARTIFACTS'], 'unittest')
    env['RIFT_MODULE_TEST'] = os.path.join(env['RIFT_ARTIFACTS'], 'moduletest')
    env['RIFT_SYSTEM_TEST'] = os.path.join(env['RIFT_ARTIFACTS'], 'systemtest')
    env['RIFT_COVERAGE'] = os.path.join(env['RIFT_ARTIFACTS'], 'coverage')
    env['RIFT_TEST_RESULTS'] = os.path.join(env['RIFT_ARTIFACTS'], 'results')
    env['RIFT_PLATFORM'] = platform
    env['RIFT_BUILD_CACHE_DIR'] = os.path.join("/net/jenkins/localdisk/jenkins/RIFT_BCACHE", platform)

    # ZSH Module path for RIFT
    # see https://github.com/RIFTIO/zsh/commit/651dcd5bfb192877c153e3451baafc62aeae4bc5)
    env['RIFT_MODULE_PATH'] = os.path.join(env["RIFT_INSTALL"], "usr/lib/zsh/5.0.7")

    # Create derivative RIFT environment
    env['RIFT_MODULE_TEST'] = env['RIFT_MODULE_TEST']
    env['UNIT_TEST_PATH'] = env['RIFT_UNIT_TEST']
    env['COVERAGE_PATH'] = env['RIFT_COVERAGE']
    env['INSTALLDIR'] = env['RIFT_INSTALL']
    env['LUA_PATH'] = '"%s"' % ';'.join([
        os.path.join(env['RIFT_INSTALL'], 'usr/share/lua/5.1/?.lua'),
        os.path.join(env['RIFT_INSTALL'], "usr/share/lua/5.1/?/init.lua"),
        '/usr/share/lua/5.1/?.lua',
        "/usr/share/lua/5.1/?/init.lua",
        ])
    env['LUA_CPATH'] = '"%s"' % ';'.join([
        os.path.join(env['RIFT_INSTALL'], "usr/lib/lua/5.1/?.so"),
        "/usr/lib64/lua/5.1/?.so",
        ])
    env['PYTHON_INSTALL_DIR'] = os.path.join(env['RIFT_INSTALL'], "usr/lib/python2.7/site-packages")
    env['RIFT_PYTHON_INSTALL_DIR'] = os.path.join(env['PYTHON_INSTALL_DIR'], "rift")
    env['PLUGINDIR'] = os.path.join(env['RIFT_INSTALL'], 'usr/lib/rift/plugins')
    env['RIFT_XSD_PATH'] = os.path.join(env['RIFT_INSTALL'], 'usr/data/xsd')
    env['PEAS_PLUGIN_LOADERS_DIR'] = '/usr/lib64/libpeas-1.0/loaders'
    if platform == "ub16":
        # When the PEAS_PLUGIN_LOADERS_DIR env var is unset, libpeas uses the platform LIBDIR
        # to find the libpes loader directory
        # https://github.com/gregier/libpeas/blob/master/libpeas/peas-dirs.c#L160
        # see peas_dirs_get_plugin_loader_dir()
        del env['PEAS_PLUGIN_LOADERS_DIR']

    env['XDG_DATA_DIRS'] = prepend_to_path_variable(
            os.environ.get('XDG_DATA_DIRS', '/usr/share'),
            os.path.join(env['RIFT_INSTALL'], 'usr/share'))

    env['SEAGULL_BIN_PATH'] = os.path.join(env['RIFT_INSTALL'], 'usr/local/seagull-1.8.2/bin')

    # RIFT-2912: Disable g-ir-scanner home directory build cache.
    env['GI_SCANNER_DISABLE_CACHE'] = "1"

    env['PYTEST_ADDOPTS'] = '"-p no:cacheprovider -x -v"'

    env['PKG_CONFIG_PATH'] = '%s:%s' % (
        os.path.join(env['RIFT_INSTALL'], 'usr/lib/pkgconfig'),
        os.path.join(env['RIFT_INSTALL'], 'usr/lib64/pkgconfig'))

    env['YUMA_MODPATH'] = '/usr/share/yuma/modules/netconfcentral'

    def prepend_install_paths(name, paths):
        original = env[name] if name in env else os.environ.get(name, '')
        env[name] = prepend_to_path_variable(
                original,
                [os.path.join(env['RIFT_INSTALL'], p) for p in paths])

    def append_root_paths(name, paths):
        original = env[name] if name in env else os.environ.get(name, '')
        env[name] = append_to_path_variable(
                original,
                [os.path.join(root, p) for p in paths])

    prepend_install_paths('PATH', [
        'bin',
        'sbin',
        'usr/bin',
        'usr/sbin',
        'usr/local/bin',
        'usr/local/sbin'])

    append_root_paths('PATH', [
        'git/rift-submodule-tools'])

    prepend_install_paths('LD_LIBRARY_PATH', [
        'usr/lib64',
        'usr/lib',
        'usr/lib/rift/plugins',
        'usr/local/confd/lib',
        env['SEAGULL_BIN_PATH']
        ])

    prepend_install_paths('GI_TYPELIB_PATH', [
        #'usr/lib/girepository-1.0',
        'usr/lib/rift/girepository-1.0',
        'usr/lib/rift/plugins',
        ])

    prepend_install_paths('YUMA_MODPATH', [
        'usr/data/yang',
        'usr/share/yuma/modules/ietf',
        'usr/local/confd/src/confd/yang',
        ])

    prepend_install_paths('PYTHONPATH', [
        'usr/local/lib/python2.7/site-packages',
        'usr/lib/python2.7/site-packages',
        'usr/lib64/python2.7/site-packages',
        'usr/data/proto',
        'usr/lib/python2.7/site-packages',
        'usr/local/lib/python2.7/site-packages',
        ])

    # Get a list of the submodules
    submodules = list_git_submodules()

    standalone_submodule_root = os.environ.get('RIFT_STANDALONE_SUBMODULE_ROOT', '')
    if standalone_submodule_root:
        submodules.append(standalone_submodule_root)

    # Iterate over all of the submodules and construct a list of the directories
    # that contain YANG files
    print(' * Finding YANG directories...')
    yang_dirs = []
    with concurrent.futures.ProcessPoolExecutor() as executor:
        results = executor.map(find_yang_dirs, submodules)
        _ = [yang_dirs.extend(r) for r in results if r]

    # Map the list of YANG directories to their build directory counterparts
    yang_build_dirs = [make_build_path(submodules, build, d) for d in yang_dirs]

    # Make YANG paths absolute
    yang_dirs = [os.path.join(root, d) for d in yang_dirs]

    # Iterate over all of the submodules and construct a list of the VALA directories
    print(' * Finding VALA directories...')
    vala_dirs = []
    with concurrent.futures.ProcessPoolExecutor() as executor:
        results = executor.map(find_vala_dirs, submodules)
        _ = [vala_dirs.extend(r) for r in results if r]

    # Map the list of YANG directories to their build directory counterparts
    vala_build_dirs = [make_build_path(submodules, build, d) for d in vala_dirs]

    env['GI_TYPELIB_PATH'] = append_to_path_variable(
            env['GI_TYPELIB_PATH'],
            vala_build_dirs)

    env['GI_TYPELIB_PATH'] = append_to_path_variable(
            env['GI_TYPELIB_PATH'],
            yang_build_dirs)

    env['PLUGINDIR'] = append_to_path_variable(
            env['PLUGINDIR'],
            vala_build_dirs)

    env['YUMA_MODPATH'] = append_to_path_variable(
            env['YUMA_MODPATH'],
            yang_dirs)

    # write the environment to .rift-env
    if not os.path.isdir(build):
        os.makedirs(build, exist_ok=True)

    with open(os.path.join(build, '.rift-env'), 'w') as fd:
        for key in env:
            val = env[key].replace(root, '${RIFT_ROOT}')
            fd.write('export {key}={val}\n'.format(key=key, val=val))


if __name__ == "__main__":
    main()
