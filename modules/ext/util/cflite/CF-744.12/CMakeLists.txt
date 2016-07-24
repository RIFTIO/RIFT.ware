cmake_minimum_required(VERSION 2.8.8)
project(CFLite)

set (CFLiteVersion 744.12)
set (CMAKE_VERBOSE_MAKEFILE YES)

include("TestBigEndian")
include("CheckFunctionExists")
include("cmake/utilities.cmake")

check_function_exists("strncat" "string.h" HAVE_STRLCAT)
if (HAVE_STRLCAT)
  add_definitions(-DHAVE_STRLCAT=1)
endif ()
check_function_exists("strncpy" "string.h" HAVE_STRLCPY)
if (HAVE_STRLCPY)
  add_definitions(-DHAVE_STRLCPY=1)
endif ()

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Debug
       CACHE STRING "Choose the type of build: Debug Release."
       FORCE)
endif ()
message("* Current build type is : ${CMAKE_BUILD_TYPE}")


set (warning_flags
  -Wall
  -Wextra
  -Wno-unused-parameter
  -Wno-sign-compare
  -Wno-missing-field-initializers
  -Wno-trigraphs
)

set (defines
  -D_GNU_SOURCE=1
  -DCF_BUILDING_CF=1
  -DDEPLOYMENT_TARGET_LINUX=1
  -DVERSION=${CFLiteVersion}
)

set (cflags 
  -fblocks
  -std=gnu99
  -fexceptions
  -include CoreFoundation_Prefix.h
)

set (linker_flags
  "--no-undefined"
  -pthread
  "-Wl,--build-id"
)

set (cf_lib_names
  BlocksRuntime dispatch dl icudata icui18n icuuc m rt uuid
)
set (cf_lib_paths "")

foreach (lib IN LISTS cf_lib_names)
  find_library(${lib}_path NAMES ${lib})
  if (NOT ${lib}_path)
    message (FATAL_ERROR "Could not find library ${lib}")
  endif ()

  list (APPEND cf_lib_paths ${${lib}_path})
endforeach ()


set (cf_sources
  CoreFoundation/CFApplicationPreferences.c
  CoreFoundation/CFArray.c
  CoreFoundation/CFBag.c
  CoreFoundation/CFBase.c
  CoreFoundation/CFBasicHash.c
  CoreFoundation/CFBigNumber.c
  CoreFoundation/CFBinaryHeap.c
  CoreFoundation/CFBinaryPList.c
  CoreFoundation/CFBitVector.c
  CoreFoundation/CFBuiltinConverters.c
  CoreFoundation/CFBurstTrie.c
  CoreFoundation/CFCalendar.c
  CoreFoundation/CFCharacterSet.c
  CoreFoundation/CFData.c
  CoreFoundation/CFDate.c
  CoreFoundation/CFDateFormatter.c
  CoreFoundation/CFDictionary.c
  CoreFoundation/CFError.c
  CoreFoundation/CFFileUtilities.c
  CoreFoundation/CFICUConverters.c
  CoreFoundation/CFLocale.c
  CoreFoundation/CFLocaleIdentifier.c
  CoreFoundation/CFLocaleKeys.c
  CoreFoundation/CFNumber.c
  CoreFoundation/CFNumberFormatter.c
  CoreFoundation/CFOldStylePList.c
  CoreFoundation/CFPlatform.c
  CoreFoundation/CFPlatformConverters.c
  CoreFoundation/CFPreferences.c
  CoreFoundation/CFPropertyList.c
  CoreFoundation/CFRunLoop.c
  CoreFoundation/CFRuntime.c
  CoreFoundation/CFSet.c
  CoreFoundation/CFSortFunctions.c
  CoreFoundation/CFStorage.c
  CoreFoundation/CFString.c
  CoreFoundation/CFStringEncodingConverter.c
  CoreFoundation/CFStringEncodingDatabase.c
  CoreFoundation/CFStringEncodings.c
  CoreFoundation/CFStringScanner.c
  CoreFoundation/CFStringUtilities.c
  CoreFoundation/CFSystemDirectories.c
  CoreFoundation/CFTimeZone.c
  CoreFoundation/CFTree.c
  CoreFoundation/CFUniChar.c
  CoreFoundation/CFUnicodeDecomposition.c
  CoreFoundation/CFUnicodePrecomposition.c
  CoreFoundation/CFURL.c
  CoreFoundation/CFURLAccess.c
  CoreFoundation/CFUtilities.c
  CoreFoundation/CFUUID.c
  CoreFoundation/CFVersion.c
  CoreFoundation/CFXMLPreferencesDomain.c

  # Added for RiftWare
  CoreFoundation/CFStream.c
  CoreFoundation/CFSocket.c
  CoreFoundation/CFSocketStream.c
)

set (public_headers
  CoreFoundation/CFArray.h
  CoreFoundation/CFBag.h
  CoreFoundation/CFBase.h
  CoreFoundation/CFBinaryHeap.h
  CoreFoundation/CFBitVector.h
  CoreFoundation/CFByteOrder.h
  CoreFoundation/CFCalendar.h
  CoreFoundation/CFCharacterSet.h
  CoreFoundation/CFData.h
  CoreFoundation/CFDate.h
  CoreFoundation/CFDateFormatter.h
  CoreFoundation/CFDictionary.h
  CoreFoundation/CFError.h
  CoreFoundation/CFLocale.h
  CoreFoundation/CFNumber.h
  CoreFoundation/CFNumberFormatter.h
  # FIXME: CFPrefs doesn't work on Linux yet
  # CoreFoundation/CFPreferences.h
  CoreFoundation/CFPropertyList.h
  CoreFoundation/CFRunLoop.h
  CoreFoundation/CFSet.h
  CoreFoundation/CFString.h
  CoreFoundation/CFStringEncodingExt.h
  CoreFoundation/CFTimeZone.h
  CoreFoundation/CFTree.h
  CoreFoundation/CFURL.h
  CoreFoundation/CFURLAccess.h
  CoreFoundation/CFUUID.h
  CoreFoundation/CoreFoundation.h

  # Added for RiftWare
  CoreFoundation/CFSocket.h
  # Added for __CFGenericValidateType_
  CoreFoundation/CFRuntime.h
)

set (private_headers
  CoreFoundation/CFCharacterSetPriv.h
  CoreFoundation/CFError_Private.h
  CoreFoundation/CFLogUtilities.h
  CoreFoundation/CFPriv.h
  # Removed for exposing __CFGenericValidateType_
  # CoreFoundation/CFRuntime.h
  CoreFoundation/CFStorage.h
  CoreFoundation/CFStringDefaultEncoding.h
  CoreFoundation/CFStringEncodingConverter.h
  CoreFoundation/CFStringEncodingConverterExt.h
  CoreFoundation/CFUniChar.h
  CoreFoundation/CFUnicodeDecomposition.h
  CoreFoundation/CFUnicodePrecomposition.h
  CoreFoundation/ForFoundationOnly.h
)

set (embedded_resources
  "${CMAKE_CURRENT_BINARY_DIR}/Resources/CFCharacterSetBitmaps.o"
  "${CMAKE_CURRENT_BINARY_DIR}/Resources/CFUnicodeData.o"
  "${CMAKE_CURRENT_BINARY_DIR}/Resources/CFUniCharPropertyDatabase.o"
)

################################################################################

add_definitions(${cflags} ${warning_flags} ${defines})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(cflite_shared SHARED ${cf_sources} ${embedded_resources})
add_library(cflite_static STATIC ${cf_sources} ${embedded_resources})

add_dependencies(cflite_shared cflite_static)

foreach (target cflite_static cflite_shared)
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME CoreFoundation
  )
  set_target_properties(${target} PROPERTIES COMPILE_DEFINITIONS "DEBUG")
endforeach ()

target_link_libraries(cflite_shared ${cf_lib_paths} ${linker_flags})
  set_target_properties(cflite_shared PROPERTIES
    SOVERSION 1
    VERSION ${CFLiteVersion}
  set_target_properties(${target} PROPERTIES COMPILE_DEFINITIONS "DEBUG")
)

install(TARGETS cflite_shared cflite_static DESTINATION lib)
install(FILES ${public_headers} DESTINATION include/CoreFoundation)
install(FILES TargetConditionals.h DESTINATION include)

################################################################################
# Embedded data
################################################################################
ConvertBinaryFileToObjectFile(
  "Resources/CFCharacterSetBitmaps.bitmap"
  "${CMAKE_CURRENT_BINARY_DIR}/Resources/CFCharacterSetBitmaps.o"
  "__CFCharacterSetBitmaps"
)

ConvertBinaryFileToObjectFile(
  "Resources/CFUniCharPropertyDatabase.data"
  "${CMAKE_CURRENT_BINARY_DIR}/Resources/CFUniCharPropertyDatabase.o"
  "__CFUniCharPropertyDatabase"
)

test_big_endian(is_big_endian)
if (is_big_endian)
  set (unicode_data_file "Resources/CFUnicodeData-B.mapping")
else ()
  set (unicode_data_file "Resources/CFUnicodeData-L.mapping")
endif ()

ConvertBinaryFileToObjectFile(
  "${unicode_data_file}"
  "${CMAKE_CURRENT_BINARY_DIR}/Resources/CFUnicodeData.o"
  "__CFUnicodeDataMapping"
)


