# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

# Recurse into the subdirectories
# This does not actually cause another cmake executable to run
# The same process will walk through the project's entire directory structure
set(subdirs vala yang)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

