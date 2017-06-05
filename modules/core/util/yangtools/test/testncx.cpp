/* STANDARD_RIFT_IO_COPYRIGHT */

/*!
 * @file testncx.cpp
 * @brief Test YangModel and libncx
 *
 * Unit tests for yangncx.cpp
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <random>

#include "rwtrace.h"
#include "rwut.h"
#include "yangncx.hpp"

using namespace rw_yang;


TEST(YangNcx, CreateDestroy)
{
  TEST_DESCRIPTION("Creates and destroy a YangModelNcx");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
}

TEST(YangNcx, LogLevel)
{
  TEST_DESCRIPTION("Test log level modifications");

  rwtrace_ctx_t * rwctx = rwtrace_init();
  rwtrace_ctx_category_severity_set(rwctx, RWTRACE_CATEGORY_RWTASKLET, RWTRACE_SEVERITY_DEBUG);
  rwtrace_ctx_category_destination_set(rwctx, RWTRACE_CATEGORY_RWTASKLET, RWTRACE_DESTINATION_CONSOLE);

  YangModelNcx* model = YangModelNcx::create_model(static_cast<void *>(rwctx));

  /* Test log level modifications */
  rw_yang_log_level_t orig_log_level = model->log_level();
  rw_yang_log_level_t new_log_level = RW_YANG_LOG_LEVEL_DEBUG2;
  if (orig_log_level == new_log_level)
      new_log_level = RW_YANG_LOG_LEVEL_DEBUG3;

  model->set_log_level(new_log_level);
  EXPECT_EQ(new_log_level, model->log_level());

  /* Now check to make sure we get output.  ncxmod_load_module logs that it's
   * attempting to load a module at level 2.  This means that stdout needs
   * to be redirected to a temporary file, load_module() run, stdout restored
   * and then the temporary file checked for the desired output.
   */
  int stdout_bkup = dup(STDOUT_FILENO);
  char temp_path[] = "/tmp/test-yang-ncx-XXXXXX";
  int fd = mkstemp(temp_path);
  FILE * fp = fdopen(fd, "rw");

  ASSERT_TRUE(fp);
  EXPECT_FALSE(fflush(stdout));
  EXPECT_TRUE(dup2(fileno(fp), STDOUT_FILENO));

  YangModule* test_ncx = model->load_module("testncx");

  EXPECT_FALSE(fflush(stdout));
  EXPECT_TRUE(dup2(stdout_bkup, STDOUT_FILENO));

  EXPECT_FALSE(close(stdout_bkup));
  EXPECT_TRUE(test_ncx);

  bool found_expected = false;
  char * line = NULL;
  size_t len = 0;

  rewind(fp);
  while(getline(&line, &len, fp) > 0) {
    std::string buf(line);
    if (buf.find("Attempting to load module")) {
      found_expected = true;
      break;
    }
  }

  EXPECT_FALSE(fclose(fp));
  EXPECT_FALSE(unlink(temp_path));
  EXPECT_TRUE(found_expected);

  rwtrace_ctx_close(rwctx);
}

TEST(YangNcx, LoadTestNcx)
{
  TEST_DESCRIPTION("Loads a base module that was already imported by a previously loaded module");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("testncx");
  EXPECT_TRUE(test_ncx);

  YangModule* test_ncx_base = model->load_module("testncx-base");
  EXPECT_TRUE(test_ncx_base);

  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
}

TEST(YangNcx, BasicModel)
{
  TEST_DESCRIPTION("Basic model tests");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("testncx");
  EXPECT_TRUE(test_ncx);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);
  EXPECT_STREQ("data",root->get_name());
  EXPECT_FALSE(root->is_leafy());
  EXPECT_FALSE(root->get_type());

  YangNode* top = root->get_first_child();
  ASSERT_TRUE(top);
  EXPECT_EQ(top->get_parent(),root);
  EXPECT_STREQ("top",top->get_name());
  EXPECT_EQ(top,root->get_first_child());
  EXPECT_FALSE(top->is_leafy());
  EXPECT_FALSE(top->get_type());

  YangNode* top_sib = top->get_next_sibling();
  EXPECT_FALSE(top_sib);

  YangNode* a = top->get_first_child();
  ASSERT_TRUE(a);
  EXPECT_EQ(a->get_parent(),top);
  EXPECT_STREQ("a",a->get_name());
  EXPECT_EQ(a,top->get_first_child());
  EXPECT_FALSE(a->is_leafy());
  EXPECT_FALSE(a->get_type());

  YangNode* cont_in_a = a->get_first_child();
  ASSERT_TRUE(cont_in_a);
  EXPECT_EQ(cont_in_a->get_parent(),a);
  EXPECT_STREQ("cont-in-a",cont_in_a->get_name());
  EXPECT_EQ(cont_in_a,a->get_first_child());
  EXPECT_FALSE(cont_in_a->is_leafy());
  EXPECT_FALSE(cont_in_a->get_type());

  YangNode* str = cont_in_a->get_first_child();
  ASSERT_TRUE(str);
  YangNode* num1 = str->get_next_sibling();
  ASSERT_TRUE(num1);
  YangNode* ll = num1->get_next_sibling();
  ASSERT_TRUE(ll);
  YangNode* lst = ll->get_next_sibling();
  ASSERT_TRUE(lst);
  YangKey* lst_key = lst->get_first_key();
  ASSERT_TRUE(lst_key);
  EXPECT_FALSE(lst_key->get_next_key());
  EXPECT_EQ(lst_key->get_list_node(),lst);
  EXPECT_EQ (lst_key->get_key_node(), lst->get_key_at_index(1));
  EXPECT_FALSE (lst->get_key_at_index(2));
  YangNode* num2 = lst->get_first_child();
  ASSERT_TRUE(num2);
  EXPECT_FALSE(num2->is_key());
  YangNode* str2 = num2->get_next_sibling();
  ASSERT_TRUE(str2);
  EXPECT_EQ(str2,lst_key->get_key_node());
  EXPECT_TRUE(str2->is_key());
  YangKeyIter ki1(lst);
  YangKeyIter ki2(lst_key);
  YangKeyIter ki3;
  EXPECT_EQ(ki1,ki2);
  EXPECT_NE(ki1,ki3);
  ++ki1;
  EXPECT_EQ(ki1,ki3);
  ki2++;
  EXPECT_EQ(ki2,ki3);

  YangNode* a1 = a->get_next_sibling();
  ASSERT_TRUE(a1);
  EXPECT_EQ(a1->get_parent(),top);
  EXPECT_STREQ("a1",a1->get_name());
  EXPECT_EQ(a1,a->get_next_sibling());
  EXPECT_FALSE(a1->is_leafy());
  EXPECT_FALSE(a1->get_type());

  YangNode* b = a1->get_next_sibling();
  ASSERT_TRUE(b);
  EXPECT_EQ(b->get_parent(),top);
  EXPECT_STREQ("b",b->get_name());
  EXPECT_EQ(b,a1->get_next_sibling());
  EXPECT_FALSE(b->is_leafy());
  EXPECT_FALSE(b->get_type());

  YangNode* c = b->get_next_sibling();
  ASSERT_TRUE(c);
  EXPECT_EQ(c->get_parent(),top);
  EXPECT_STREQ("c",c->get_name());
  EXPECT_EQ(c,b->get_next_sibling());
  EXPECT_FALSE(c->is_leafy());
  EXPECT_FALSE(c->get_type());

  YangNode* c_c = c->get_first_child();
  ASSERT_TRUE(c_c);
  EXPECT_STREQ("c",c_c->get_name());
  EXPECT_EQ(c_c,c->get_first_child());
  EXPECT_FALSE(c_c->get_first_child());
  EXPECT_FALSE(c_c->get_next_sibling());
  EXPECT_TRUE(c_c->is_leafy());
  EXPECT_TRUE(c_c->get_type());

  YangNode* d = c->get_next_sibling();
  ASSERT_TRUE(d);
  EXPECT_EQ(d->get_parent(),top);
  EXPECT_STREQ("d",d->get_name());
  EXPECT_EQ(d,c->get_next_sibling());
  EXPECT_FALSE(d->is_leafy());
  EXPECT_FALSE(d->get_type());

  YangNode* e = d->get_next_sibling();
  ASSERT_TRUE(e);
  EXPECT_STREQ("e",e->get_name());
  EXPECT_EQ(e,d->get_next_sibling());
  EXPECT_TRUE(e->get_next_sibling());
  EXPECT_FALSE(e->is_leafy());
  EXPECT_FALSE(e->get_type());

  YangNode* b_n1 = b->get_first_child();
  ASSERT_TRUE (b_n1);
  ASSERT_TRUE (b_n1->is_leafy());
  EXPECT_STREQ ("str3", b_n1->get_name());

  ASSERT_TRUE (nullptr == b_n1->get_case());
  ASSERT_TRUE (nullptr == b_n1->get_choice());

  b_n1 = b_n1->get_next_sibling();
  ASSERT_TRUE (b_n1);
  ASSERT_TRUE (b_n1->is_leafy());
  EXPECT_STREQ ("num3", b_n1->get_name());

  ASSERT_TRUE (nullptr == b_n1->get_case());
  ASSERT_TRUE (nullptr == b_n1->get_choice());

  b_n1 = b_n1->get_next_sibling();
  ASSERT_TRUE (b_n1);
  ASSERT_TRUE (b_n1->is_leafy());
  EXPECT_STREQ ("ch1-1", b_n1->get_name());

  YangNode *choice = b_n1->get_choice();
  ASSERT_TRUE (choice);
  YangNode *ycase = b_n1->get_case();
  ASSERT_TRUE (ycase);

  EXPECT_STREQ ("c1", ycase->get_name());
  //EXPECT_STREQ ("Comes with many cases", ycase->get_description());
  EXPECT_STREQ ("http://riftio.com/ns/core/util/yangtools/tests/testncx",
                ycase->get_ns());
  EXPECT_TRUE (ycase->get_location().c_str()); // Dont really care whats in there
  EXPECT_STREQ ("tn", ycase->get_prefix());
  ASSERT_EQ (ycase->get_choice(), choice);
  EXPECT_STREQ ("ch", choice->get_name());


  EXPECT_TRUE (choice->get_location().c_str()); // Dont really care whats in there

  // Move over to the next sibling within the case

  YangNode* b_n2 = b_n1->get_next_sibling();
  ASSERT_TRUE (b_n2);
  ASSERT_EQ (b_n2->get_case(), ycase);
  ASSERT_EQ (b_n2->get_choice(), choice);

  // Siblings in a case do not conflict
  ASSERT_TRUE (!b_n1->is_conflicting_node(b_n2));

  // Get to the next case
  YangNode *b_n3 = b_n2->get_next_sibling();
  ASSERT_TRUE (b_n3);
  ASSERT_EQ (b_n3->get_choice(), choice);
  ASSERT_TRUE (b_n3->get_case()!= ycase);


  //b_n3 coflicts with both b_n1 and b_n2
  EXPECT_TRUE (b_n1->is_conflicting_node (b_n3));
  EXPECT_TRUE (b_n3->is_conflicting_node (b_n2));

  // b_n4 and b_n5 are choice children of the next sibling
  YangNode *b_n4 = b_n3->get_next_sibling();
  ASSERT_STREQ ("ch2-3", b_n4->get_name());
  EXPECT_TRUE (b_n1->is_conflicting_node (b_n4));

  YangNode *b_n5 = b_n4->get_next_sibling();
  ASSERT_STREQ ("ch2-2-1", b_n5->get_name());

  YangNode *b_n6 = b_n5->get_next_sibling();
  ASSERT_STREQ ("ch2-2-2", b_n6->get_name());
  EXPECT_TRUE (b_n6->is_conflicting_node (b_n5));
  EXPECT_TRUE (!b_n6->is_conflicting_node (b_n3));
  EXPECT_TRUE (!b_n6->is_conflicting_node (b_n4));
  EXPECT_TRUE (b_n6->is_conflicting_node (b_n1));

  YangNode *b_n7 = b_n6->get_next_sibling();
  ASSERT_STREQ ("ch3-1", b_n7->get_name());
  EXPECT_TRUE (b_n1->is_conflicting_node (b_n7));
  EXPECT_TRUE (b_n7->is_conflicting_node (b_n6));
  EXPECT_TRUE (b_n7->is_conflicting_node (b_n4));

  YangNode *b_n8 = b_n7->get_first_child();
  ASSERT_STREQ ("ch2-2-1", b_n8->get_name());
  EXPECT_TRUE (!b_n8->is_conflicting_node (b_n7));

  // The next cases do actually conflict, but the API is not written to
  // catch conflicts accross different levels of choice roots.
  EXPECT_TRUE (!b_n6->is_conflicting_node (b_n8));
  EXPECT_TRUE (!b_n4->is_conflicting_node (b_n8));
  EXPECT_TRUE (!b_n8->is_conflicting_node (b_n1));

  YangNode *b_n9 = b_n8->get_next_sibling();
  ASSERT_STREQ ("ch2-2-2", b_n9->get_name());

  YangNode *b_n10 = b_n9->get_next_sibling();
  ASSERT_STREQ ("ch3-l1", b_n10->get_name());

  YangNode* d_n1 = d->get_first_child();
  YangNode* e_n1 = e->get_first_child();
  ASSERT_TRUE(d_n1);
  ASSERT_TRUE(e_n1);

  ASSERT_TRUE(d_n1->is_leafy());
  ASSERT_TRUE(e_n1->is_leafy());
  EXPECT_STREQ(d_n1->get_name(),e_n1->get_name());
  EXPECT_EQ(d_n1->get_name(),e_n1->get_name());
  ASSERT_TRUE(d_n1->get_type());
  ASSERT_TRUE(e_n1->get_type());
  EXPECT_EQ(d_n1->get_type(),e_n1->get_type());

  YangNodeIter i = e->child_begin();
  ASSERT_STREQ("n1",i->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());

  // Validation between 128 and 140
  YangValue* v = nullptr;
  EXPECT_FALSE(i->get_type()->parse_value("-12", &v));
  EXPECT_FALSE(i->get_type()->parse_value("14", &v));
  EXPECT_TRUE(i->get_type()->parse_value("-13", &v));
  EXPECT_TRUE(i->get_type()->parse_value("15", &v));

  ASSERT_STREQ("n2",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_EQ(i->get_parent(),e);

  // Valiadation  {range "100..200|300|400..500";}
  EXPECT_TRUE(i->get_type()->parse_value("201", &v));
  EXPECT_TRUE(i->get_type()->parse_value("299", &v));
  EXPECT_FALSE(i->get_type()->parse_value("300", &v));
  EXPECT_TRUE(i->get_type()->parse_value("301", &v));
  EXPECT_TRUE(i->get_type()->parse_value("399", &v));
  EXPECT_EQ(i->get_parent(),e);

  ASSERT_STREQ("n2",(i++)->get_name());
  EXPECT_STREQ("n3",i->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_EQ(i->get_parent(),e);
  YangNodeIter j = i;
  EXPECT_EQ(i,j);
  EXPECT_EQ(i->get_parent(),j->get_parent());
  EXPECT_STREQ("n4",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_NE(i,j);
  EXPECT_STREQ("n3",(j++)->get_name());
  EXPECT_EQ(i,j);
  EXPECT_STREQ("n4",(*i).get_name());
  EXPECT_NE(i,YangNodeIter());
  EXPECT_STREQ("n5",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_STREQ("n6",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_STREQ("n7",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_STREQ("n8",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_STREQ("iref1",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());

  EXPECT_STREQ("bin1",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());

  // Validations - Binary is string componsed from [a-zA-z0-9+/]*
  EXPECT_FALSE(i->get_type()->parse_value("Ab0+/", &v));
  EXPECT_TRUE(i->get_type()->parse_value("Ab0+-", &v));
  EXPECT_TRUE(i->get_type()->parse_value("Ab@", &v));
  // Length validation 2 - 5 for binary, the ascii string length is 3 to 10
  EXPECT_TRUE(i->get_type()->parse_value("Ab0De1Fg2Hi", &v));
  EXPECT_TRUE(i->get_type()->parse_value("A", &v));

  EXPECT_STREQ("bits1",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());

  //Validation - Only a - d are valid
  EXPECT_FALSE(i->get_type()->parse_value("a", &v));
  EXPECT_FALSE(i->get_type()->parse_value("d", &v));
  EXPECT_TRUE(i->get_type()->parse_value("e", &v));
  EXPECT_TRUE(i->get_type()->parse_value("A", &v));
  EXPECT_TRUE(i->get_type()->parse_value("A", &v));

  EXPECT_STREQ("bool1",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());

  // Validation - true, false, 0, 1 are valid
  EXPECT_FALSE(i->get_type()->parse_value("true", &v));
  EXPECT_FALSE(i->get_type()->parse_value("false", &v));
  EXPECT_FALSE(i->get_type()->parse_value("0", &v));
  EXPECT_FALSE(i->get_type()->parse_value("1", &v));
  EXPECT_TRUE(i->get_type()->parse_value("T", &v));
  EXPECT_TRUE(i->get_type()->parse_value("2", &v));

  EXPECT_STREQ("dec1",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_STREQ("e1",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_TRUE(i->get_type()->parse_value("2", &v));
  EXPECT_STREQ("s1",(++i)->get_name());
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  // leaf s1 { type string{length "2..5";} }
  EXPECT_TRUE(i->get_type()->parse_value("2", &v));
  EXPECT_TRUE(i->get_type()->parse_value("abcdef", &v));
  EXPECT_FALSE(i->get_type()->parse_value("abcd", &v));
  // deleted due to confd: EXPECT_STREQ("axml",(++i)->get_name());
  // deleted due to confd: EXPECT_TRUE(i->is_leafy());
  // deleted due to confd: EXPECT_TRUE(i->get_type());
  // deleted due to confd: EXPECT_EQ(i->get_parent(),e);
  EXPECT_STREQ("u1",(++i)->get_name());
  YangNodeIter u1 = i;
  EXPECT_TRUE(i->is_leafy());
  EXPECT_TRUE(i->get_type());
  EXPECT_EQ(i->get_parent(),e);
  // String of length 2 pattern p-r*, characters a..d and most numbers -10000000..10000000
  EXPECT_FALSE(i->get_type()->parse_value("2", &v));
  EXPECT_FALSE(i->get_type()->parse_value("a", &v));
  EXPECT_TRUE(i->get_type()->parse_value("e", &v));
  EXPECT_TRUE(i->get_type()->parse_value("A", &v));
  EXPECT_TRUE(i->get_type()->parse_value("AA", &v));
  EXPECT_TRUE(i->get_type()->parse_value("PP", &v));
  EXPECT_FALSE(i->get_type()->parse_value("qq", &v));
  EXPECT_TRUE(i->get_type()->parse_value("abcdef", &v));
  EXPECT_TRUE(i->get_type()->parse_value("abcd", &v));
  EXPECT_TRUE(i->get_type()->parse_value("10000001", &v));
  EXPECT_TRUE(i->get_type()->parse_value("-10000001", &v));
  EXPECT_FALSE(i->get_type()->parse_value("10000000", &v));
  EXPECT_FALSE(i->get_type()->parse_value("-10000000", &v));
  YangNodeIter k;
  ++i;
  EXPECT_EQ(i,k);

  YangValueIter u1vi(u1);
  EXPECT_FALSE((u1vi++)->is_keyword());
  EXPECT_TRUE((u1vi++)->is_keyword());
  EXPECT_TRUE((u1vi++)->is_keyword());
  EXPECT_TRUE((u1vi++)->is_keyword());
  EXPECT_TRUE((u1vi++)->is_keyword());
  EXPECT_TRUE((u1vi++)->is_keyword());
  EXPECT_TRUE((u1vi++)->is_keyword());
  EXPECT_FALSE((u1vi++)->is_keyword());
  EXPECT_FALSE((u1vi++)->is_keyword());
  EXPECT_FALSE((u1vi++)->is_keyword());
  EXPECT_EQ(u1vi,YangValueIter());

  YangNode* f = e->get_next_sibling();
  EXPECT_STREQ("f",f->get_name());
  EXPECT_EQ(f,e->get_next_sibling());
  EXPECT_FALSE(f->is_leafy());
  EXPECT_TRUE(f->get_next_sibling());
  EXPECT_FALSE(f->get_type());

  YangNode* w_ns = f->get_first_child();
  ASSERT_TRUE(w_ns);
  YangNode* wo_ns = w_ns->get_next_sibling();
  ASSERT_TRUE(wo_ns);

  YangNode* g = f->get_next_sibling();
  ASSERT_STREQ("g",g->get_name());
  YangNode* ge1 = g->get_first_child();
  ASSERT_TRUE(ge1);
  YangNode* ge2 = ge1->get_next_sibling();
  ASSERT_TRUE(ge2);
  YangNode* ge3 = ge2->get_next_sibling();
  ASSERT_TRUE(ge3);
  YangNode* ge4 = ge3->get_next_sibling();
  ASSERT_TRUE(ge4);
  YangValueIter ge1vi(ge1);
  YangValueIter ge2vi(ge2);
  YangValueIter ge3vi(ge3);
  while (ge1vi != YangValueIter()) {
    EXPECT_STREQ(ge1vi->get_name(),ge2vi->get_name());
    EXPECT_STREQ(ge1vi->get_name(),ge3vi->get_name());
    EXPECT_TRUE(ge1vi->is_keyword());
    ge1vi++;
    ++ge2vi;
    ge3vi++;
  }
  YangValueIter vi(ge1);
  EXPECT_STREQ((vi++)->get_name(),"TNB_E_A");
  EXPECT_STREQ((vi++)->get_name(),"TNB_E_B");
  EXPECT_STREQ((vi++)->get_name(),"TNB_E_C");
  EXPECT_STREQ((vi++)->get_name(),"TNB_E_D");
  EXPECT_STREQ((vi++)->get_name(),"TNB_E_E");
  EXPECT_EQ(vi,YangValueIter());

  YangNode* show = g->get_next_sibling();
  YangNode *counters = show->get_first_child()->get_first_child();
  EXPECT_STREQ("counters",counters->get_name());
  EXPECT_FALSE(counters->is_leafy());
  EXPECT_FALSE(counters->get_type());

  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
}


TEST(YangNcx, YangNcxExt)
{
  TEST_DESCRIPTION("Test extensions and extension iterators");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* test_ncx_ext = model->load_module("testncx-ext");
  EXPECT_TRUE(test_ncx_ext);

  YangNode* root = model->get_root_node();

  YangNode* top1 = root->get_first_child();
  ASSERT_TRUE(top1);
  EXPECT_STREQ("top1",top1->get_name());

  YangNode* top1cina = top1->get_first_child();
  ASSERT_TRUE(top1cina);
  EXPECT_STREQ("cina",top1cina->get_name());

  YangNode* top1lina = top1cina->get_next_sibling();
  ASSERT_TRUE(top1lina);
  EXPECT_STREQ("lina",top1lina->get_name());

  YangNode* top1lf5 = top1lina->get_next_sibling();
  ASSERT_TRUE(top1lf5);
  EXPECT_STREQ("lf5",top1lf5->get_name());

  YangNode* top1cint1 = top1lf5->get_next_sibling();
  ASSERT_TRUE(top1cint1);
  EXPECT_STREQ("cint1",top1cint1->get_name());

  YangNode* lf1 = top1cint1->get_first_child();
  ASSERT_TRUE(lf1);
  EXPECT_STREQ("lf1",lf1->get_name());

  YangExtensionIter ei(top1);
  ASSERT_NE(ei,YangExtensionIter());
  EXPECT_STREQ(ei->get_name(),"ext-1");
  EXPECT_STREQ(ei->get_value(),"ext 1 in top1");
  EXPECT_EQ(++ei,YangExtensionIter());

  ei = top1cina->extension_begin();
  ASSERT_NE(ei,YangExtensionIter());
  EXPECT_STREQ(ei->get_value(),"ext 1 in a-group:cina");
  ASSERT_NE(++ei,YangExtensionIter());
  EXPECT_STREQ(ei->get_name(),"ext-a");
  const char *ns = "http://riftio.com/ns/core/util/yangtools/tests/testncx-ext";
  EXPECT_EQ(ei,YangExtensionIter(top1cina->extension_begin()->search(ns,"ext-a")));
  ASSERT_NE(++ei,YangExtensionIter());
  EXPECT_STREQ(ei->get_value(),"ext 1 in a-group");
  EXPECT_EQ(++ei,YangExtensionIter());

  ei = lf1->extension_begin();
  ASSERT_NE(ei,YangExtensionIter());
  EXPECT_STREQ(ei->get_value(),"ext b in lf1, not group-a");
  EXPECT_EQ(++ei,YangExtensionIter());



  YangNode* top2 = top1->get_next_sibling();
  ASSERT_TRUE(top2);
  EXPECT_STREQ("top2",top2->get_name());



  YangNode* top3 = top2->get_next_sibling();
  ASSERT_TRUE(top3);
  EXPECT_STREQ("top3",top3->get_name());

  YangNode* top3lf8 = top3->get_first_child();
  ASSERT_TRUE(top3lf8);
  EXPECT_STREQ("lf8",top3lf8->get_name());

  ei = top3lf8->extension_begin();
  ASSERT_NE(ei,YangExtensionIter());
  EXPECT_STREQ(ei->get_name(),"ext-a");
  EXPECT_EQ(++ei,YangExtensionIter());

  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
}


TEST(YangNcx, YangNcxModule)
{
  TEST_DESCRIPTION("Test modules and module import");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* top2 = model->load_module("testncx-mod-top2");
  EXPECT_TRUE(top2);

  YangNode* root = model->get_root_node();
  YangNodeIter tni = root->child_begin();

  YangNode* t2c1 = &*(tni++);
  ASSERT_TRUE(t2c1);
  EXPECT_STREQ("t2c1",t2c1->get_name());

  YangNode* t2c2 = &*(tni++);
  ASSERT_TRUE(t2c2);
  EXPECT_STREQ("t2c2",t2c2->get_name());

  EXPECT_EQ(tni,YangNodeIter());


  YangModule* top1 = model->load_module("testncx-mod-top1");
  EXPECT_TRUE(top1);
  tni = root->child_begin();
  /* It appears that libncx sorts the module list alphabetically. */

  EXPECT_STREQ("t1c1",tni->get_name());
  YangNode* t1c1 = &*(tni++);
  ASSERT_TRUE(t1c1);

  EXPECT_STREQ("t1c2",tni->get_name());
  YangNode* t1c2 = &*(tni++);
  ASSERT_TRUE(t1c2);

  EXPECT_STREQ("t2c1",tni->get_name());
  EXPECT_EQ(t2c1,&*(tni++));
  EXPECT_STREQ("t2c2",tni->get_name());
  EXPECT_EQ(t2c2,&*(tni++));
  EXPECT_EQ(tni,YangNodeIter());


  YangModuleIter mi = model->module_begin();
  ASSERT_NE(mi,YangModuleIter());
  ASSERT_NE(mi,model->module_end());
  ASSERT_STREQ("testncx-mod-base1",mi->get_name());
  EXPECT_STREQ("testncx-mod-base2",(++mi)->get_name());
  EXPECT_STREQ("testncx-mod-base3",(++mi)->get_name());
  EXPECT_STREQ("testncx-mod-base4",(++mi)->get_name());
  YangModuleIter b4i = mi;
  ASSERT_STREQ("testncx-mod-top1",(++mi)->get_name());
  EXPECT_STREQ("testncx-mod-top2",(++mi)->get_name());
  YangModuleIter t2i = mi;
  ASSERT_STREQ("yuma-ncx",(++mi)->get_name());


  YangModule* top3 = model->load_module("testncx-mod-top3");
  EXPECT_TRUE(top3);

  EXPECT_STREQ("testncx-mod-top2",t2i->get_name());
  EXPECT_STREQ("testncx-mod-top3",(++t2i)->get_name());
  EXPECT_STREQ("yuma-ncx",(++t2i)->get_name());
  EXPECT_STREQ("testncx-mod-base4",b4i->get_name());
  EXPECT_STREQ("testncx-mod-base5",(++b4i)->get_name());
  EXPECT_STREQ("testncx-mod-top1",(++b4i)->get_name());
  EXPECT_STREQ("testncx-mod-top2",(++b4i)->get_name());
  EXPECT_STREQ("testncx-mod-top3",(++b4i)->get_name());
  EXPECT_STREQ("yuma-ncx",(++b4i)->get_name());

  EXPECT_EQ(b4i,t2i);
  EXPECT_EQ(b4i,mi);
  EXPECT_EQ(t2i,mi);

  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
}

static void check_rw_types(YangNode* node)
{
  static const struct {
    const char* name;
    const char* value;
  }
  tests[] =
  {
    { "ip-version", "ipv4" },
    { "dscp", "55" },
    { "ipv6-flow-label", "123456" },
    { "port-number", "12345" },
    { "as-number", "12345678" },
    { "ip-address", "192.168.1.1%12" },
    { "ipv4-address", "10.10.1.1%12" },
    { "ipv6-address", "1234::1234%12" },
    { "ip-address-no-zone", "10.10.1.1" },
    { "ipv4-address-no-zone", "192.168.1.1" },
    { "ipv6-address-no-zone", "1234::1234" },
    { "ip-prefix", "1234::0000/16" },
    { "ipv4-prefix", "192.168.1.1/24" },
    { "ipv6-prefix", "1234::abcd/24" },
    { "domain-name", "riftio.com" },
    { "host", "boson.riftio.com" },
    { "uri", "http://www.riftio.com/index.html" },
    { "counter32", "2134325" },
    { "zero-based-counter32", "0" },
    { "counter64", "12345678901234" },
    { "zero-based-counter64", "0" },
    { "gauge32", "213466" },
    { "gauge64", "123456789012345" },
    { "object-identifier", "0.1.255.1024.4.5" },
    { "object-identifier-128", "0.1.255.1024.4.5.2345" },
    { "yang-identifier", "dots.and-dashes" },
    { "date-and-time", "2014-04-01T18:43:13.284637-05:00" },
    { "timeticks", "46378435" },
    { "timestamp", "543785634" },
    { "phys-address", "aa:bb:cc:dd:ee:ff:00:11:22:33" },
    { "mac-address", "00:1b:21:b9:8a:c8" },
    { "xpath1.0", "/tcont/xpath1.0" },
    { "hex-string", "ab:cd:ef:10:23:45:67:89:00" },
    { "uuid", "0123acfd-1234-abcd-ef01-1234567809ab" },
    { "dotted-quad", "0.0.0.0" },
    { NULL, NULL }
  };

  for (auto p = &tests[0]; p->name; ++p) {
    YangNode* yn = node->search_child(p->name);
    ASSERT_TRUE(yn);
    YangType* yt = yn->get_type();
    ASSERT_TRUE(yt);
    YangValue* yv = NULL;
    rw_status_t rs = yt->parse_value(p->value, &yv);
    EXPECT_EQ(rs, RW_STATUS_SUCCESS);
    if (RW_STATUS_SUCCESS != rs) {
      std::cout << "parse of " << p->name << " failed for value " << p->value << std::endl;
    }
  }
}

TEST(YangNcx, RwTypesYang)
{
  TEST_DESCRIPTION("Test rw-yang-types.yang");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* trwt = model->load_module("test-rw-yang-types");
  EXPECT_TRUE(trwt);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);
  YangNode* tuses = root->search_child("tuses");
  ASSERT_TRUE(tuses);
  check_rw_types(tuses);
  YangNode* tcont = root->search_child("tcont");
  ASSERT_TRUE(tcont);
  check_rw_types(tcont);
  YangNode* ttypes = root->search_child("ttypes");
  ASSERT_TRUE(ttypes);
  check_rw_types(ttypes);
}

TEST(YangNcx, IDRefCase1)
{
  TEST_DESCRIPTION("Test Identity Reference Case 1");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* id_top1 = model->load_module("testncx-idref-top1");
  EXPECT_TRUE(id_top1);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);
  EXPECT_STREQ("data",root->get_name());
  EXPECT_FALSE(root->is_leafy());
  EXPECT_FALSE(root->get_type());

  YangNode* top = root->get_first_child();
  ASSERT_TRUE(top);
  EXPECT_EQ(top->get_parent(),root);
  EXPECT_STREQ("language-name",top->get_name());
  EXPECT_EQ(top,root->get_first_child());
  EXPECT_TRUE(top->is_leafy());
  EXPECT_TRUE(top->get_type());

  YangValueIter i(top);
  EXPECT_NE(i,YangValueIter());
  ASSERT_STREQ("english",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("telugu",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("french",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("german",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  EXPECT_EQ(i,YangValueIter());

  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
}

TEST(YangNcx, IDRefCase2)
{
  TEST_DESCRIPTION("Test Identity Reference Case 2");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* id_top2 = model->load_module("testncx-idref-top2");
  EXPECT_TRUE(id_top2);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);
  EXPECT_STREQ("data",root->get_name());
  EXPECT_FALSE(root->is_leafy());
  EXPECT_FALSE(root->get_type());

  YangNode* top = root->get_first_child();
  ASSERT_TRUE(top);
  EXPECT_EQ(top->get_parent(),root);
  EXPECT_STREQ("language-name",top->get_name());
  EXPECT_EQ(top,root->get_first_child());
  EXPECT_TRUE(top->is_leafy());
  EXPECT_TRUE(top->get_type());

  YangValueIter i(top);
  EXPECT_NE(i,YangValueIter());
  ASSERT_STREQ("english",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("french",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("malayalam",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  EXPECT_EQ(i,YangValueIter());

  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
}

TEST(YangNcx, IDRefCase3)
{
  TEST_DESCRIPTION("Test Identity Reference Case 3");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  YangModule* id_top1 = model->load_module("testncx-idref-top1");
  EXPECT_TRUE(id_top1);

  YangModule* id_top2 = model->load_module("testncx-idref-top2");
  EXPECT_TRUE(id_top2);

  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);
  EXPECT_STREQ("data",root->get_name());
  EXPECT_FALSE(root->is_leafy());
  EXPECT_FALSE(root->get_type());

  YangNode* top = root->get_first_child();
  ASSERT_TRUE(top);
  EXPECT_EQ(top->get_parent(),root);
  EXPECT_STREQ("language-name",top->get_name());
  EXPECT_EQ(top,root->get_first_child());
  EXPECT_TRUE(top->is_leafy());
  EXPECT_TRUE(top->get_type());

  YangValueIter i(top);
  EXPECT_NE(i,YangValueIter());
  ASSERT_STREQ("english",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("telugu",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("french",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("german",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("french",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  ASSERT_STREQ("malayalam",i->get_name());
  EXPECT_TRUE((i++)->is_keyword());
  EXPECT_EQ(i,YangValueIter());

  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
}

TEST(YangNcx, Augment1)
{
  TEST_DESCRIPTION("Extended augment tests #1");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);
  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);

  YangModule* tnaa1 = model->load_module("test-augment-a1");
  ASSERT_TRUE(tnaa1);
  EXPECT_FALSE(root->get_first_child());
  YangAugment* tnab1a1_aug_b1_c1 = tnaa1->get_first_augment();
  EXPECT_FALSE(root->get_first_child());
  ASSERT_TRUE(tnab1a1_aug_b1_c1);
  YangNode* a1_b1_c1 = tnab1a1_aug_b1_c1->get_target_node();
  EXPECT_STREQ(a1_b1_c1->get_name(),"b1_c1");

  YangAugment* tnab1a1_aug_b1_c = tnab1a1_aug_b1_c1->get_next_augment();
  ASSERT_TRUE(tnab1a1_aug_b1_c);
  YangNode* a1_b1_c = tnab1a1_aug_b1_c->get_target_node();
  EXPECT_STREQ(a1_b1_c->get_name(),"b1_c");

  YangAugment* tnab1a1_aug_b1_c2 = tnab1a1_aug_b1_c->get_next_augment();
  ASSERT_TRUE(tnab1a1_aug_b1_c2);
  YangNode* a1_b1_c2 = tnab1a1_aug_b1_c2->get_target_node();
  EXPECT_STREQ(a1_b1_c2->get_name(),"b1_c2");
  EXPECT_TRUE(tnab1a1_aug_b1_c2->get_next_augment());

  std::cout << std::endl << "Model before loading base:" << std::endl;
  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);

  std::cout << std::endl << "Augmented b1_c1's tree:" << std::endl;
  rw_yang_node_dump_tree(a1_b1_c1, 2/*indent*/, 1/*verbosity*/);


  // Now make the augmented nodes directly visible in the model by loading their module
  YangModule* tnab1 = model->load_module("test-augment-b1");
  ASSERT_TRUE(tnab1);

  YangNode* b1_c = root->get_first_child();
  ASSERT_TRUE(b1_c);
  EXPECT_EQ(b1_c,a1_b1_c);

  std::cout << std::endl << "Model after loading base:" << std::endl;
  rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
}

TEST(YangNcx, ReusableGrouping)
{
  TEST_DESCRIPTION("Reusable Grouping Test #1");
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);
  YangNode* root = model->get_root_node();
  ASSERT_TRUE(root);
  YangModule* ymod = model->load_module("other-rwmanifest");

  static const char* rwvcs_variable_list_nodes[] = {
    "variable"
  };
  static const char* rwvcs_sequence_nodes[] = {
    "action"
  };
  static const char* rwvcs_event_list_nodes[] = {
    "event-list"
  };
  static const char* rwvcs_zookeeper_nodes[] = {
    "zookeeper"
  };
  static const char* rwvcs_rwcolony_nodes[] = {
    "rwcolony"
  };
  static const char* rwvcs_rwvm_nodes[] = {
    "rwvm"
  };
  static const char* rwvcs_component_list_nodes[] = {
    "component"
  };
  static const char* bootstrap_phase_nodes[] = {
    "bootstrap-phase"
  };
  static const char* init_phase_nodes[] = {
    "init-phase"
  };
  static const char* multi_node_test_nodes[] = {
    "node1",
    "node2"
  };
  static const char* leaf_test_nodes[] = {
    "leaf1"
  };

  typedef struct grp_s {
    const char * grp_name;
    const char ** nodes;
    int num_nodes;
  } grp_t;

  static grp_t top_groupings[] = {
    {"rwvcs-variable-list", rwvcs_variable_list_nodes, sizeof(rwvcs_variable_list_nodes)/sizeof(char*)},
    {"rwvcs-sequence", rwvcs_sequence_nodes, sizeof(rwvcs_sequence_nodes)/sizeof(char*)},
    {"rwvcs-event-list", rwvcs_event_list_nodes, sizeof(rwvcs_event_list_nodes)/sizeof(char*)},
    {"rwvcs-zookeeper", rwvcs_zookeeper_nodes, sizeof(rwvcs_zookeeper_nodes)/sizeof(char*)},
    {"rwvcs-rwcolony", rwvcs_rwcolony_nodes, sizeof(rwvcs_rwcolony_nodes)/sizeof(char*)},
    {"rwvcs-rwvm", rwvcs_rwvm_nodes, sizeof(rwvcs_rwvm_nodes)/sizeof(char*)},
    {"rwvcs-component-list", rwvcs_component_list_nodes, sizeof(rwvcs_component_list_nodes)/sizeof(char*)},
    {"bootstrap-phase",bootstrap_phase_nodes, sizeof(bootstrap_phase_nodes)/sizeof(char*)},
    {"init-phase",init_phase_nodes, sizeof(init_phase_nodes)/sizeof(char*)},
    {"multi-node-test", multi_node_test_nodes, sizeof(multi_node_test_nodes)/sizeof(char*)},
    {"leaf-test", leaf_test_nodes, sizeof(leaf_test_nodes)/sizeof(char*)}
  };

  int no_of_top_groupings = sizeof(top_groupings)/sizeof(grp_t);
  int i = 0;

  typedef std::map<std::string, bool> name_map_t;
  name_map_t top_grp_map;


  for (i = 0; i < no_of_top_groupings; i++) {
    top_grp_map[top_groupings[i].grp_name] = false;
  }

  i = 0;

  for (auto git = ymod->grouping_begin();
           git !=  ymod->grouping_end();
           git++, i++) {
    EXPECT_EQ(top_grp_map[git->get_name()], false);
    top_grp_map[git->get_name()] = true;
    int j = 0;
    for (auto ch =  git->child_begin();
              ch != git->child_end();
              ch++, j++) {
      ASSERT_LT(i,no_of_top_groupings);
      ASSERT_LT(j, top_groupings[i].num_nodes);
      EXPECT_EQ(std::string(ch->get_name()), top_groupings[i].nodes[j]);
      if (RW_YANG_STMT_TYPE_CONTAINER == ch->get_stmt_type() ||
          RW_YANG_STMT_TYPE_LIST == ch->get_stmt_type())  {
        YangExtension* yext_msg_new = ch->search_extension(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_MSG_NEW);
        if (yext_msg_new) {
          std::cout << "Got extension " << yext_msg_new->get_name() << " for node " << ch->get_name() << std::endl;
        }
      }
    }
  }

  // Iterate over the MAP to see if the iterator walked over all the top level groupings
  for (i = 0; i < no_of_top_groupings; i++) {
    EXPECT_EQ(top_grp_map[top_groupings[i].grp_name], true);
  }

  // Iterate over the nodes to see if we can find the Uses and Groupings
  YangNode *config = root->get_first_child();
  ASSERT_NE(config, nullptr);
  EXPECT_EQ(std::string(config->get_name()), "config");

  YangNode *manifest = config->get_first_child();
  ASSERT_NE(manifest, nullptr);
  EXPECT_EQ(std::string(manifest->get_name()), "manifest");

  YangNode *profile_name = manifest->get_first_child();
  ASSERT_NE(profile_name, nullptr);
  EXPECT_EQ(std::string(profile_name->get_name()), "profile-name");

  YangNode *profile = profile_name->get_next_sibling();
  ASSERT_NE(profile, nullptr);
  EXPECT_EQ(std::string(profile->get_name()), "profile");

  for (auto ni = profile->child_begin(); ni != profile->child_end(); ni++) {
    YangUses* uses = ni->get_uses();
    if (uses) {
      YangNode *grp = uses->get_grouping();
      ASSERT_NE(grp, nullptr);
      std::cout << "Got uses and groupig uses[" << grp->get_name() << std::endl;
    }
  }
}

TEST(YangNcx, MultiModel)
{
  TEST_DESCRIPTION("Multiple model tests");

  YangModelNcx* model1 = YangModelNcx::create_model();
  YangModel::ptr_t p1(model1);
  ASSERT_TRUE(model1);

  YangModule* modtop1 = model1->load_module("testncx-mod-top1");
  EXPECT_TRUE(modtop1);

  YangModelNcx* model2 = YangModelNcx::create_model();
  YangModel::ptr_t p2(model2);
  ASSERT_TRUE(model2);

  YangModule* modtop2 = model2->load_module("testncx-mod-top2");
  EXPECT_TRUE(modtop2);

  YangNode* root1 = model1->get_root_node();
  ASSERT_TRUE(root1);
  YangNode* root1t1c1 = root1->search_child("t1c1",nullptr);
  YangNode* root1t1c2 = root1->search_child("t1c2",nullptr);
  YangNode* root1t2c1 = root1->search_child("t2c1",nullptr);
  YangNode* root1t2c2 = root1->search_child("t2c2",nullptr);
  EXPECT_TRUE(root1t1c1);
  EXPECT_TRUE(root1t1c2);
  EXPECT_FALSE(root1t2c1);
  EXPECT_FALSE(root1t2c2);

  YangNode* root2 = model2->get_root_node();
  ASSERT_TRUE(root2);
  YangNode* root2t1c1 = root2->search_child("t1c1",nullptr);
  YangNode* root2t1c2 = root2->search_child("t1c2",nullptr);
  YangNode* root2t2c1 = root2->search_child("t2c1",nullptr);
  YangNode* root2t2c2 = root2->search_child("t2c2",nullptr);
  EXPECT_FALSE(root2t1c1);
  EXPECT_FALSE(root2t1c2);
  EXPECT_TRUE(root2t2c1);
  EXPECT_TRUE(root2t2c2);
}

TEST(YangNcx, XmlSamples)
{
  TEST_DESCRIPTION("Test YangModel XML samples");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  auto test_ydom_top = model->load_module("test-ydom-top");
  ASSERT_TRUE(test_ydom_top);
  auto test_ydom_aug = model->load_module("test-ydom-aug");
  ASSERT_TRUE(test_ydom_aug);
  auto testy2p_top1 = model->load_module("testy2p-top1");
  ASSERT_TRUE(testy2p_top1);
  auto test_aug_choice = model->load_module("test-aug-choice");
  ASSERT_TRUE(test_aug_choice);
  auto testncx = model->load_module("testncx");
  ASSERT_TRUE(testncx);
  auto test_augment_a1 = model->load_module("test-augment-a1");
  ASSERT_TRUE(test_augment_a1);

  auto root = model->get_root_node();
  ASSERT_TRUE(root);

  auto ynode = test_ydom_top->search_node("top",test_ydom_top->get_ns());
  ASSERT_TRUE(ynode);

  auto xml1 = ynode->get_xml_sample_element( 2, 0, true, false );
  auto xml2 = ynode->get_xml_sample_element( 2, 0, true, true );
  EXPECT_EQ(xml1, xml2);
  EXPECT_NE(xml2, "");

  xml1 = xml2;
  xml2 = ynode->get_xml_sample_element( 2, 1, true, false );
  EXPECT_NE(xml1, xml2);
  EXPECT_NE(xml2, "");

  xml1 = xml2;
  xml2 = ynode->get_xml_sample_element( 0, 1, true, false );
  EXPECT_NE(xml1, xml2);
  EXPECT_NE(xml2, "");

  xml1 = xml2;
  xml2 = ynode->get_xml_sample_element( 0, 2, true, false );
  EXPECT_NE(xml1, xml2);
  EXPECT_NE(xml2, "");

  xml1 = xml2;
  xml2 = ynode->get_xml_sample_element( 0, 100, true, false );
  EXPECT_NE(xml1, xml2);
  EXPECT_NE(xml2, "");
  std::cout << xml2 << "\n\n";


  ynode = testncx->search_node("top",testncx->get_ns());
  ASSERT_TRUE(ynode);

  auto tnx_top_d = ynode->search_child("d");
  ASSERT_TRUE(tnx_top_d);

  xml1 = tnx_top_d->get_xml_sample_element( 0, 1, true, false );
  xml2 = tnx_top_d->get_xml_sample_element( 0, 1, false, false );
  EXPECT_NE(xml1, xml2);
  EXPECT_NE(xml1, "");
  EXPECT_NE(xml2, "");
  std::cout << xml2 << "\n\n";

  static const struct {
    const char* element;
    const char* string;
  } elems1[] = {
    { "n1",    "<tn:n1>0</tn:n1>" },
    { "n2",    "<tn:n2>0</tn:n2>" },
    { "n3",    "<tn:n3>0</tn:n3>" },
    { "n4",    "<tn:n4>0</tn:n4>" },
    { "n5",    "<tn:n5>0</tn:n5>" },
    { "n6",    "<tn:n6>0</tn:n6>" },
    { "n7",    "<tn:n7>0</tn:n7>" },
    { "n8",    "<tn:n8>0</tn:n8>" },
    { "iref1", "<tn:iref1>riftio</tn:iref1>" },
    { "bin1",  "<tn:bin1>BASE64+Encoded==</tn:bin1>" },
    { "bits1", "<tn:bits1>a b</tn:bits1>" },
    { "bool1", "<tn:bool1>true</tn:bool1>" },
    { "dec1",  "<tn:dec1>0.0</tn:dec1>" },
    { "e1",    "<tn:e1/>" },
    { "s1",    "<tn:s1>STRING</tn:s1>" },
    { "u1",    "<tn:u1>UNION_VALUE</tn:u1>" },
  };
  for (auto &e:elems1) {
    ynode = tnx_top_d->search_child(e.element);
    ASSERT_TRUE(ynode);
    EXPECT_STREQ(e.string, ynode->get_xml_sample_element(0,1,false,false).c_str());
  }

  xml1 = tnx_top_d->get_xml_sample_element( 0, 1, false, true );
  xml2 = tnx_top_d->get_xml_sample_element( 0, 1, false, false );
  EXPECT_NE(xml1, xml2);
  EXPECT_NE(xml1, "");
  EXPECT_NE(xml2, "");


  auto top1c1 = testy2p_top1->search_node("top1c1",testy2p_top1->get_ns());
  ASSERT_TRUE(ynode);
  xml2 = top1c1->get_xml_sample_element( 0, 100, false, false );
  EXPECT_NE(xml2, "");
  std::cout << xml2 << "\n\n";

  static const struct {
    const char* element;
    const char* string;
  } elems2[] = {
    { "li8",       "<y2p-top1:li8>-8</y2p-top1:li8>" },
    { "li16",      "<y2p-top1:li16>-16</y2p-top1:li16>" },
    { "li32",      "<y2p-top1:li32>-32</y2p-top1:li32>" },
    { "li64",      "<y2p-top1:li64>-64</y2p-top1:li64>" },
    { "lu8",       "<y2p-top1:lu8>8</y2p-top1:lu8>" },
    { "lu16",      "<y2p-top1:lu16>16</y2p-top1:lu16>" },
    { "lu32",      "<y2p-top1:lu32>32</y2p-top1:lu32>" },
    { "lu64",      "<y2p-top1:lu64>64</y2p-top1:lu64>" },
    { "liref",     "<y2p-top1:liref>IDENTITY_VALUE</y2p-top1:liref>" },
    { "lbin",      "<y2p-top1:lbin>BASE64+Encoded==</y2p-top1:lbin>" },
    { "lbits",     "<y2p-top1:lbits>top1-a top1-b</y2p-top1:lbits>" },
    { "lenum",     "<y2p-top1:lenum>TOP1-E_A</y2p-top1:lenum>" },
    { "lbool",     "<y2p-top1:lbool>true</y2p-top1:lbool>" },
    { "ldec",      "<y2p-top1:ldec>0.0</y2p-top1:ldec>" },
    { "lmt",       "<y2p-top1:lmt/>" },
    { "ls",        "<y2p-top1:ls>STRING</y2p-top1:ls>" },
    { "lunion",    "<y2p-top1:lunion>UNION_VALUE</y2p-top1:lunion>" },
    { "top1g2-c1", "<y2p-top1:top1g2-c1>...</y2p-top1:top1g2-c1>" },
    { "top1g2-l1", "<y2p-top1:top1g2-l1>...</y2p-top1:top1g2-l1>" },
  };
  for (auto &e:elems2) {
    ynode = top1c1->search_child(e.element);
    ASSERT_TRUE(ynode);
    EXPECT_STREQ(e.string, ynode->get_xml_sample_element(0,0,false,false).c_str());
  }
}

TEST(YangNcx, JsonSamples)
{
  TEST_DESCRIPTION("Test YangModel JSON samples");

  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  auto test_ydom_top = model->load_module("test-ydom-top");
  ASSERT_TRUE(test_ydom_top);
  auto test_ydom_aug = model->load_module("test-ydom-aug");
  ASSERT_TRUE(test_ydom_aug);
  auto testy2p_top1 = model->load_module("testy2p-top1");
  ASSERT_TRUE(testy2p_top1);
  auto test_aug_choice = model->load_module("test-aug-choice");
  ASSERT_TRUE(test_aug_choice);
  auto testncx = model->load_module("testncx");
  ASSERT_TRUE(testncx);
  auto test_augment_a1 = model->load_module("test-augment-a1");
  ASSERT_TRUE(test_augment_a1);

  auto root = model->get_root_node();
  ASSERT_TRUE(root);

  auto ynode = test_ydom_top->search_node("top",test_ydom_top->get_ns());
  ASSERT_TRUE(ynode);

  auto json1 = ynode->get_rwrest_json_sample_element( 2, 0, true );
  auto json2 = ynode->get_rwrest_json_sample_element( 2, 0, true );
  EXPECT_EQ(json1, json2);
  EXPECT_NE(json2, "");

  json1 = json2;
  json2 = ynode->get_rwrest_json_sample_element( 2, 1, true );
  EXPECT_NE(json1, json2);
  EXPECT_NE(json2, "");

  json1 = json2;
  json2 = ynode->get_rwrest_json_sample_element( 0, 1, true );
  EXPECT_NE(json1, json2);
  EXPECT_NE(json2, "");

  json1 = json2;
  json2 = ynode->get_rwrest_json_sample_element( 0, 2, true );
  EXPECT_NE(json1, json2);
  EXPECT_NE(json2, "");

  json1 = json2;
  json2 = ynode->get_rwrest_json_sample_element( 0, 100, true );
  EXPECT_NE(json1, json2);
  EXPECT_NE(json2, "");
  std::cout << json2 << "\n\n";


  ynode = testncx->search_node("top",testncx->get_ns());
  ASSERT_TRUE(ynode);

  auto tnx_top_d = ynode->search_child("d");
  ASSERT_TRUE(tnx_top_d);

  json1 = tnx_top_d->get_rwrest_json_sample_element( 0, 1, true );
  json2 = tnx_top_d->get_rwrest_json_sample_element( 0, 1, false );
  EXPECT_NE(json1, json2);
  EXPECT_NE(json1, "");
  EXPECT_NE(json2, "");
  std::cout << json2 << "\n\n";

  static const struct {
    const char* element;
    const char* string;
  } elems1[] = {
    { "n1",    "0" },
    { "n2",    "0" },
    { "n3",    "0" },
    { "n4",    "0" },
    { "n5",    "0" },
    { "n6",    "0" },
    { "n7",    "0" },
    { "n8",    "0" },
    { "iref1", "\"riftio\"" },
    { "bin1",  "\"BASE64+Encoded==\"" },
    { "bits1", "\"a b\"" },
    { "bool1", "true" },
    { "dec1",  "0.0" },
    { "e1",    "[null]" },
    { "s1",    "\"STRING\"" },
    { "u1",    "\"UNION_VALUE\"" },
  };
  for (auto &e:elems1) {
    ynode = tnx_top_d->search_child(e.element);
    ASSERT_TRUE(ynode);
    EXPECT_STREQ(e.string, ynode->get_rwrest_json_sample_element(0,1,false).c_str());
  }

  auto top1c1 = testy2p_top1->search_node("top1c1",testy2p_top1->get_ns());
  ASSERT_TRUE(ynode);
  json2 = top1c1->get_rwrest_json_sample_element( 0, 1, false );
  EXPECT_NE(json2, "");
  std::cout << json2 << "\n\n";

  static const struct {
    const char* element;
    const char* string;
  } elems2[] = {
    { "li8",       "-8" },
    { "li16",      "-16" },
    { "li32",      "-32" },
    { "li64",      "-64" },
    { "lu8",       "8" },
    { "lu16",      "16" },
    { "lu32",      "32" },
    { "lu64",      "64" },
    { "liref",     "\"IDENTITY_VALUE\"" },
    { "lbin",      "\"BASE64+Encoded==\"" },
    { "lbits",     "\"top1-a top1-b\"" },
    { "lenum",     "\"TOP1-E_A\"" },
    { "lbool",     "true" },
    { "ldec",      "0.0" },
    { "lmt",       "[null]" },
    { "ls",        "\"STRING\"" },
    { "lunion",    "\"UNION_VALUE\"" },
    { "top1g2-c1", "{ ... }" },
    { "top1g2-l1", "[ { ... }, { ... } ]" },
  };
  for (auto &e:elems2) {
    ynode = top1c1->search_child(e.element);
    ASSERT_TRUE(ynode);
    EXPECT_STREQ(e.string, ynode->get_rwrest_json_sample_element(0,0,false).c_str());
  }
}

