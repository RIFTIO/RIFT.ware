# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

# This file contains cmake macros for generating documentation

include(CMakeParseArguments)

##
# This function adds the doxygen target.
# The target copies the doxgen template configuration
# and updates the OUTPUT_DIRECTORY, EXCLUDE_PATTERNS and INPUT
# in the doxygen template before invoking doxygen.
#
# If DOXY_FILES is provides it generates the documenatation for
# the specified files, otherwise this function adds all the files
# in sub directories recursively. Note that the DOXY_FILES should
# have files with paths relative to the current source directory.
#
# DOXY_EXCLUDE_PATTERNS can be used explictly omit somefiles
# from doxygen documentation.
#
# rift_add_doxygen_target(<TARGET>
#                         EXCLUDE_FROM_DOXYGEN_TARGETS
#                         DOXY_NAME <doxgen-header-name>
#                         DOXY_FILES <doxygen-files>
#                         DOXY_EXCLUDE_PATTERNS <doxygen-exclude-patterns>
#                         DEST_DIR <doc-destination>)
##
function(rift_add_doxygen_target TARGET)
  ##
  # Parse the function arguments
  ##
  set(parse_options EXCLUDE_FROM_DOXYGEN_TARGETS)
  set(parse_onevalargs DEST_DIR DOXY_NAME)
  set(parse_multivalueargs DOXY_FILES DOXY_EXCLUDE_PATTERNS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  if(NOT ARGS_EXCLUDE_FROM_DOXYGEN_TARGETS)
    set(found 0)
    foreach(doc_target ${RIFT_DOCUMENTATION_TARGET_LIST})
      if("${doc_target}" MATCHES "${TARGET}")
        set(found 1)
      endif()
    endforeach(doc_target)
    if(NOT found)
      set(RIFT_DOCUMENTATION_TARGET_LIST ${RIFT_DOCUMENTATION_TARGET_LIST} 
        ${TARGET} CACHE INTERNAL "Documentation Target List")
    endif()
  endif()

  set(outdir_cfg "OUTPUT_DIRECTORY       = ${CMAKE_INSTALL_PREFIX}")
  set(outdir_cfg "${outdir_cfg}/doxygen/${ARGS_DEST_DIR}")

  if (ARGS_DOXY_NAME)
    set(PNAME "\"${ARGS_DOXY_NAME}\"")
  else (ARGS_DOXY_NAME)
    set(PNAME "${TARGET}")
  endif (ARGS_DOXY_NAME)

  if(ARGS_DOXY_EXCLUDE_PATTERNS)
    set(exclude_pattern_cfg "EXCLUDE_PATTERNS       =")
    foreach(_pattern ${ARGS_DOXY_EXCLUDE_PATTERNS})
      set(exclude_pattern_cfg "${exclude_pattern_cfg} ${_pattern}")
    endforeach(_pattern)
  else(ARGS_DOXY_EXCLUDE_PATTERNS)
    set(exclude_pattern_cfg "EXCLUDE_PATTERNS       =")
  endif(ARGS_DOXY_EXCLUDE_PATTERNS)

  if(ARGS_DOXY_FILES)
    set(input_cfg "INPUT                  =")
    foreach(_doxy_file ${ARGS_DOXY_FILES})
      set(input_cfg "${input_cfg} ${CMAKE_CURRENT_SOURCE_DIR}/${_doxy_file}")
    endforeach(_doxy_file)
  else(ARGS_DOXY_FILES)
    # DOXY_FILES not specified, generate the doxygen documentation for the 
    # current source directory
    set(input_cfg "INPUT                  = ${CMAKE_CURRENT_SOURCE_DIR}")
  endif(ARGS_DOXY_FILES)

  # copy the doxygen config template
  # and update the OUTPUT_DIRECTORY and INPUT
  add_custom_target(${TARGET}
    ${CMAKE_COMMAND} -E 
    copy ${RIFT_CMAKE_MODULE_DIR}/doxy_template.cfg 
    ${CMAKE_CURRENT_BINARY_DIR}/doxy.cfg
    COMMAND
    sed -i -e 's%^PROJECT_NAME[\ ]*=.*$$%PROJECT_NAME = ${PNAME}%g' doxy.cfg
    COMMAND
    sed -i -e 's%^OUTPUT_DIRECTORY[\ ]*=.*$$%${outdir_cfg}%g' doxy.cfg
    COMMAND
    sed -i -e 's%^INPUT[\ ]*=.*$$%${input_cfg}%g' doxy.cfg
    COMMAND
    sed -i -e 's%^EXCLUDE_PATTERNS[\ ]*=.*$$%${exclude_pattern_cfg}%g' doxy.cfg
    COMMAND
    rm -rf ${CMAKE_INSTALL_PREFIX}/doxygen/${ARGS_DEST_DIR}
    COMMAND
    mkdir -p ${CMAKE_INSTALL_PREFIX}/doxygen
    COMMAND
    doxygen doxy.cfg
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction(rift_add_doxygen_target)

##
# Function to create a target for vala documentation
# rift_add_valadoc_target(<TARGET>
#                         VALA_FILES <vala-file> ...
#                         DEST_DIR <documentation-destination>
#                         [VAPI_DIRS <vapi-directory> ...]
#                         [PACKAGES <package> ..])
function(rift_add_valadoc_target TARGET)
  ##
  # Parse the function arguments
  ##
  set(parse_options "")
  set(parse_onevalargs DEST_DIR)
  set(parse_multivalueargs VALA_FILES VAPI_DIRS PACKAGES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  # Parse the VAPI_DIRS
  unset(vapidir_opt)
  foreach(dir ${ARGS_VAPI_DIRS})
    list(APPEND vapidir_opts "--vapidir=${dir}")
  endforeach(dir)

  # Parse pacakges
  unset(pkg_opts)
  foreach(pkg ${ARGS_PACKAGES})
    list(APPEND pkg_opts "--pkg=${pkg}")
  endforeach(pkg)

  # Add the custom target for generating the documentation
  add_custom_target(${TARGET}
    rm -rf ${CMAKE_INSTALL_PREFIX}/documentation/vala/${ARGS_DEST_DIR}
    COMMAND
    mkdir -p ${CMAKE_INSTALL_PREFIX}/documentation/vala
    COMMAND
    valadoc -o ${CMAKE_INSTALL_PREFIX}/documentation/vala/${ARGS_DEST_DIR} 
    --vapidir=/usr/share/vala-0.16/vapi 
    ${vapidir_opts}
    ${pkg_opts}
    --basedir=${CMAKE_CURRENT_SOURCE_DIR}
    ${ARGS_VALA_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
endfunction(rift_add_valadoc_target)

##
# This function fetches all the issues for a component
# from JIRA database and stores it in asciidoc format
# rift_jira_gen_asciidoc(<TARGET>
#                        JIRA_PROJECT <project-name>
#                        JIRA_COMPONENTS <jira-component-list>
#                        JIRA_TASK_TYPE <task-type>)
##
function(rift_jira_gen_asciidoc TARGET)
  set(parse_options "")
  set(parse_onevalargs JIRA_PROJECT JIRA_TASK_TYPE)
  set(parse_multivalueargs JIRA_COMPONENTS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  foreach(component ${ARGS_JIRA_COMPONENTS})
    string(REPLACE "." "_" fname ${component})
    string(TOLOWER ${fname} fname)
    set(fname ${fname}.ad)
    message("asciidoc = ${fname}")
    list(APPEND asciidoc_files ${fname})

    add_custom_command(
      OUTPUT ${fname}
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../jira/scripts/rwjira.py 
      --input-jira
      --jira-filter-component ${component}
      --jira-project ${ARGS_JIRA_PROJECT}
      --jira-filter-tasktype ${ARGS_JIRA_TASK_TYPE}
      --output-asciidoc
      --output-file ${fname}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )
  endforeach(component)

  message("TARGET=${TARGET}")
  add_custom_target(${TARGET}
    DEPENDS ${asciidoc_files}
    )
endfunction(rift_jira_gen_asciidoc)

##
# This function converts fetches all the issues for a component
# from JIRA database and stores it in xml format
# rift_jira_gen_xml(<TARGET>
#                   JIRA_PROJECT <project-name>
#                   JIRA_COMPONENTS <jira-component-list>
#                   JIRA_TASK_TYPE <task-type>)
##
function(rift_jira_gen_xml TARGET)
  set(parse_options "")
  set(parse_onevalargs JIRA_PROJECT JIRA_TASK_TYPE)
  set(parse_multivalueargs JIRA_COMPONENTS)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  foreach(component ${ARGS_JIRA_COMPONENTS})
    string(REPLACE "." "_" fname ${component})
    string(TOLOWER ${fname} fname)
    set(fname ${fname}.xml)
    message("xml = ${fname}")
    list(APPEND xml_files ${fname})

    add_custom_command(
      OUTPUT ${fname}
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/jira/scripts/rwjira.py 
      --input-jira
      --jira-filter-component ${component}
      --jira-project ${ARGS_JIRA_PROJECT}
      --jira-filter-tasktype ${ARGS_JIRA_TASK_TYPE}
      --output-xml
      --output-file ${fname}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )
  endforeach(component)

  add_custom_target(${TARGET}
    DEPENDS ${xml_files}
    )
endfunction(rift_jira_gen_xml)

##
##
# This function converts the JIRA xml files into asciidoc.
# The RIFT requirements and software tasks are represented in the xml format
# This target converts the xml documents into asciidoc format. If the
# JIRA database is already populated with the requirements/tasks, this
# target fetches the JIRA ID for each task from the database, otherwise it
# puts xxx in place of JIRA ID.
#
# rift_jira_xml_to_asciidoc(<TARGET>
#                           JIRA_XML_FILE_LIST <jira-xml-file-list>)
##
function(rift_jira_xml_to_asciidoc TARGET)
  set(parse_options "")
  set(parse_onevalargs "")
  set(parse_multivalueargs JIRA_XML_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  foreach(jira_xml_file ${ARGS_JIRA_XML_FILES})
    get_filename_component(basename ${jira_xml_file} NAME_WE)
    list(APPEND asciidoc_files ${basename}.ad)

    add_custom_command(
      OUTPUT ${basename}.ad
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/jira/scripts/rwjira.py 
      --jira-project RIFTWARE 
      --input-xml ${CMAKE_CURRENT_SOURCE_DIR}/${jira_xml_file}
      --output-asciidoc
      --output-file ${basename}.ad
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${jira_xml_file}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )
  endforeach(jira_xml_file)

  add_custom_target(${TARGET}
    DEPENDS ${asciidoc_files}
    )
endfunction(rift_jira_xml_to_asciidoc)

##
# This function converts ASCIIDOC files to PDF
# rift_asciidoc_to_pdf(<TARGET>
#                      ASCIIDOC_FILES <asciidoc-file-list>)
##
function(rift_asciidoc_to_pdf TARGET)
  set(parse_options "")
  set(parse_onevalargs "")
  set(parse_multivalueargs ASCIIDOC_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(RIFT_DOCUMENTATION_TARGET_LIST ${RIFT_DOCUMENTATION_TARGET_LIST} ${TARGET} 
    CACHE INTERNAL "Documentation Target List")

  set(asciidoctor "/bin/asciidoctor")
  if (EXISTS "/usr/bin/asciidoctor")
    set(asciidoctor "/usr/bin/asciidoctor")
  endif()

  foreach(asciidoc_file ${ARGS_ASCIIDOC_FILES})
    get_filename_component(basename ${asciidoc_file} NAME_WE)
    list(APPEND pdf_files ${basename}.pdf)

    add_custom_command(
      OUTPUT ${basename}.xml
      COMMAND ${asciidoctor} -b docbook45 -o - ${CMAKE_CURRENT_SOURCE_DIR}/${asciidoc_file} 
           | sed -e 's/contentwidth=\"banana\"/width=\"100%\" scalefit=\"1\"/'
           | sed -e 's/width=\"banana\"/width=\"100%\" scalefit=\"1\"/' > ${CMAKE_CURRENT_BINARY_DIR}/${basename}.xml 
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${asciidoc_file}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )

    add_custom_command(
      OUTPUT ${basename}.fo
      COMMAND xsltproc --maxdepth 5000 --stringparam callout.graphics 0 
      --stringparam navig.graphics 0 --stringparam admon.textlabel 1 
      --stringparam admon.graphics 0 --output ${CMAKE_CURRENT_BINARY_DIR}/${basename}.fo 
      "/etc/asciidoc/docbook-xsl/fo.xsl" ${CMAKE_CURRENT_BINARY_DIR}/${basename}.xml
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${basename}.xml
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )

    add_custom_command(
      OUTPUT ${basename}_no_copyright.pdf
      COMMAND fop -q -fo ${CMAKE_CURRENT_BINARY_DIR}/${basename}.fo
      -pdf ${CMAKE_CURRENT_BINARY_DIR}/${basename}_no_copyright.pdf
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${basename}.fo
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )

    add_custom_command(
      OUTPUT ${basename}.pdf
      COMMAND 
      pdftk ${basename}_no_copyright.pdf 
      stamp ${RIFT_CMAKE_MODULE_DIR}/riftio_watermark.pdf
      output ${basename}.pdf
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${basename}_no_copyright.pdf
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )
  endforeach(asciidoc_file)

  add_custom_target(${TARGET}
    DEPENDS ${pdf_files}
    )
endfunction(rift_asciidoc_to_pdf)

##
# This function converts ASCIIDOC files to HTML
# rift_asciidoc_to_html(<TARGET>
#                       ASCIIDOC_FILES <asciidoc-file-list>)
##
function(rift_asciidoc_to_html TARGET)
  set(parse_options "")
  set(parse_onevalargs "")
  set(parse_multivalueargs ASCIIDOC_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(asciidoctor "/bin/asciidoctor")
  if (EXISTS "/usr/bin/asciidoctor")
    set(asciidoctor "/usr/bin/asciidoctor")
  endif()

  foreach(asciidoc_file ${ARGS_ASCIIDOC_FILES})
    get_filename_component(basename ${asciidoc_file} NAME_WE)
    list(APPEND html_files ${basename}.html)
    add_custom_command(
      OUTPUT ${basename}.html
      COMMAND 
      ${asciidoctor} -a toc2 -a toc-position=left -b html5 
      -o ${CMAKE_CURRENT_BINARY_DIR}/${basename}.html -a theme=flask
      ${CMAKE_CURRENT_SOURCE_DIR}/${asciidoc_file}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${asciidoc_file}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )
  endforeach(asciidoc_file)

  add_custom_target(${TARGET}
    DEPENDS ${html_files}
    )
endfunction(rift_asciidoc_to_html)

##
# This function determines the category of the asciidoc file
# Each asciidoc file is expected to have a header have a comment
# indicating the category of the file. This category is used by
# the index.html for creating a top level html page. The format of the
# comment should be qas indicated below:
# // Specify the category of this document. The following
# // comment will be used to bin the document into a
# // category on the "index.html" page.
# // Category: Process and Tools
#
# rift_get_file_category(ASCIIDOC_FILE <asciidoc-file>
#                        OUT_CATEGORY <namee-of-the-category-variable>)
# 
# NOTE: The category will be stored in the OUT_CATEGORY
##
function(rift_get_file_category)
  set(parse_options "")
  set(parse_onevalargs ASCIIDOC_FILE OUT_CATEGORY)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  file(STRINGS ${ARGS_ASCIIDOC_FILE} tempvar 
    REGEX "^// Category: ([.]*)")
  list(LENGTH tempvar tempcount)
  if(NOT ${tempcount} EQUAL 1)
    set(errmsg "Expecting one '// Category' statement, but found ${tempcount}")
    set(errmsg "${errmsg} in the file ${ARGS_ASCIIDOC_FILE}.")
    message(FATAL_ERROR "${errmsg}")
  endif()

  string(REGEX REPLACE "^// Category: ([.]*)" "\\1" tempcat ${tempvar})

  set(${ARGS_OUT_CATEGORY} ${tempcat} PARENT_SCOPE)
endfunction(rift_get_file_category)

##
# This function determines the title of the asciidoc file
# It extracts the title from the following format:
# **Title**
# =========
#
# rift_get_file_title(ASCIIDOC_FILE <asciidoc-file>
#                    OUT_TITLE <name-of-the-title-variable>)
# 
# NOTE: The title will be stored in the OUT_TITLE
##
function(rift_get_file_title)
  set(parse_options "")
  set(parse_onevalargs ASCIIDOC_FILE OUT_TITLE)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  file(STRINGS ${ARGS_ASCIIDOC_FILE} tempvar 
    LIMIT_COUNT 1 REGEX "^\\*\\*")
  list(LENGTH tempvar tempcount)
  if(NOT ${tempcount} EQUAL 1)
    set(errmsg "Didn't find title in the file ${ARGS_ASCIIDOC_FILE}.")
    message(FATAL_ERROR "${errmsg}")
  endif()

  string(REGEX REPLACE "^\\*\\*([^*]*)" "\\1" title ${tempvar})

  set(${ARGS_OUT_TITLE} ${title} PARENT_SCOPE)
endfunction(rift_get_file_title)

##
# This function generates the header portion of index.html
# rift_generate_index_html_head(INDEX_FILE <output-file-name>)
##
function(rift_generate_index_html_head)
  set(parse_options "")
  set(parse_onevalargs INDEX_FILE)
  set(parse_multivalueargs "")
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(fname ${ARGS_INDEX_FILE})

  file(WRITE ${fname}  "<!DOCTYPE html>\n")
  file(APPEND ${fname} "<html lang = \"en\">\n")
  file(APPEND ${fname} "<head>\n")
  file(APPEND ${fname} "    <meta charset=\"UTF-8\">\n")
  file(APPEND ${fname} "    <title>RIFT.io Documentation Home</title>\n")
  file(APPEND ${fname} "</head>\n")

  file(APPEND ${fname} "<body>\n")
  file(APPEND ${fname} "    <h2 align=\"center\">\n")
  file(APPEND ${fname} "        <img src=images/riftio-logo.png alt=\"RIFTio\" width=\"150\" height=\"47\"/>\n")
  file(APPEND ${fname} "        Engineering Asciidocs\n")
  file(APPEND ${fname} "    </h2>\n")
  file(APPEND ${fname} "    <hr>\n")
  file(APPEND ${fname} "    <p>Links to many more documents are available at the \n")
  file(APPEND ${fname} "    <a href='https://confluence.riftio.com/display/RB/Documentation'>Documentation Home</a>\n")
  file(APPEND ${fname} "    </p>\n")
  file(APPEND ${fname} "    <hr>\n")

  file(APPEND ${fname} "    <style type=\"text/css\">\n")
  file(APPEND ${fname} "        .left {\n")
  file(APPEND ${fname} "            /* background-color: LightGray; */\n")
  file(APPEND ${fname} "            float: left;\n")
  file(APPEND ${fname} "            width: 50%;\n")
  file(APPEND ${fname} "        }\n")

  file(APPEND ${fname} "        .right {\n")
  file(APPEND ${fname} "            /* background-color: LightGray; */\n")
  file(APPEND ${fname} "            float: right;\n")
  file(APPEND ${fname} "            width:50%;\n")
  file(APPEND ${fname} "        }\n")
  file(APPEND ${fname} "    </style>\n")

endfunction(rift_generate_index_html_head)

##
# The documentation links are presented in two columns
# This function emits the necessary output for one column
#
# rift_generate_index_html_column(COLUMN_TYPE <"left"|"right">
#                                 CATEGORY_LIST <category-list>
#                                 ASCIIDOC_FILES <asciidoc-file-list>)
##
function(rift_generate_index_html_column)
  set(parse_options "")
  set(parse_onevalargs INDEX_FILE COLUMN_TYPE)
  set(parse_multivalueargs CATEGORY_LIST ASCIIDOC_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(fname ${ARGS_INDEX_FILE})
  file(APPEND ${fname} "    <div class=\"${ARGS_COLUMN_TYPE}\">\n")
  file(APPEND ${fname} "        <ul>\n")

  foreach(cat ${ARGS_CATEGORY_LIST})
    file(APPEND ${fname} "            <li>\n")
    file(APPEND ${fname} "                <h3>${cat}</h3>\n")
    file(APPEND ${fname} "                <ol>\n")
    foreach(f ${ARGS_ASCIIDOC_FILES})
      rift_get_file_category(
        ASCIIDOC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${f}
        OUT_CATEGORY file_category
        )
      rift_get_file_title(
        ASCIIDOC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${f}
        OUT_TITLE file_title
        )
      if(cat STREQUAL file_category)
        get_filename_component(file_pfx ${f} NAME_WE)
        file(APPEND ${fname} "                    <li> <a href=\"html/${file_pfx}_index.html\">\n")
        file(APPEND ${fname} "                            ${file_title} </a> </li>\n")
      endif()
    endforeach(f)
    file(APPEND ${fname} "                </ol>\n")
    file(APPEND ${fname} "            </li>\n")
  endforeach(cat)

  file(APPEND ${fname} "        </ul>\n")
  file(APPEND ${fname} "    </div>\n")
endfunction(rift_generate_index_html_column)

##
# This function generates index.html for rift documentation 
# web site.
#
# rift_generate_index_html(CATEGORY_COLUMN_A <column-A-category-list>
#                          CATEGORY_COLUMN_B <column-B-category-list>
#                          ASCIIDOC_FILES <asciidoc-file-list>)
##
function(rift_generate_index_html)
  set(parse_options "")
  set(parse_onevalargs "" )
  set(parse_multivalueargs CATEGORY_COLUMN_A CATEGORY_COLUMN_B ASCIIDOC_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(ASCIIDOC_CATEGORY_LIST 
    ${ARGS_CATEGORY_COLUMN_A} 
    ${ARGS_CATEGORY_COLUMN_B}
    )

  ##
  # Validate the each document has precisely one "// Category: " statement, and
  # category is valid.
  ##
  foreach(f ${ARGS_ASCIIDOC_FILES})
    rift_get_file_category(
      ASCIIDOC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${f}
      OUT_CATEGORY file_category
      )

    unset(found)
    foreach(cat ${ASCIIDOC_CATEGORY_LIST})
      if(cat STREQUAL file_category)
        set(found 1)
      endif()
    endforeach(cat)
    if(NOT found)
      message("Invalid category '${file_category}' specified for file '${f}'")
      message(FATAL_ERROR "Valid category list is '${ASCIIDOC_CATEGORY_LIST}'")
    endif()
  endforeach(f)

  # generate the header for index.html
  rift_generate_index_html_head(
    INDEX_FILE ${CMAKE_CURRENT_BINARY_DIR}/index.html)

  # generate the left column
  rift_generate_index_html_column(
    INDEX_FILE ${CMAKE_CURRENT_BINARY_DIR}/index.html
    COLUMN_TYPE "left"
    CATEGORY_LIST ${ARGS_CATEGORY_COLUMN_A}
    ASCIIDOC_FILES ${ARGS_ASCIIDOC_FILES})

  # generate the right column
  rift_generate_index_html_column(
    INDEX_FILE ${CMAKE_CURRENT_BINARY_DIR}/index.html
    COLUMN_TYPE "right"
    CATEGORY_LIST ${ARGS_CATEGORY_COLUMN_B}
    ASCIIDOC_FILES ${ARGS_ASCIIDOC_FILES})
 
  # close the html file 
  file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/index.html "</body>\n")
  file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/index.html "</html>\n")
endfunction(rift_generate_index_html)

##
# This function generates an html file that can contain the
# html generated from the asciidoc->html tool.
#
# This html will have a navigation section at the top and
# uses iframe to contain the html from the asciidoc tool
#
# rift_generate_iframe_html(ASCIIDOC_FILES <file-list>)
##
function(rift_generate_iframe_html)
  set(parse_options "")
  set(parse_onevalargs "")
  set(parse_multivalueargs ASCIIDOC_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  foreach(f ${ARGS_ASCIIDOC_FILES})
    get_filename_component(basename ${f} NAME_WE)
    rift_get_file_title(
      ASCIIDOC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${f}
      OUT_TITLE file_title
      )

    set(fname ${CMAKE_CURRENT_BINARY_DIR}/${basename}_index.html)
    file(WRITE ${fname}  "<!DOCTYPE html>\n")
    file(APPEND ${fname} "<html lang = \"en\">\n")
    file(APPEND ${fname} "<head>\n")
    file(APPEND ${fname} "    <meta charset=\"UTF-8\">\n")
    file(APPEND ${fname} "    <title>${file_title}</title>\n")
    file(APPEND ${fname} "</head>\n")
    file(APPEND ${fname} "<body>\n")
    file(APPEND ${fname} "    <style type=\"text/css\">\n")
    file(APPEND ${fname} "        html, body { overflow-y:hidden; }\n")
    file(APPEND ${fname} "        html, body, div, iframe { margin:0; padding:0; height:85%; }\n")
    file(APPEND ${fname} "        iframe { position:fixed; display:block; width:100%; border:none; }\n")
    file(APPEND ${fname} "    </style>\n")

    file(APPEND ${fname} "    <h3 align=\"center\">\n")
    file(APPEND ${fname} "        <img src=../images/riftio-logo.png alt=\"RIFT.io\" width=\"100\" height=\"32\" align=\"left\">\n")
    file(APPEND ${fname} "        <a href=\"../index.html\">Documentation Home</a>\n")
    file(APPEND ${fname} "        <img src=../images/next-arrow.png>\n")
    file(APPEND ${fname} "        ${file_title}\n")
    file(APPEND ${fname} "        <a href=\"../pdf/${basename}.pdf\">\n")
    file(APPEND ${fname} "        <img src=../images/pdf.png>\n")
    file(APPEND ${fname} "        </a>\n")
    file(APPEND ${fname} "    </h3>\n")

    file(APPEND ${fname} "    <hr>\n")

    file(APPEND ${fname} "    <iframe seamless src=\"${basename}.html\"></iframe>\n")

    file(APPEND ${fname} "</body>\n")
    file(APPEND ${fname} "</html>\n")
  endforeach(f)

endfunction(rift_generate_iframe_html)

##
# This function converts LYX files to PDF
# rift_lyx_to_pdf(<TARGET>
#                 LYX_FILES <lyx-file-list>)
##
function(rift_lyx_to_pdf TARGET)
  set(parse_options "")
  set(parse_onevalargs "")
  set(parse_multivalueargs LYX_FILES)
  cmake_parse_arguments(ARGS "${parse_options}" "${parse_onevalargs}" "${parse_multivalueargs}" ${ARGN})

  set(RIFT_DOCUMENTATION_TARGET_LIST ${RIFT_DOCUMENTATION_TARGET_LIST} ${TARGET} 
    CACHE INTERNAL "Documentation Target List")

  foreach(lyx_file ${ARGS_LYX_FILES})
    get_filename_component(basename ${lyx_file} NAME_WE)
    list(APPEND pdf_files ${CMAKE_CURRENT_BINARY_DIR}/${basename}.pdf)
    message("Generating ${basename}.pdf")

    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${basename}_no_copyright.pdf
      COMMAND lyx -e pdf ${CMAKE_CURRENT_SOURCE_DIR}/${lyx_file}
      COMMAND mv -f ${CMAKE_CURRENT_SOURCE_DIR}/${basename}.pdf
      ${basename}_no_copyright.pdf
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${lyx_file}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )

    add_custom_command(
      OUTPUT ${basename}.pdf
      COMMAND 
      pdftk ${basename}_no_copyright.pdf 
      stamp ${RIFT_CMAKE_MODULE_DIR}/riftio_watermark.pdf
      output ${basename}.pdf
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${basename}_no_copyright.pdf
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      )
  endforeach(lyx_file)

  add_custom_target(${TARGET} ALL
    DEPENDS ${pdf_files}
    )
endfunction(rift_lyx_to_pdf)

