# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 8/26/2013
# 

# Recurse into the subdirectories
# This does not actually cause another cmake executable to run
# The same process will walk through the project's entire directory structure
set(subdirs vala test) 
rift_add_subdirs(SUBDIR_LIST ${subdirs})
