#! /usr/bin/env python

###############################################################################
# Reservation System Nagios plugin for Available VMs
#	derived from Nagios plugin template @ https://gist.github.com/jm-welch/9009533
#
# Notes
# ex. ./check_reservation_vms.py --critLarge=1 --warnLarge=2 --critSmall=3 --warnSmall=6
#
###############################################################################

__author__ = 'Nate Hudson'
__version__= 0.1

# the reservation REST API URL
REST_URL = "http://reservation.eng.riftio.com/testlab/API/"

from optparse import OptionParser, OptionGroup
import logging as log

# couldn't get NDL to import, let's just use requests and the REST API
import requests

## These will override any args passed to the script normally. Comment out after testing.
#testargs = '--help'
#testargs = '--version'
#testargs = '-vvv'

def main():
    #""" Main plugin logic goes here """

    ## Parse command-line arguments
    args, args2 = parse_args()

    ## Uncomment to test logging levels against verbosity settings
    # log.debug('debug message')
    # log.info('info message')
    # log.warning('warning message')
    # log.error('error message')
    # log.critical('critical message')
    # log.fatal('fatal message')

    log.info('args='+str(args))

    r = requests.get(REST_URL+'/available')

    if r.status_code != 200:
	gtfo(3,'Got a '+str(r.status_code)+' back, not a 200');

    log.debug("r.text="+r.text);

    num_large = r.text.count('large')
    num_small = r.text.count('small')

    perfData = '| num_small='+str(num_small)+' num_large='+str(num_large)+'';

    log.info('found '+str(num_large)+' large VMs and '+str(num_small)+' small VMs');

    # critical
    if (num_small < args.critSmall):
	gtfo(2,'Not enough Small VMs',perfData)
    if (num_large < args.critLarge):
	gtfo(2,'Not enough Large VMs',perfData)

    # warning
    if (num_small < args.warnSmall):
	gtfo(1,'Not enough Small VMs',perfData)
    if (num_large < args.warnLarge):
	gtfo(1,'Not enough Large VMs',perfData)

    gtfo(0,'minimum VMs are available',perfData)


def parse_args():
    #""" Parse command-line arguments """

    parser = OptionParser(usage='usage: %prog [-v|vv|vvv] [options]',
                          version='{0}: v.{1} by {2}'.format('%prog', __version__, __author__))

    ## Verbosity (want this first, so it's right after --help and --version)
    parser.add_option('-v', help='Set verbosity level',
                      action='count', default=0, dest='v')

    ## CLI arguments specific to this script
    #group = OptionGroup(parser,'Plugin Options')
    #group.add_option('-x', '--extra', help='Your option here',
    #                 default=None)

    ## Common CLI arguments
    parser.add_option('-c', '--critLarge', help='Set the critical threshold for Large VMs. Default: 1 ', default=1, type=float, dest='critLarge', metavar='##')
    parser.add_option('-w', '--warnLarge', help='Set the warning threshold for Large VMs. Default: 2 ',   default=2, type=float, dest='warnLarge', metavar='##')
    parser.add_option('-s', '--critSmall', help='Set the critical threshold for Small VMs. Default: 3 ', default=3, type=float, dest='critSmall', metavar='##')
    parser.add_option('-t', '--warnSmall', help='Set the warning threshold for Small VMs. Default: 6 ',   default=6, type=float, dest='warnSmall', metavar='##')

    #parser.add_option_group(group)

    ## Try to parse based on the testargs variable. If it doesn't exist, use args
    try:
        args, args2 = parser.parse_args(testargs.split())
    except NameError:
        args, args2 = parser.parse_args()

    ## Set the logging level based on the -v arg
    log.getLogger().setLevel([log.ERROR, log.WARN, log.INFO, log.DEBUG][args.v])

    log.debug('Parsed arguments: {0}'.format(args))
    log.debug('Other  arguments: {0}'.format(args2))

    return args, args2

def gtfo(exitcode, message='',perfdata=''):
    #""" Exit gracefully with exitcode and (optional) message """

    log.debug('Exiting with status {0}. Message: {1}'.format(exitcode, message))

    if exitcode == 3:
	message = "Unknown: "+message+' '+perfdata;
    if exitcode == 2:
	message = "Critical: "+message+' '+perfdata;
    if exitcode == 1:
	message = "Warning: "+message+' '+perfdata;
    if exitcode == 0:
	message = "OK: "+message+' '+perfdata;

    if message:
        print(message)
    exit(exitcode)

if __name__ == '__main__':
    ## Initialize logging before hitting main, in case we need extra debuggability
    log.basicConfig(level=log.DEBUG, format='%(asctime)s - %(funcName)s - %(levelname)s - %(message)s')
    main()
