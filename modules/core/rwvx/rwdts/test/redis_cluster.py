#!/usr/bin/python

# STANDARD_RIFT_IO_COPYRIGHT

#author: Vijay Nag
#description: Script for running redis clusters in Rift
#gtest environment
#date: 18/09/2014

import os
import signal
import sys
import subprocess
import string
import getopt
import shutil
import datetime

class RedisCluster(object):
  def __init__(self, startPort, dir, cleanup, numNodes, gemDir, daemonize = False):
    self.mypid=os.getpid()
    self.numNodes = numNodes
    self.startPort = startPort
    self.dir=os.path.abspath(dir)
    self.cleanup = cleanup
    self.kids=[]
    self.gemDir=gemDir
    self.runAsDaemon=daemonize
    self.config = string.Template("\
    port $PORT\n\
    cluster-enabled yes\n\
    cluster-config-file $NODENUM.conf\n\
    cluster-node-timeout 5000\n\
    timeout 0\n\
    appendonly yes\n")

    if (False == os.path.isfile("/usr/bin/redis-server")):
      print "Required {0} executable not found".format("/usr/bin/redis-server")
      #sys.exit(2)
      os.kill(os.getpid(), signal.SIGKILL)
    if (False == os.path.isfile(self.dir+"/redis-trib.rb")):
      print "Required {0} ruby script not found".format(self.dir+"/redis-trib.rb")
      #sys.exit(2)
      os.kill(os.getpid(), signal.SIGKILL)
    os.environ["GEM_PATH"]=gemDir + ":" + gemDir +"gems"

  def createClusterConfig(self):
    config_dict = {}
    for val in range(self.numNodes):
      config_dict["PORT"] = self.startPort + val
      config_dict["NODENUM"] = val
      cmd = "mkdir -p node-redis." + str(val)
      os.system(cmd)
      fname = "node-redis." + str(val) + "/redis.conf"
      f = open(fname, 'w')
      f.write(self.config.substitute(config_dict))
      f.close()

  def runSingleInstance(self):
    signal.signal(signal.SIGCHLD, self.chldClean)
    signal.signal(signal.SIGINT,  self.chldClean)
    pid = os.fork()
    if pid == 0:
      arg=["/usr/bin/redis-server", "--port "+str(self.startPort)]
      os.execv('/usr/bin/redis-server', arg)
      sys.exit(0)
    else:
      self.kids.append(pid)

  def startRedisServers(self):
    signal.signal(signal.SIGCHLD, self.chldClean)
    signal.signal(signal.SIGINT,  self.chldClean)
    signal.signal(signal.SIGTERM,  self.chldClean)
    signal.signal(signal.SIGABRT,  self.chldClean)
    for val in range(self.numNodes):
      pid = os.fork()
      if pid == 0:
        arg=['/usr/bin/redis-server', 'redis.conf']
        dirname = "./node-redis." + str(val)
        os.chdir(dirname)
        os.execv('/usr/bin/redis-server', arg)
        sys.exit(0)
      else:
        self.kids.append(pid)

  def chldClean(self, SIG, FRM):
      if self.kids == []:
        sys.exit(0)
      try:
          pid, status, rusage = os.wait3(os.WNOHANG)
      except OSError:
          sys.exit(0)
      if pid not in self.kids and SIG == signal.SIGCHLD:
        pass
      else:
        signal.signal(signal.SIGCHLD, signal.SIG_DFL)
        while self.kids:
          pids = self.kids.pop(0)
          print "Sending SIGKILL to process: pid=",pids
          try:
            os.kill(pids, signal.SIGTERM)
          except:
            pass
        sys.exit(0)

  def cleanUp(self):
    if self.cleanup:
      for val in range(numNodes):
        try:
          shutil.rmtree("node-redis." + str(val))
        except:
          pass

  def formCluster(self):
    ip_string="127.0.0.1"
    port=0
    cmd="create "
    for val in range(self.numNodes):
      cmd += ip_string + ":"+ str(self.startPort+port) + " "
      port = port + 1
    import time
    time.sleep(3) #damn, sleep for 3 seconds hoping all guys are up!
    ret = subprocess.call(self.dir+"/redis-trib.rb " + cmd, shell=True)

  def run(self):
    if (self.numNodes > 1):
       self.createClusterConfig()
       self.startRedisServers()
       self.formCluster()
    else:
      self.runSingleInstance()

  def daemonize(self):
    """
      This method forks a watcher process and the actual worker process.
      Watcher process runs in same pgid as that of the caller and worker process
      is placed in a group of its own. This way worker process group do not respond
      to the signals emanating from the controlling terminal of the watcher process.
      A pipe is setup between the watcher(write-end) and worker(read-end). The worker
      thread after spawning redis-servers does a forever blocking read on the read end of
      the pipe. Return from read(2) basically means the watcher died(hence the caller)
      and worker has to gracefully exit all the redis tasks
    """
    r,w = os.pipe()
    pid = os.fork()
    if (0 == pid):
      #worker process
      os.setpgid(0,0)
      os.close(w)
      self.run()
      r = os.fdopen(r)
      a = r.read() #watch on the pipe till the caller pgid dies
      print "[redis_cluster_reap]pipe shutdown, sending SIGKILL to process group: ", os.getpid()
      self.chldClean(signal.SIGINT, 0) #ok, caller pgid died and we need to clean up.
    else:
      """
      wait for signals from caller pgid and
      ignore sigint so that ctrl-c doesn't terminate us under gdb,
      we will anyways be terminated by SIGKILL by reaper
      """
      os.close(r)
      w = os.fdopen(w, 'w')
      signal.signal(signal.SIGINT, signal.SIG_IGN)
      now = datetime.datetime.now()
      while True:
        pass
        current = datetime.datetime.now()
        if (((current.second - now.second) + ((current.minute - now.minute) * 60)) > 60):
          break

  def start(self):
    if (self.runAsDaemon is True):
      self.daemonize()
    else:
      self.run()
      now = datetime.datetime.now()
      while True:
        pass
        current = datetime.datetime.now() 
        if (((current.second - now.second) + ((current.minute - now.minute) * 60)) > 60):
          break
           
  def __del__(self):
    self.cleanUp()

def usage():
  print "python redis_cluster.py"
  print "Allowed options"
  print "-h [--help]                Display this message"
  print "-s [--daemon]              Detach redis-server from current process group"
  print "-p [--start-port]          Listening ports for redis-servers"
  print "-d [--bin-dir]             Directory where redis executables are available. This parameter overrides RIFT_INSTALL environmental variable"
  print "-c [--clean]               Clean up directories and files created by this script"
  print "-n [--num-nodes]           Number of nodes in redis-cluster"
  print "-g [--gem-dir]             Directory where redis ruby gems are available. This parameter overrides GEM_PATH environmental variable"

def ParseCmdLine(argv):
  startPort = 0
  dir=""
  cleanup=False
  numNodes = 1  #not a cluster
  gemDir = ""
  daemonize=False
  try:
    opts, args = getopt.getopt(argv, 'n:p:d:g:hcs', ['start-port=', 'bin-dir=', 'num-nodes=', 'gem-dir=', 'clean','help', 'daemon'])
  except getopt.GetoptError, err:
    print str(err)
    usage()
    #sys.exit(2)
    os.kill(os.getpid(), signal.SIGKILL)
  for opt, arg in opts:
      if opt in ('-h', '--help'):
          usage()
          sys.exit(2)
      elif opt in ('-p', '--start-port'):
          startPort = int(arg)
      elif opt in ('-d', '--bin-dir'):
          dir=arg
      elif opt in ('-c', '--clean'):
          cleanup = True
      elif opt in ('-n', '--num-nodes'):
          numNodes = int(arg)
      elif opt in ('-g', '--gem-dir'):
          gemDir = arg
      elif opt in ('-s', '--daemon'):
          daemonize = True
      else:
          usage()
          #sys.exit(2)
          os.kill(os.getpid(), signal.SIGKILL)
  if (0 == startPort):
    print "[-p --startPort] mandatory parameter"
    usage()
    #sys.exit(2)
    os.kill(os.getpid(), signal.SIGKILL)
  elif ("" == dir):
    try:
      dir = os.environ["RIFT_INSTALL"] + "/usr/bin/"
      gemDir = dir + "/redis-gems"
    except:
      print "environ[RIFT_INSTALL] not set and dir parameter is empty"
      usage()
      #sys.exit(2)
      os.kill(os.getpid(), signal.SIGKILL)
  return startPort, dir, cleanup, numNodes, gemDir, daemonize

def main(startPort, dir, cleanup, numNodes, gemDir, daemonize):
  redisCluster = RedisCluster(startPort, dir, cleanup, numNodes, gemDir, daemonize)
  redisCluster.start()

if __name__=="__main__":
  startPort, dir, cleanup, numNodes, gemDir, daemonize = ParseCmdLine(sys.argv[1:])
  main(startPort, dir, cleanup, numNodes, gemDir, daemonize)

