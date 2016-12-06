
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
 * @file yangpbc_doc_writer.cpp
 *
 * Yang documentation generator
 */

#include "yangpbc_doc_writer.hpp"

#include <iostream>
#include <boost/format.hpp>
#include <algorithm>

using namespace rw_yang;

static std::string html_head_str {
"\n<head>\n"
"  <script>\n"
"    function add_list_item(name, id, type) {\n"
"      var li_element = document.currentScript.parentElement;\n"
"      li_element.innerHTML = `\n"
"        <input type=\"checkbox\" id=\"c${id}\" checked=\"checked\"/>\n"
"        <label for=\"c${id}\" class=\"checked\">&#x25bc;</label>\n"
"        <label for=\"c${id}\" class=\"unchecked\">&#x25b6;</label>\n"
"        <a href=\"#${id}\">${name}</a>`;\n"
"    }\n"
"    function on_load() {\n"
"      var toc_width = document.getElementById(\"toc\").offsetWidth;\n"
"      document.getElementById(\"content\").style.marginLeft = toc_width;\n"
"    }\n"
"  </script>\n"
"  <link rel=\"stylesheet\" type=\"text/css\" href=\"rwyangdoc.css\"/>\n"
"</head>\n"
};

TocListItem::TocListItem(std::ostream& os, 
      const std::string& text,
      const std::string& id,
      const std::string& type,
      PbMessage* pbmsg,
      bool first_item)
  : os_(os),
    pbmsg_(pbmsg)
{
  if (first_item) {
    os_ << "\n<ul>\n";
  }
  os_ << boost::format(
          "<li>\n<script>add_list_item(\"%1%\",\"%2%\",\"%3%\")</script>")
          % text % id % type << std::endl;
}

TocListItem::~TocListItem()
{
  if (has_list_) {
    os_ << "\n</ul>\n";
  }
  os_ << "</li>\n";
}

PbMessageSection::PbMessageSection(
            std::ostream& os, 
            PbMessage* pbmsg,
            const std::string& title,
            const std::string& id,
            const std::string& type,
            bool top_level,
            HTMLDocType doc_type)
  : os_(os),
    pbmsg_(pbmsg),
    doc_type_(doc_type)
{
  std::string heading;
  if (top_level) {
    heading = "h2";
  } else {
    heading = "h3";
  }

  os_ << boost::format(
    "<section id=\"%2%\">\n"
    "  <%1%>%3%</%1%>\n"
    )
    % heading
    % id
    % title;

  if (top_level) {
    const char* ydesc = pbmsg_->get_schema_defining_pbmod()->get_ymod()->get_description();
    os_ << "<p class=\"description\">" << ydesc << "</p>\n";
    return;
  }

  const char* ydesc = pbmsg_->get_ynode()->get_description();
  os_ << "<p class=\"description\">" << ydesc << "</p>\n";

  write_key_identifiers();
  write_children();

  if (doc_type_ == HTMLDocType::DOC_TYPE_USER) {
    write_samples();
  }
}

PbMessageSection::~PbMessageSection()
{
  os_ << "</section>\n";
}

void PbMessageSection::add_table_def(const std::string& term, const std::string& val)
{
  os_ << boost::format(
          "<tr><td>%1%</td><td><pre>%2%</pre></td></tr>\n")
          % term % val;
}

void PbMessageSection::add_table_child(
          const std::string& name,
          const std::string& type,
          const std::string& descr,
          const std::string& ref)
{
  std::string child;
  if (!ref.empty()) {
    child = "<a href=\"#" + ref + "\">" + name + "</a>";
  } else {
    child = name;
  }

  os_ << boost::format(
          "<tr><td>%1%</td><td>%2%</td><td>%3%</td></tr>\n")
          % child
          % type
          % descr;
}

void PbMessageSection::add_sample(
      const std::string& heading,
      const std::string& content)
{
  unsigned lines = std::count(content.begin(), content.end(), '\n');
  lines +=2;
  if (lines > 30) {
    lines = 30;
  }

  os_ << boost::format(
          "<br/><strong><em>%1%</em></strong><br/>\n"
          "<textarea rows=\"%3%\" cols=\"100\" readonly>\n"
          "%2%\n"
          "</textarea>\n")
          % heading % content % lines;
}

void PbMessageSection::write_key_identifiers()
{
  os_ << "<em>Key Identifiers:</em>\n"
      << "<table class=\"definitions\">\n";

  add_table_def("XPath Path", pbmsg_->get_xpath_path());
  add_table_def("XPath Keyed Path", pbmsg_->get_xpath_keyed_path());
  add_table_def("RW.REST URI Path", pbmsg_->get_rwrest_uri_path());

  // only for API html
  if (doc_type_ == HTMLDocType::DOC_TYPE_API) {
    std::string prefix;
    switch(pbmsg_->get_category_msg_type()) {
      case PbMsgType::data_module:
      case PbMsgType::group_root:
      case PbMsgType::group:
        if (pbmsg_->get_ynode()->is_config()) {
          add_table_def("RW Keyspec XPath Path", 
                        "C," + pbmsg_->get_xpath_path());
          add_table_def("RW Keyspec XPath Keyed Path", 
                        "C," + pbmsg_->get_xpath_keyed_path());
        }
        if (pbmsg_->get_pbnode()->has_op_data()) {
          prefix = "D,";
        }
        break;
      case PbMsgType::rpci_module:
      case PbMsgType::rpc_input:
        prefix = "I,";
        break;
      case PbMsgType::rpco_module:
      case PbMsgType::rpc_output:        
        prefix = "O,";
        break;
      case PbMsgType::notif_module:
      case PbMsgType::notif:
        prefix = "N,";
        break;
      default:
        RW_ASSERT_NOT_REACHED();
    }

    if (!prefix.empty()) {
      add_table_def("RW Keyspec XPath Path", 
          prefix + pbmsg_->get_xpath_path());
      add_table_def("RW Keyspec XPath Keyed Path", 
          prefix + pbmsg_->get_xpath_keyed_path());
    }

    add_table_def("Protobuf Type", pbmsg_->get_proto_message_path());
    add_table_def("Python Proto-GI Type", pbmsg_->get_gi_python_typename());
    add_table_def("C Protobuf-C Struct Type", pbmsg_->get_pbc_message_typename());

    std::string rwpb = pbmsg_->get_rwpb_long_alias();
    if (!pbmsg_->has_conflict()) {
      std::string alias = pbmsg_->get_rwpb_short_alias();
      if (alias.length()) {
        rwpb.push_back('\n');
        rwpb.append(alias);
      }
      alias = pbmsg_->get_rwpb_msg_new_alias();
      if (alias.length()) {
        rwpb.push_back('\n');
        rwpb.append(alias);
      }
    }
    add_table_def("C RWPB Identifiers", rwpb);
    add_table_def("YPBC Base Identifier", pbmsg_->get_ypbc_long_alias());
  }
  os_ << "</table>\n";
}

void PbMessageSection::write_children()
{
  if (!pbmsg_->has_fields()) {
    return;
  }
  os_ << "<br/><em>Fields</em>\n"
      << "<table class=\"children\">\n"
      << "  <tr><th>Name</th><th>Type</th><th>Description</th></tr>\n";

  for (auto fi = pbmsg_->field_begin(); fi != pbmsg_->field_end(); ++fi) {
    YangNode *ynode = (*fi)->get_ynode();
    std::string descr = ynode->get_description();
    std::string type;
    std::string ref;
    auto stmt_type = ynode->get_stmt_type();
    if (stmt_type == RW_YANG_STMT_TYPE_LEAF) {
      type = rw_yang_leaf_type_string(ynode->get_type()->get_leaf_type());
    } else {
      type = rw_yang_stmt_type_string(stmt_type);
      PbMessage* fmsg = (*fi)->get_field_pbmsg();
      if (fmsg) {
        ref = fmsg->get_xpath_path();
        std::replace(ref.begin(), ref.end(), '/', '.');
      }
    }

    add_table_child(ynode->get_name(), type, descr, ref);
  }

  os_ << "</table>\n";
}

void PbMessageSection::write_samples()
{
  YangNode* ynode = pbmsg_->get_ynode();
  if (ynode->is_config()) {
    add_sample("JSON Config Sample", ynode->get_rwrest_json_sample_element(0, 2, true));
  }
  add_sample("JSON Sample", ynode->get_rwrest_json_sample_element(0, 2, false));

  if (ynode->is_config()) {
    add_sample("XML Config Sample", ynode->get_xml_sample_element(0, 2, true, true));
  }
  add_sample("XML Sample", ynode->get_xml_sample_element(0, 2, false, true));
}

HTMLDocWriter::HTMLDocWriter(const std::string& html_file, HTMLDocType doc_type)
  : html_file_(html_file.c_str()),
    doc_type_(doc_type)
{
  if (html_file_.is_open()) {
    is_good_ = true;
  }
}

HTMLDocWriter::~HTMLDocWriter()
{
}

bool HTMLDocWriter::is_good() const {
  return is_good_;
}

void HTMLDocWriter::write_to_file()
{
  std::stringstream tmp;
  tmp << boost::format(
    "<html>\n"
    "<title>\n"
    "%1%"
    "</title>\n"
    "%2%"
    "<body onload=\"on_load()\">\n"
    "  <div class=\"wrapper\">\n"
    "    <nav class=\"toc-treeview\" id=\"toc\">\n"
    "     <h1>Table of Contents</h1>\n"
    "     <ul>\n"
    "%3%"
    "     </ul>\n"
    "    </nav>\n" 
    "    <section class=\"content\" id=\"content\">\n"
    "%4%"
    "    </section>\n"
    "  </div>\n"
    "</html>")
    % title_stream_.str()
    % html_head_str
    % toc_stream_.str()
    % content_stream_.str()
    << std::endl;

  html_file_ << tmp.str();

  html_file_.close();
}

void HTMLDocWriter::write_content_head(PbModule* module)
{
  std::string heading;
  if (doc_type_ == HTMLDocType::DOC_TYPE_API) {
    heading = "Programmers API Reference";
  } else {
    heading = "User documentation";
  }
  content_stream_ << boost::format(
    "  <h1>%1% - %4%</h1>\n"
    "  <section id=\"1\">\n"
    "    <h2>Schema Globals</h2>\n"
    "    <table class=\"definitions\">\n"
    "      <tr>\n"
    "        <td>Global Schema Type</td>\n"
    "        <td><pre>%2%</pre></td>\n"
    "      </tr>\n"
    "      <tr>\n"
    "        <td>Global Schema Pointer</td>\n"
    "        <td><pre>%3%</pre></td>\n"
    "      </tr>\n"
    "    </table>\n"
    "  </section>\n")
    % module->get_ymod()->get_name()
    % module->get_ypbc_global("t_schema")
    % module->get_ypbc_global("g_schema")
    % heading;

  TocListItem temp(toc_stream_, "Schema Globals", "1", "", nullptr, false);
  (void)temp;
}

bool HTMLDocWriter::visit_module_enter(PbModule* module)
{
  title_stream_ << module->get_ymod()->get_name() << ".yang ";
  if (doc_type_ == HTMLDocType::DOC_TYPE_API) {
    title_stream_ << "- API reference\n";   
  } else {
    title_stream_ << "- User documentation\n";   
  }
  write_content_head(module);

  return true;
}

bool HTMLDocWriter::visit_module_exit(PbModule* module)
{
  return true;
}

bool HTMLDocWriter::visit_message_enter(PbMessage* msg)
{
  auto stmt_type = msg->get_pbnode()->get_yang_stmt_type();
  auto category_msg_type = msg->get_category_msg_type();
  std::string title;
  std::string type;
  bool top_level = false;

  switch(stmt_type) {
    case RW_YANG_STMT_TYPE_CONTAINER:
      title = msg->get_pbnode()->get_xml_element_name();
      type = "container";
      break;
    case RW_YANG_STMT_TYPE_LIST:
      title = msg->get_pbnode()->get_xml_element_name();
      type = "list";
      break;
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      title = msg->get_pbnode()->get_xml_element_name();
      type = "leaf-list";
      break;
    case RW_YANG_STMT_TYPE_RPCIO:
      if (msg->get_pbnode()->get_yang_fieldname() == "input") {
        title = msg->get_pbnode()->get_parent_pbnode()->get_xml_element_name();
        type = "rpc input";
      } else {
        title = msg->get_pbnode()->get_parent_pbnode()->get_xml_element_name();
        type = "rpc output";
      }
      break;
    case RW_YANG_STMT_TYPE_NOTIF:
      title = msg->get_pbnode()->get_xml_element_name();
      type = "notification";
      break;
    case RW_YANG_STMT_TYPE_ROOT:
      title = msg->get_pbmod()->get_ymod()->get_name();
      top_level = true;
      switch(category_msg_type) {
        case PbMsgType::data_module:
          type = "data";
          break;
        case PbMsgType::rpci_module:
          type = "rpc input";
          break;
        case PbMsgType::rpco_module:
          type = "rpc output";
          break;
        case PbMsgType::notif_module:
          type = "notification";
          break;
        default:
          RW_ASSERT_NOT_REACHED();
      }
      title += " " + type;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  bool first_item = false;
  if (!toc_stack_.empty() &&
      toc_stack_.top().pbmsg_ == msg->get_parent_pbmsg() &&
      !toc_stack_.top().has_list_) {
    toc_stack_.top().has_list_ = true;
    first_item = true;
  }

  std::string id;
  if (!top_level) {
    id = msg->get_xpath_path();
    std::replace(id.begin(), id.end(), '/', '.');
  } else {
    id = title;
    std::replace(id.begin(), id.end(), ' ', '.');
  }

  toc_stack_.emplace(toc_stream_, title, id, type, msg, first_item); 
  section_stack_.emplace(content_stream_, msg, 
              title, id, type, top_level, doc_type_);

  return true;
}

bool HTMLDocWriter::visit_message_exit(PbMessage* msg)
{
  toc_stack_.pop();
  section_stack_.pop();

  return true;
}

