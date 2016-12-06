
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */



/**
 * @file yangpbc_doc_writer.hpp
 *
 * Yang HTML documentation generator.
 */

#ifndef RW_YANGTOOLS_YANGPBC_DOC_WRITER_HPP__
#define RW_YANGTOOLS_YANGPBC_DOC_WRITER_HPP__

#include "yangpbc.hpp"
#include <fstream>
#include <stack>
#include <sstream>

namespace rw_yang {

/**
 * DocType of the generated HTML document.
 */
enum class HTMLDocType {
  DOC_TYPE_API,  ///< Used for Programmers API reference
  DOC_TYPE_USER  ///< Used for User documentation
};

/**
 * Utility class that emits the Table Of Contents items
 */
class TocListItem
{
 public:
  /**
   * Constructor for the TOC list item. Internally constructs the HTML for the
   * list item.
   * @param os [in] Output stream to which the HTML TOC will be written
   * @param text [in] Item text
   * @param id   [in] HTML Element id attribute
   * @param type [in] Type of the Yang Element
   * @param pbmsg [in] PbMessage corresponding to the list item
   */
  TocListItem(
      std::ostream& os, 
      const std::string& text,
      const std::string& id,
      const std::string& type,
      PbMessage* pbmsg,
      bool first_item);

  /**
   * Desctructor - gracefully closes the list tag.
   */
  ~TocListItem();

 public:
  bool has_list_ = false; ///< Specifies if this List Item has a list within

 public:
  std::ostream& os_;              ///< Output stream to which HTML is written
  PbMessage*    pbmsg_ = nullptr; ///< PbMessage corresponding to the list item
};

/**
 * Utility class that emits the content HTML for every PbMessage. The contents
 * are written within a <section/> tag and identified with a xpath like id.
 */
class PbMessageSection
{
 public:
  /**
   * Constructor for the PbMessage HTML content. Internally constructs the
   * section for the Yang node.
   * @param os [in] Output stream to which HTML content is written to.
   * @param pbmsg [in] PbMessage for the Yang node that is getting documented
   * @param title [in] Heading for the Yang node
   * @param id    [in] Unique identifier (HTML id) XPath like
   * @param type  [in] Type of the Yang node
   * @param top_level [in] Specified if the node is at the top of a Module
   * @param doc_type  [in] API reference or user documentation
   */
  PbMessageSection(
            std::ostream& os, 
            PbMessage* pbmsg,
            const std::string& title,
            const std::string& id,
            const std::string& type,
            bool top_level,
            HTMLDocType doc_type);

  /**
   * Desctructor that closes the appropriate HTML tag
   */
  ~PbMessageSection();

 public:
  /**
   * Adds an entry into the Definitions table. Used for displaying the Key
   * Identifiers.
   * @param term [in] Identifier name
   * @param val  [in] Identifier value
   */
  void add_table_def(const std::string& term, const std::string& val);

  /**
   * Adds an entry into the Fields table.
   * @param name [in] Field name
   * @param type [in] Field type (yang type)
   * @param descr [in] Field description
   * @param ref  [in] Id of the content section for this field
   */
  void add_table_child(
          const std::string& name,
          const std::string& type,
          const std::string& descr,
          const std::string& ref);

  /**
   * Adds a sample json/xml into the content section.
   * @param heading [in] Heading for the sample 
   * @param content [in] sample content
   */
  void add_sample(const std::string& heading, 
          const std::string& content);

  /**
   * Writes all the key identifiers of the yang node into the content stream.
   */
  void write_key_identifiers();

  /**
   * Writes all the fields of the yang node into the content stream.
   */
  void write_children();

  /**
   * Writes sample json/xml for the yang node into the content stream.
   */
  void write_samples();

 public:
  std::ostream& os_;            ///< content stream
  PbMessage* pbmsg_ = nullptr;  ///< PbMessage for the yang node
  HTMLDocType doc_type_;        ///< API reference or User doc
};

/**
 * Writes HTML documentation for the given Yang model/module. This is
 * implemented as a visitor for the class PbModel. Visitor callbacks are invoked
 * when a PbModule or PbMessage is traversed in the model hierarchy.
 */
class HTMLDocWriter: public PbModelVisitor
{
 public:
  /**
   * Constructor for the HTMLDocWriter.
   * @param html_file [in] HTML filename to which the documentation is written
   * @param doc_type  [in] Specifies if it is a API or User doc
   */
  HTMLDocWriter(const std::string& html_file, HTMLDocType doc_type);

  /**
   * Destructor for HTMLDocWriter
   */
  ~HTMLDocWriter() override;

 public:
  /**
   * Returns true when the HTML file is successfully created.
   */
  bool is_good() const;

  /**
   * Writes the contents from the in-memory streams to HTML file.
   */
  void write_to_file();

 public:

  /**
   * Invoked when the PbModule is traversed, during the model traversal. Used to
   * write the module globals.
   *
   * @see PbModelVisitor::visit_module_enter()
   * @returns true if further traversal is needed, false otherwise.
   */
  bool visit_module_enter(PbModule* module) override;

  /**
   * Invoked when PbModule traversal is done. 
   *
   * @see PbModelVisitor::visit_module_exit()
   * @returns true if further traversal is needed, false otherwise.
   */
  bool visit_module_exit(PbModule* module) override;

  /**
   * Invoked when a PbMessage is traversed. In this callback Toc items and
   * content sections are written to the toc_stream and content_stream
   * respectively.
   *
   * @see PbModelVisitor::visit_message_enter()
   * @returns true if further traversal is needed, false otherwise.
   */
  bool visit_message_enter(PbMessage* msg) override;

  /**
   * Invoked when a PbMessage traversal is done. Used to properly close the HTML
   * tags.
   *
   * @see PbModelVisitor::visit_message_exit()
   * @returns true if further traversal is needed, false otherwise.
   */
  bool visit_message_exit(PbMessage* msg) override;

 public:
  /**
   * Writes the globals and the document heading to the content_stream
   */
  void write_content_head(PbModule* module);

 public:
 
  std::ofstream html_file_; ///< HTML File output stream
  bool is_good_ = false;    ///< Specifies if the HTML file open was a success

  HTMLDocType doc_type_;  ///< Specifies if it is a API or User documentation

  /**
   * A TOC stack is used to create TOC list items when a PbMessage is traversed
   * and terminate the HTML list tags when PbMessage traversal is done.
   */
  std::stack<TocListItem> toc_stack_;

  /**
   * Similar to the TOC stack the section stack helps in closing the section
   * tags when PbMessage traversal is done.
   */
  std::stack<PbMessageSection> section_stack_;

  std::stringstream title_stream_;   ///< In-memory stream for <title/>
  std::stringstream toc_stream_;     ///< In-memory stream for ToC
  std::stringstream content_stream_; ///< In-memory stream fro content section
};

} // namespace rw_yang

#endif // RW_YANGTOOLS_YANGPBC_DOC_WRITER_HPP__
