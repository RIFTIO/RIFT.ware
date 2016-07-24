# Creation Date: 2016/1/12
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)


rift_python_install_tree(
  FILES
    rift/openmano/__init__.py
    rift/openmano/rift2openmano.py
    rift/openmano/openmano_client.py
  COMPONENT ${PKG_LONG_NAME}
  PYTHON3_ONLY)
