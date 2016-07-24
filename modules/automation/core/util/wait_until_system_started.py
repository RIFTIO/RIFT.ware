#!/usr/bin/env python3
"""
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

@file wait_until_system_started.py
@author Karun Ganesharatnam (karun.ganesharatnam@riftio.com)
@date 07/09/2015
@summary: This script will wait until all components are running

"""

import argparse
import logging
import pdb
import sys
import time
import traceback

import rift.vcs.vcs
import rift.auto.session


logging.basicConfig(format='%(asctime)-15s %(levelname)s %(message)s')
logger = logging.getLogger()
logger.setLevel(logging.INFO)

parser = argparse.ArgumentParser()
parser.add_argument('--confd-host',
                    default="127.0.0.1",
                    help="Hostname or IP where the confd netconf server is running. ")

parser.add_argument('--pause-on-error',
                    action='store_true',
                    help="Flag for whether to drop the script into debugger on error (paused)")

parser.add_argument('--max-wait',
                    type=int,
                    default=1000,
                    help="Maximum wait in seconds")

args = parser.parse_args(sys.argv[1:])
start = time.time()

initial_wait = min(60, args.max_wait)
time.sleep(initial_wait)
time_elapsed = time.time() - start
time_remaining = args.max_wait - time_elapsed

try:
    session = rift.auto.session.RestconfSession(args.confd_host)
    time_elapsed = time.time() - start
    time_remaining = args.max_wait - time_elapsed
    rift.vcs.vcs.wait_until_system_started(session, timeout_secs=time_remaining)
    logger.info("All components started successfully")
except Exception:
  if args.pause_on_error:
    logger.error("Unhandled exception occurred during startup")
    logger.error(traceback.format_exc())
    logger.info("System is paused and accessible via other interfaces for debugging")
    pdb.set_trace()
  else:
    raise

