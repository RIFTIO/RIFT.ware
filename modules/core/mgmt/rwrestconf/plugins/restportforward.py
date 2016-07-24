# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Max Beckett
# Creation Date: 8/29/2015
# 


# Workaround RIFT-6485 - rpmbuild defaults to python2 for
# anything not in a site-packages directory so we have to
# install the plugin implementation in site-packages and then
# import it from the actual plugin.

try:
   import rift.tasklets.restportforward
except Exception as e:
   raise e
class Tasklet(rift.tasklets.restportforward.RestPortForwardTasklet):
   pass
