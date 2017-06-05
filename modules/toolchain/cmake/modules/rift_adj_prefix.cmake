# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

# cmake script for generating the gnome rpms
cmake_minimum_required(VERSION 2.8)

# the paths in .pc files point to the "prefix" value passed to the 
# configure script during the last build.
# adjust the value to point to the current build path

# the paths in .la files point to the "prefix" value passed to the 
# configure script during the last build.
# adjust the value to point to the current build path


file(GLOB files @PKG_INSTALL_PREFIX@/*.pc)
file(GLOB_RECURSE la_files @PKG_INSTALL_PREFIX@/*.la)
list(APPEND files ${la_files})

set(more_files 
  @PKG_INSTALL_PREFIX@/usr/share/gdb/auto-load/libgobject-2.0.so.0.3800.2-gdb.py
  @PKG_INSTALL_PREFIX@/usr/share/gdb/auto-load/libglib-2.0.so.0.3800.2-gdb.py
)

foreach(m ${more_files})
  if(EXISTS ${m})
    list(APPEND files ${m})
  endif()
endforeach(m)

foreach(f ${files})
  message("adjusting prefix on ${f}")
  message("command: sed -i -e s,/[a-zA-Z0-9./_-]*/usr/rift/usr,@CMAKE_INSTALL_PREFIX@/usr,g ${f}")
  execute_process(
    COMMAND
    sed -i -e s,/[a-zA-Z0-9./_-]*/usr/rift/usr,@CMAKE_INSTALL_PREFIX@/usr,g ${f}
    RESULT_VARIABLE result
    )
  if(result)
    message("Error: ${result}")
    #    message(FATAL_ERROR "Failed to update the prefix in pkg-config file ${f}")
  endif(result)
endforeach(f)
