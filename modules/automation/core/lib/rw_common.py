
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

'''
Description:    Misc constants, functions, classes for common use in test automation code.
Author:         Andrew Golovanov
Version:        1.0
Creation Date:  4/29/2014
Copyright:      RIFTio, Inc.
'''

#--- EXTERNAL MODULES

import os, sys, re, signal, time, datetime, socket, getpass, traceback, pprint, subprocess as sbpr
from flask._compat import reraise
import gevent, gevent.queue, gevent.monkey
import pexpect as pe

#--- CONSTANTS


def os_write(buf, s):
    try:
        os.write(buf, s)
    except Exception:
        os.write(buf, s.encode('utf-8'))

TMFMT          = '%Y%m%d.%H%M%S'    # Standard timeout template
AGE_UNITS      = 'weeks', 'days', 'hours', 'minutes', 'seconds'
DBGMODE        = '-DEBUG' in sys.argv

SCRNAMEFULL    = os.path.realpath(sys.argv[0])     # Absolute pathname of the main script
SCRNAMEBASE    = os.path.splitext(os.path.basename(SCRNAMEFULL))[0]     # extension removed filename of the main script
CURRDIR        = os.environ['PWD']                 # Current directory
CURUSER        = getpass.getuser()                 # Current username
MYPID          = os.getpid()                       # Current process ID
HOSTNAME       = socket.gethostname()              # Local host name
try:
    HOSTIP     = socket.gethostbyname(HOSTNAME)    # Local host IP
except:
    print("error retrieving IP address for host \"%s\" using gethostbyname()"  % HOSTNAME)
    raise
HOME           = os.environ['HOME']                # User home directory

reCLASS        = re.compile("^<class '([^']+)'>$")

#--- VARIABLES

# RIFT_ROOT
try:
    RIFT_ROOT = os.environ['RIFT_ROOT']
except:
    file_name = os.path.realpath(__file__).split(os.sep)[:-1]
    while len(file_name) > 1:
        RIFT_ROOT = os.sep.join(file_name)
        if os.path.exists(os.path.join(RIFT_ROOT, '.gitmodules')):
            break
        file_name.pop()
    else:
        raise SystemExit('Unable to discover RIFT_ROOT from the path: {}'.format(__file__))

#--- CLASSES

class ddict(dict):
    """Namespace for the data"""
    def __getattr__(self, member):
        return self.get(member, None)
    def __str__(self):
        if not self.keys(): return ''
        k = sorted([x for x in self.keys() if x[0] != '_'])
        f = '{{:{}}} : {{}}'.format(max(map(len, self.keys())))
        return '\n'+'\n'.join([f.format(x, self[x]) for x in k])
    def usrdata(self):
        return sorted([x for x in self.keys() if x[0] != '_'])
    def usrmeth(self):
        return sorted([x for x in list(set(dir(self)) - set(dir({})))
                       if x[0] != '_' if callable(getattr(self, x))])
    __setattr__ = dict.__setitem__
    __delattr__ = dict.__delitem__
    def __repr__(self):
        ind = 3 * ' '
        s   = ['Object of type "{}" (id: {})'.format(reCLASS.search(str(type(self))).group(1), id(self))]
        ud  = self.usrdata()
        f   = '{{}}{{:{}}} : {{}}'.format(max(map(len, ud)))
        fl  = True
        usm = '{}User methods: {}'
        s.append(usm.format(ind, ', '.join(self.usrmeth())))
        usd = '{}User data: {}'
        s.append(usd.format(ind, ', '.join(ud)))
        for d in ud:
            mObj = reCLASS.search(str(type(self[d])))
            if mObj and mObj.group(1).split('.')[-1][0] != '_':
                if fl:
                    s.append('{}Sub-objects:'.format(ind))
                    fl = not fl
                if isinstance(self[d], ddict):
                    s.append(f.format(2*ind, d, repr(self[d]).replace('\n', '\n'+3*ind)))
                else:
                    udt = sorted([x for x in dir(self[d]) if x[0] != '_' if not callable(getattr(self[d], x))])
                    umt = sorted([x for x in dir(self[d]) if x[0] != '_' if callable(getattr(self[d], x))])
                    s.append(f.format(2*ind, d, (
                                mObj.group(1) + '\n' +
                                usm.format(ind, ', '.join(umt)) + '\n' +
                                '{}User data:    {}'.format(ind, ', '.join(udt))).replace('\n', '\n'+3*ind)))
        return '\n'.join(s)

class Cfg(ddict):
    """
    Parses config files and stores configuration data in a namespace based on the dictionary class.
    """

    def __init__ (self, cfgFile=None, cfgSect=None, enforceTypes=True, printDocExp=False):
        """
        Arguments:

          cfgFile      - a pathname or a list of pathnames of the config files (may be empty)
                         if None, a default config file is presumed to be located in the same directory
                         as the main script with having the name of that directory + .conf extension;
                         if an empty string, no config file is used, only options from the command line;
                         '-CFGFILE' value from the command line (if specified) overrides any value
                         of this argument including the case when there is no value for this parameter
                         was specified in the command line argument (this prevents using config file)
          cfgSect      - a name or a list of names of the target sections; if an empty list or a string,
                         then there will be no target sections considered for parsing (ie. only [default]
                         sections will be processed); if None, a default section name will be used
                         which is the name of the script being executed but without a file extension;
                         '-CFGSECT' value from the command line (if specified) will override a value
                         of this argument
          enforceTypes - if True will try to convert string values into scalar types whenever possible
          printDocExp  - if True, the documentation and expected-results will be included into string
                         representation of this object returned by the __str__ method
        """

        self._printDocExp = printDocExp

        #--- Command line args

        cla = dict ([(x.partition(' ')[0], x.partition(' ')[2]) for x in re.split (
                    ' -+', ' %s' % ' '.join (sys.argv[1:]))[1:]])

        #--- Detect a list of config file(s) and a list of target sections

        mainScript = os.path.realpath (sys.argv [0])
        if 'CFGFILE' in cla:
            cfgFile = cla['CFGFILE'].split()
        elif cfgFile is None:
            dirName = os.path.dirname(mainScript)
            cfgFile = ['{0}/{1}.conf'.format(dirName, os.path.basename(dirName))]
        elif type(cfgFile) != list:
            cfgFile = [cfgFile]
        if 'CFGSECT' in cla:
            cfgSect = cla['CFGSECT'].replace(',', ' ').split()
        elif cfgSect is None:
            cfgSect = [os.path.splitext(os.path.basename(mainScript))[0]]
        elif type(cfgSect) != list:
            cfgSect = [cfgSect]
        self._cfgFile = cfgFile
        self._cfgSect = cfgSect

        if not len(cfgFile): return self.update(cla)    # No config file will be used

        reCOMMENT    = re.compile ('(?m)#.*$')
        reBLLINE     = re.compile ('(?m)^\s*\n\r?')
        reSECTS      = re.compile ('(?m)^\s*\[([^\]]+)\]\s*\n\r?')
        rePARAMSL    = re.compile ('(?m)^\s*([.\w]+)\s*=[ \t\r]*($|[^{ \t\r\n].*$)')
        rePARAMML    = re.compile ('(?ms)^\s*([.\w]+)\s*=[ \t\r]*\{\s+(.*?)\n\r?[ \t\r]*\}[ \t\r]*$')
        reSUBST      = re.compile ('\$\{([.\w]+)\}')
        reINT        = re.compile ('^\d+$')
        reFLT        = re.compile ('^\d+\.\d+$')

        # Parse all config files
        d2 = dict()
        for fn in cfgFile:

            if not fn: continue
            if not os.path.exists(fn): raise Exception('Non existing config file: {}'.format(fn))
            with open(fn) as fo:
                f = reSECTS.split(reBLLINE.sub('', reCOMMENT.sub('', fo.read())))
                if not f[0].strip(): del f[0]
                try:    f = dict(zip(f[::2], f[1::2]))
                except: raise Exception('Error while parsing sections in the config file "{0}"'.format(fn))
                for sn in f:
                    if sn not in d2: d2[sn] = dict()
                    d2[sn].update([(x[0], x[1].rstrip()) for x in rePARAMSL.findall(f[sn])])
                    d2[sn].update([(x[0], '\n'.join([y.strip() for y in x[1].rstrip().split('\n')]))
                                   for x in rePARAMML.findall(f[sn])])

        # Plane off sections
        d1 = dict()
        if 'default' in d2: d1.update(d2['default'])
        for sn in cfgSect:
            if sn in d2: d1.update(d2[sn])
        d1.update(cla)

        # Substitutions
        while True:
            toSubLst = [x for x in d1 if d1[x] if reSUBST.search(d1[x])]
            if not len(toSubLst): break
            for toSub in toSubLst:
                for fromSub in [x for x in reSUBST.findall(d1[toSub])]:
                    s = fromSub.split('.')
                    if len(s) > 2:
                        raise Exception('Invalid syntax of reference "${{{0}}}" in the parameter "{1}"'.format(fromSub, toSub))
                    try:
                        if len(s) < 2: subVal = d1[fromSub]
                        else:          subVal = d2[s[0]][s[1]]
                    except:
                        raise Exception('Unable to resolve "${{{0}}}" in the parameter "{1}"'.format(fromSub, toSub))
                    d1[toSub] = d1[toSub].replace('${{{0}}}'.format(fromSub), subVal)

        # Coercion & regexp
        for p in d1:
            if   p[-4:] == '.exp':
                if d1[p][:3] == 'RE:': d1[p] = re.compile(d1[p][3:].strip())
            elif enforceTypes and p[-4:] != '.doc':
                if   reINT.search(d1[p]): d1[p] = int  (d1[p])
                elif reFLT.search(d1[p]): d1[p] = float(d1[p])
                elif d1[p] in ('True', 'False'):
                    d1[p] = True if d1[p] == 'True' else False
                elif d1[p] == 'None':
                    d1[p] = None

        # Finalize
        self.update(d1)

    def getAllParams(self):
        """Returns sorted list of all config parameters (no doc strings or expected results)"""
        return sorted([x for x in self.keys() if x[-4:] != '.doc' if x[-4:] != '.exp'])

    def getDocStr(self, param):
        """Returns doc string (or None) for a specified parameter name (no .doc extension)"""
        return getattr(self, '{}.doc'.format(param))

    def getExpRes(self, param, compiled=True):
        """
        Returns expected result (or None) for a specified parameter name (no .exp extension).
        For regular expressions, the 'compiled' argument specifies if regexp
        should be returned as a compiled object (True) or a source string (False).
        """
        v = getattr(self, '{}.exp'.format(param))
        return v if type(v) == str or compiled else v.pattern

    def __str__(self):
        ind = '  '
        res = ['--- CONFIGURATION OBJECT: ---']
        for p in self.getAllParams():
            res.append('{}:'.format(p))
            res.append('{}Value: {}'.format(ind, str(getattr(self, p)).replace('\n', '\n{}     '.format(2*ind))))
            if self._printDocExp:
                d = self.getDocStr(p)
                res.append('{}Doc:   {}'.format(ind, '-' if d is None else d))
                d = self.getExpRes(p)
                res.append('{}Exp:   {}'.format(ind, '-' if d is None else d))
        res = '\n{}'.format(ind).join(res)
        if 'STOP_AFTER_CFG' in self:
            raise SystemExit(res)
        return res

class Log(object):
    """
    Supports logging from both test scripts and SUT to:
       - stdout (such as xterm)  --> main test log only
       - a log file
       - optional GUI
    """

    def __init__(self, logRoot=None, logAge=3, ageUnit='days', stdout=True, guiCall=None,
                debug=DBGMODE, dirSuff=None, grp_name=None):
        """
        Initializes logging object. Supports logging for all sessions.

        Arguments:

        logRoot  - root directory for test logging
                   default: ~/test_logs/
        logAge   - integer number of ageUnit after which old logs have to be wiped out
                   default: 3
        ageUnit  - supported values are: 'weeks', 'days', 'hours', 'minutes', 'seconds'
                   default: days
        stdout   - if False, the main test log will NOT be printed to stdout (useful in cron)
                   default: turn on logging on stdout
        guiCall  - callback procedure for the test logging into the GUI
                   defailt: no logging to GUI
        debug    - if True, all debug messages will be printed, otherwise skipped
                   default: depends on the command line argument '-DEBUG'
        dirSuff  - optional suffix that will be added to the name of log directory (separated by '.')
                   default: no additional suffix
        grp_name - an optional group name that has to be set for the logging directory path
                   (must be a valid group name that exists in the OS); note that for security
                   reasons this setting allowed only on the bottom-up dirs in the logging directory
                   path that are owned by the current user (owner of this process)
        """

        self._tmStmp  = time.strftime(TMFMT)
        self._myUid   = os.geteuid()
        if logRoot is None:
            logRoot = os.path.join(HOME, 'test_logs')
        self.logRoot  = os.path.realpath(os.path.expanduser(logRoot))
        self.logDir   = os.path.join(self.logRoot, '{}.{}{}'.format(self._tmStmp, SCRNAMEBASE,
            '.'.join(('', str(dirSuff))) if dirSuff else ''))
        if not os.path.exists (self.logDir):
            try:
                sbpr.call('mkdir -p {}'.format(self.logDir), shell=True)
                if isinstance(grp_name, str):
                    path   = self.logDir
                    while True:
                        if os.stat(path).st_uid != self._myUid:
                            break
                        if sbpr.call('chgrp {} {}'.format(grp_name, path), shell=True):
                            break
                        path = os.path.split(path)[0]
                    #os.makedirs(self.logDir, 0777)
            except Exception as e:
                raise Exception('Failed to create new logging directory: {}: {}'.format(self.logDir, str(e)))
        if not os.access(self.logDir, os.W_OK):
            raise Exception('No write permissions for the directory: {}'.format(self.logDir))
        self.stdout   = stdout
        self.debug    = debug
        self.logAge   = datetime.timedelta(**{ageUnit:int(logAge)})
        self.bLine    = 80*'-'
        self.out      = sys.stdout.fileno()

        # All active logs as tuples: (open log file object, GUI callback)
        self.allLogs  = {'main' : (open(os.path.join(self.logDir, '.'.join((SCRNAMEBASE, 'txt'))), 'w'), guiCall)}

        # Start logging
        gevent.signal(signal.SIGINT, self._stopLog)
        self.gq  = gevent.queue.Queue()
        self.glt = gevent.Greenlet.spawn(self._gqWriter)
        gevent.sleep(0.1)

        # Print logging info
        self.logd(self)

        # Start cleaning
        self._glt = gevent.Greenlet.spawn(self._cleanLog)
        gevent.sleep(0)

    def log(self, msg, debug=False, surround=False, _stime=0):
        """
        Arguments:

        msg
        debug    - if True, print as a debug message (unless self.debug=False)
        surround - if True, surround the message with the break lines (mostly used in test steps)
        """
        if debug:
            if not self.debug: return
            dbgMsg = 'DEBUG|'
        else:
            dbgMsg = ''
        ts = '{}|{} '.format(time.strftime ('%X'), dbgMsg)
        tn = '\n'.join(('', ts))
        s  = [''.join((ts, str(msg).replace('\n', tn)))]
        if surround:
            tb = [''.join((ts, self.bLine))]
            s  = tb + s + tb
        s.append('')
        s = '\n'.join(s)
        if self.stdout: os_write(self.out, s)
        self.gq.put_nowait(('main', s))
        gevent.sleep(_stime)
        return ts

    def logd(self, msg):
        """Print debug message"""
        if not self.debug: return
        self.log(msg, debug=True)

    def logp(self, msg, debug=False, surround=False):
        """Log a message and then make a pause. Works via stdin"""
        ts = self.log(msg, debug=debug, surround=surround)
        sys.stdout.write('{}\n{} Press "Enter" to continue...'.format(ts, ts))
        sys.stdout.flush()
        sys.stdin.readline()

    def _stopLog(self):
        for log in self.allLogs:
            try:
                fd = self.allLogs[log][0]
                os.fsync(fd)
                os.close(fd)
            except: pass

    def _addLog(self, logId, guiCall):
        f = open(os.path.join(self.logDir, logId), 'a')  # 'a' is to handle SUT reboots
        self.allLogs[logId] = (f, guiCall)

    def _gqWriter(self):
        while True:
            while not self.gq.empty():
                logId, logData = self.gq.get()
                if logId in self.allLogs:
                    logFd, guiCall = self.allLogs[logId]
                    if callable(guiCall): guiCall(logData)
                    if logFd:             os_write(logFd.fileno(), logData)
            gevent.sleep(0)

    def _cleanLog(self):
        now  = datetime.datetime.now ()
        mObj = re.compile('^\d{8}\.\d{6}')
        dLst = ((x, datetime.datetime.strptime (mObj.search(x).group(), TMFMT))
                for x in os.listdir (self.logRoot) if mObj.search(x))
        for fn, dt in list (dLst):
            fn = os.path.join(self.logRoot, fn)
            try:
                if os.stat(fn).st_uid != self._myUid: continue
            except:
                pass
            if (now - dt) <= self.logAge: continue
            try:    os.system ('rm -rf {}'.format(fn))
            except: pass

    def __str__(self):
        fs  = lambda x: '[closed]' if x.closed else '[open]  '
        ind = ' '*4
        s   = ['--- LOGGING OBJECT: ---']
        s.append('{}{:20} : {}'.format(ind, 'Logging directory', self.logDir))
        s.append('{}{:20} : {} {}'.format(ind, 'Main log file',  fs(self.allLogs['main'][0]),
                                        self.allLogs['main'][0].name))
        s.append('{}{:20} : {}'.format(ind, 'Output to stdout',  self.stdout))
        s.append('{}{:20} : {}'.format(ind, 'Debug output',      self.debug))
        s.append('{}{:20} : {}'.format(ind, 'Max log age',       self.logAge))
        s.append('{}{:20} : {}'.format(ind, 'Logging thread',    'stopped' if self.glt.dead else 'running'))
        s.append('{}{:20} : {}'.format(ind, 'Logging queue',     self.gq.qsize()))
        for logId in self.allLogs:
            if logId == 'main': continue
            s.append('{}Session "{:10}" : {} {}'.format(ind, logId, fs(self.allLogs[logId][0]),
                                     self.allLogs[logId][0].name))
        return '\n'.join(s)

#--- MISC FUNCTIONS

def hExcept (e=None):
    """
    Exception handler, returns a tuple of four elements:
    type, description, last stack entry and formatted message
    based on the last stack entry. If there is no exception,
    it returns None.
    """

    excType, excDescr, excTrace = e if e else sys.exc_info()
    errStack = traceback.extract_tb(excTrace)
    if not errStack: return None
    errStack = errStack [-1]
    finMsg  =   '{:<15} : {}'                .format('Description', str(excDescr).replace('\n', '\n{:<15} : '.format('')))
    finMsg += '\n{:<15} : file: {}, line: {}'.format('Place',       errStack[0], errStack[1])
    finMsg += '\n{:<15} : {}'                .format('Type',        str(excType))
    return (excType, excDescr, errStack, finMsg)

def dspObj(obj, hdr=None, stop=False, subObj=False, parentObj=None, ignPriv=True):
    '''
    Prints out methods and data members an object.
    Arguments:
        hdr       - an optional message (string) that will be added to the header
        stop      - if True, stop at the end of this function
        subObj    - if False, do not display sub-objects, show one level only
        parentObj - for internal use only (do not change this argument)
        ignPriv   - if True, do not display "private" members
    '''
    meth = ['this object: {}'.format(type(obj))]
    data = list()
    sbcl = list()
    mlen = 25
    for x in dir(obj):
        if ignPriv and x[0] == '_': continue  # ignore underscore names
        try:    v = getattr(obj, x)
        except: v = '** N/A (failed to retrieve a value for this member) **'
        if callable(v): meth.append(x)
        elif isinstance(v, dict):
            data.append((x+' :',dict()))
            data.extend([('\t{:<30}  '.format(p), q) for p,q in v.iteritems()])
        elif isinstance(v, list):
            data.append((x+' :',list()))
            data.extend([('\t'+str(p), '') for p in v])
        else:
            mlen = max(mlen, len(x))
            data.append((x,v))
            if subObj is True and 'class' in str(type(v)):
                sbcl.append(v)

    me = str(obj)
    if not me: me = type(obj)
    try:
      print('\n====== OBJECT{}: str={}, type={} {}\n'.format(
            '' if hdr is None else ' "{}"'.format(hdr),
            str(obj), type(obj),
            '' if parentObj is None else '--> IS CHILD OF {}'.format(parentObj)))
    except: pass
    fstr1  = '{{:<{}}}'.format(mlen)
    fstr2  = '{{:<{}}} {{:<40}}  {{}}'.format(mlen)
    print('Methods:\n\t{}\n'.format('\n\t'.join([fstr1.format(x) for x in meth])))
    print('Data Members:\n\t{}\n'.format(
        '\n\t'.join([fstr2.format(x, type(v), v.encode('utf-8') if isinstance(v, unicode) else str(v)) for x,v in data])))
    if sbcl:
        print('\n>>>>> SUBOBJECTS OF "{}" >>>>>'.format(me))
        for x in sbcl: dspObj(x, stop=False, parentObj=me)
        print('<<<<< END OF SUBOBJECTS OF "{}" <<<<<\n'.format(me))
    if stop: sys.exit(100)

def get_fd_limits():
    '''
    Returns a 2-tuple with the current max (soft, hard) limits of the open file
    descriptors for this process.
    '''
    try:
        return tuple(int(x) for x in sbpr.check_output(
            'ulimit -Sn;ulimit -Hn', shell=True).strip().split())
    except Exception as e:
        raise Exception('[get_fd_limits] Failed to retrieve open fd limits: {}'.format(str(e)))
