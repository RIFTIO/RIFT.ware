# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 04/22/2016
# 

rift_py3test(utest_uploader
  TEST_ARGS
  ${CMAKE_CURRENT_SOURCE_DIR}/utest_uploader.py
  )

rift_py3test(utest_serializer
  TEST_ARGS
  ${CMAKE_CURRENT_SOURCE_DIR}/utest_serializer.py
  )

rift_py3test(utest_export
  TEST_ARGS
  ${CMAKE_CURRENT_SOURCE_DIR}/utest_export.py
  )

rift_py3test(utest_onboard
  TEST_ARGS
  ${CMAKE_CURRENT_SOURCE_DIR}/utest_onboard.py
  )

rift_py3test(utest_image_upload.py
  TEST_ARGS
  ${CMAKE_CURRENT_SOURCE_DIR}/utest_image_upload.py
  )

rift_py3test(utest_package.py
  TEST_ARGS
  ${CMAKE_CURRENT_SOURCE_DIR}/utest_package.py
  )
