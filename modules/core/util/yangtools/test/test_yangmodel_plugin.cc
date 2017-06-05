/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Author(s): Rajesh Ramankutty
 * Creation Date: 05/16/2014
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#include <stdio.h>
#include "rwlib.h"
#include "rw_vx_plugin.h"
#include "../vala/yangmodel_plugin.h"
#include "yangmodel.h"

#include "rwut.h"

#if !defined(INSTALLDIR) || !defined(PLUGINDIR)
#error - Makefile must define INSTALLDIR and PLUGINDIR
#endif

using ::testing::AtLeast;

class YangModelPluginTest : public ::testing::Test {

  protected:
    virtual void SetUp() {
        rw_status_t status = RW_STATUS_SUCCESS;

        rw_vx_prepend_gi_search_path(INSTALLDIR "/usr/lib/rift/girepository-1.0");
        rw_vx_prepend_gi_search_path(PLUGINDIR);
        rw_vx_prepend_gi_search_path("../../vala");
        rw_vx_require_repository("YangModelPlugin", "1.0");

        /*
         * Allocate a RIFT_VX framework to run the test
         * NOTE: this initializes a LIBPEAS instance
         */
        this->vxfp = rw_vx_framework_alloc();
        RW_ASSERT(this->vxfp);

        rw_vx_add_peas_search_path(this->vxfp, PLUGINDIR);

        // Register the plugin
        status = rw_vx_library_open(this->vxfp,
                                    (char*)"yangmodel_plugin-c",
                                    (char *)"",
                                    &this->common);

        RW_ASSERT((RW_STATUS_SUCCESS == status) && (NULL != this->common));

        this->load_module(YANGMODEL_PLUGIN_TYPE_MODEL, (void **) &this->model_c, (void **) &this->model_i);
        this->load_module(YANGMODEL_PLUGIN_TYPE_MODULE, (void **) &this->module_c, (void **) &this->module_i);
        this->load_module(YANGMODEL_PLUGIN_TYPE_NODE, (void **) &this->node_c, (void **) &this->node_i);
        this->load_module(YANGMODEL_PLUGIN_TYPE_KEY, (void **) &this->key_c, (void **) &this->key_i);
    }

    void load_module(GType type, void **p_api, void **p_iface) {
        rw_status_t status = rw_vx_library_get_api_and_iface(this->common, type, p_api, p_iface, NULL);
        RW_ASSERT(status == RW_STATUS_SUCCESS);
        RW_ASSERT(*p_api);
        RW_ASSERT(*p_iface);
    }


    virtual void TearDown() {
      // ADD STUFF HERE
    }

    void stop_test()
    { 
      fprintf(stderr, "Stopping test.\n");
      this->stopped = true;
    }

    bool stopped = false;
    rw_vx_modinst_common_t *common = NULL;
    rw_vx_framework_t *vxfp;

    YangmodelPluginModel *model_c;
    YangmodelPluginModelIface *model_i;

    YangmodelPluginModule *module_c;
    YangmodelPluginModuleIface *module_i;

    YangmodelPluginNode *node_c;
    YangmodelPluginNodeIface *node_i;

    YangmodelPluginKey *key_c;
    YangmodelPluginKeyIface *key_i;
};

#define MODELI this->model_i
#define MODELC this->model_c

#define MODULEI this->module_i
#define MODULEC this->module_c

#define NODEI this->node_i
#define NODEC this->node_c

#define KEYI this->key_i
#define KEYC this->key_c


TEST_F(YangModelPluginTest, CreateDestroy)
{
  TEST_DESCRIPTION("Creates and destroy a YangModelNcx");
  rw_yang_model_t* model; 
  model = (rw_yang_model_t*)(MODELI->alloc(MODELC));
  ASSERT_TRUE(model);
}

TEST_F(YangModelPluginTest, LoadTestNcx)
{
  TEST_DESCRIPTION("Loads a base module that was already imported by a previously loaded module");
  rw_yang_model_t* model = (rw_yang_model_t*)(MODELI->alloc(MODELC));
  ASSERT_TRUE(model);

  rw_yang_module_t* test_ncx = (rw_yang_module_t*)(MODELI->load_module(MODELC, (guint64)model, (char*)"testncx"));
  EXPECT_TRUE(test_ncx);

  rw_yang_module_t* test_ncx_base = (rw_yang_module_t*)(MODELI->load_module(MODELC, (guint64)model, (char*)"testncx-base"));
  EXPECT_TRUE(test_ncx_base);
  MODELI->free(MODELC, (guint64)model);
}


TEST_F(YangModelPluginTest, BasicModel)
{
  TEST_DESCRIPTION("Basic model tests");

  rw_yang_model_t* model = (rw_yang_model_t*)(MODELI->alloc(MODELC));
  ASSERT_TRUE(model);

  rw_yang_module_t* test_ncx = (rw_yang_module_t*)(MODELI->load_module(MODELC, (guint64)model, (char*)"testncx"));
  EXPECT_TRUE(test_ncx);

  rw_yang_node_t* root = (rw_yang_node_t*)MODELI->get_root_node(MODELC, (guint64)model);
  ASSERT_TRUE(root);
  EXPECT_STREQ("data", NODEI->name(NODEC, (guint64)root));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)root));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)root));

  rw_yang_node_t* top = (rw_yang_node_t*)NODEI->first_child(NODEC, (guint64)root);
  ASSERT_TRUE(top);
  EXPECT_EQ((guint64)root, NODEI->parent_node(NODEC, (guint64)top));
  EXPECT_STREQ("top", NODEI->name(NODEC, (guint64)top));
  EXPECT_EQ((guint64)top, NODEI->first_child(NODEC, (guint64)root));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)top));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)top));

  rw_yang_node_t* top_sib = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)top);
  EXPECT_FALSE(top_sib);

  rw_yang_node_t* a = (rw_yang_node_t*)NODEI->first_child(NODEC, (guint64)top);
  ASSERT_TRUE(a);
  EXPECT_EQ(NODEI->parent_node(NODEC, (guint64)a), (guint64)top);
  EXPECT_STREQ("a", NODEI->name(NODEC, (guint64)a));
  EXPECT_EQ((guint64)a, NODEI->first_child(NODEC, (guint64)top));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)a));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)a));

  rw_yang_node_t* cont_in_a = (rw_yang_node_t*)NODEI->first_child(NODEC, (guint64)a);
  ASSERT_TRUE(cont_in_a);
  EXPECT_EQ(NODEI->parent_node(NODEC, (guint64)cont_in_a),(guint64)a);
  EXPECT_STREQ("cont-in-a", NODEI->name(NODEC, (guint64)cont_in_a));
  EXPECT_EQ((guint64)cont_in_a, NODEI->first_child(NODEC, (guint64)a));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)cont_in_a));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)cont_in_a));

  rw_yang_node_t* str = (rw_yang_node_t*)NODEI->first_child(NODEC, (guint64)cont_in_a);
  ASSERT_TRUE(str);
  rw_yang_node_t* num1 = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)str);
  ASSERT_TRUE(num1);
  rw_yang_node_t* ll = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)num1);
  ASSERT_TRUE(ll);
  rw_yang_node_t* lst = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)ll);
  ASSERT_TRUE(lst);
  rw_yang_key_t* lst_key = (rw_yang_key_t*)NODEI->first_key(NODEC, (guint64)lst);
  ASSERT_TRUE(lst_key);
  EXPECT_FALSE(KEYI->next_key(KEYC, (guint64)lst_key));
  EXPECT_EQ(KEYI->list_node(KEYC, (guint64)lst_key), (guint64)lst);
  rw_yang_node_t* num2 = (rw_yang_node_t*)NODEI->first_child(NODEC, (guint64)lst);
  ASSERT_TRUE(num2);
  EXPECT_FALSE(NODEI->is_key(NODEC, (guint64)num2));
  rw_yang_node_t* str2 = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)num2);
  ASSERT_TRUE(str2);
  EXPECT_EQ((guint64)str2, KEYI->key_node(KEYC, (guint64)lst_key));
  EXPECT_TRUE(NODEI->is_key(NODEC, (guint64)str2));

  rw_yang_node_t* a1 = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)a);
  ASSERT_TRUE(a1);
  EXPECT_EQ(NODEI->parent_node(NODEC, (guint64)a1), (guint64)top);
  EXPECT_STREQ("a1", NODEI->name(NODEC, (guint64)a1));
  EXPECT_EQ((guint64)a1, NODEI->next_sibling(NODEC, (guint64)a));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)a1));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)a1));

  rw_yang_node_t* b = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)a1);
  ASSERT_TRUE(b);
  EXPECT_EQ(NODEI->parent_node(NODEC, (guint64)b), (guint64)top);
  EXPECT_STREQ("b", NODEI->name(NODEC, (guint64)b));
  EXPECT_EQ((guint64)b, NODEI->next_sibling(NODEC, (guint64)a1));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)b));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)b));

  rw_yang_node_t* c = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)b);
  ASSERT_TRUE(c);
  EXPECT_EQ(NODEI->parent_node(NODEC, (guint64)c), (guint64)top);
  EXPECT_STREQ("c", NODEI->name(NODEC, (guint64)c));
  EXPECT_EQ((guint64)c, NODEI->next_sibling(NODEC, (guint64)b));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)c));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)c));

  rw_yang_node_t* c_c = (rw_yang_node_t*)NODEI->first_child(NODEC, (guint64)c);
  ASSERT_TRUE(c_c);
  EXPECT_STREQ("c", NODEI->name(NODEC, (guint64)c_c));
  EXPECT_EQ((guint64)c_c, NODEI->first_child(NODEC, (guint64)c));
  EXPECT_FALSE(NODEI->first_child(NODEC, (guint64)c_c));
  EXPECT_FALSE(NODEI->next_sibling(NODEC, (guint64)c_c));
  EXPECT_TRUE(NODEI->is_leafy(NODEC, (guint64)c_c));
  EXPECT_TRUE(NODEI->type(NODEC, (guint64)c_c));

  rw_yang_node_t* d = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)c);
  ASSERT_TRUE(d);
  EXPECT_EQ(NODEI->parent_node(NODEC, (guint64)d), (guint64)top);
  EXPECT_STREQ("d", NODEI->name(NODEC, (guint64)d));
  EXPECT_EQ((guint64)d, NODEI->next_sibling(NODEC, (guint64)c));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)d));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)d));

  rw_yang_node_t* e = (rw_yang_node_t*)NODEI->next_sibling(NODEC, (guint64)d);
  ASSERT_TRUE(e);
  EXPECT_STREQ("e", NODEI->name(NODEC, (guint64)e));
  EXPECT_EQ((guint64)e, NODEI->next_sibling(NODEC, (guint64)d));
  EXPECT_TRUE(NODEI->next_sibling(NODEC, (guint64)e));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)e));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)e));

  rw_yang_node_t* d_n1 = (rw_yang_node_t*)NODEI->first_child(NODEC, (guint64)d);
  rw_yang_node_t* e_n1 = (rw_yang_node_t*)NODEI->first_child(NODEC, (guint64)e);
  ASSERT_TRUE(d_n1);
  ASSERT_TRUE(e_n1);
  ASSERT_TRUE(NODEI->is_leafy(NODEC, (guint64)d_n1));
  ASSERT_TRUE(NODEI->is_leafy(NODEC, (guint64)e_n1));
  EXPECT_STREQ(NODEI->name(NODEC, (guint64)d_n1),
               NODEI->name(NODEC, (guint64)e_n1));
  EXPECT_EQ(NODEI->name(NODEC, (guint64)d_n1),
            NODEI->name(NODEC, (guint64)e_n1));
  ASSERT_TRUE(NODEI->type(NODEC, (guint64)d_n1));
  ASSERT_TRUE(NODEI->type(NODEC, (guint64)e_n1));
  EXPECT_EQ(NODEI->type(NODEC, (guint64)d_n1),
            NODEI->type(NODEC, (guint64)e_n1));

  //rw_yang_model_dump(model, 2/*indent*/, 1/*verbosity*/);
  MODELI->free(MODELC, (guint64)model);
}


TEST_F(YangModelPluginTest, YangNcxExt)
{
  TEST_DESCRIPTION("Test extensions and extension iterators");

  rw_yang_model_t* model = (rw_yang_model_t*)(MODELI->alloc(MODELC));
  ASSERT_TRUE(model);

  rw_yang_module_t* test_ncx = (rw_yang_module_t*)(MODELI->load_module(MODELC, (guint64)model, (char*)"testncx-ext"));
  EXPECT_TRUE(test_ncx);

  rw_yang_node_t* root = (rw_yang_node_t*)MODELI->get_root_node(MODELC, (guint64)model);
  ASSERT_TRUE(root);
  EXPECT_STREQ("data", NODEI->name(NODEC, (guint64)root));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)root));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)root));


  rw_yang_node_t* top1 = (rw_yang_node_t*)(NODEI->first_child(NODEC, (guint64)root));
  ASSERT_TRUE(top1);
  EXPECT_STREQ("top1", NODEI->name(NODEC, (guint64)top1));

  rw_yang_node_t* top1cina = (rw_yang_node_t*)(NODEI->first_child(NODEC, (guint64)top1));
  ASSERT_TRUE(top1cina);
  EXPECT_STREQ("cina", NODEI->name(NODEC, (guint64)top1cina));

  rw_yang_node_t* top1lina = (rw_yang_node_t*)(NODEI->next_sibling(NODEC, (guint64)top1cina));
  ASSERT_TRUE(top1lina);
  EXPECT_STREQ("lina", NODEI->name(NODEC, (guint64)top1lina));

  rw_yang_node_t* top1lf5 = (rw_yang_node_t*)(NODEI->next_sibling(NODEC, (guint64)top1lina));
  ASSERT_TRUE(top1lf5);
  EXPECT_STREQ("lf5", NODEI->name(NODEC, (guint64)top1lf5));

  rw_yang_node_t* top1cint1 = (rw_yang_node_t*)(NODEI->next_sibling(NODEC, (guint64)top1lf5));
  ASSERT_TRUE(top1cint1);
  EXPECT_STREQ("cint1", NODEI->name(NODEC, (guint64)top1cint1));

  rw_yang_node_t* lf1 = (rw_yang_node_t*)(NODEI->first_child(NODEC, (guint64)top1cint1));
  ASSERT_TRUE(lf1);
  EXPECT_STREQ("lf1", NODEI->name(NODEC, (guint64)lf1));

  rw_yang_node_t* top2 = (rw_yang_node_t*)(NODEI->next_sibling(NODEC, (guint64)top1));
  ASSERT_TRUE(top2);
  EXPECT_STREQ("top2", NODEI->name(NODEC, (guint64)top2));

  rw_yang_node_t* top3 = (rw_yang_node_t*)(NODEI->next_sibling(NODEC, (guint64)top2));
  ASSERT_TRUE(top3);
  EXPECT_STREQ("top3", NODEI->name(NODEC, (guint64)top3));

  rw_yang_node_t* top3lf8 = (rw_yang_node_t*)(NODEI->first_child(NODEC, (guint64)top3));
  ASSERT_TRUE(top3lf8);
  EXPECT_STREQ("lf8", NODEI->name(NODEC, (guint64)top3lf8));
}


TEST_F(YangModelPluginTest, YangNcxModule)
{
  TEST_DESCRIPTION("Test modules and module import");

  rw_yang_model_t* model = (rw_yang_model_t*)(MODELI->alloc(MODELC));
  ASSERT_TRUE(model);

  rw_yang_module_t* top2 = (rw_yang_module_t*)(MODELI->load_module(MODELC, (guint64)model, (char*)"testncx-mod-top2"));
  EXPECT_TRUE(top2);

  rw_yang_node_t* root = (rw_yang_node_t*)MODELI->get_root_node(MODELC, (guint64)model);
  ASSERT_TRUE(root);
  EXPECT_STREQ("data", NODEI->name(NODEC, (guint64)root));
  EXPECT_FALSE(NODEI->is_leafy(NODEC, (guint64)root));
  EXPECT_FALSE(NODEI->type(NODEC, (guint64)root));



  rw_yang_module_t* top1 = (rw_yang_module_t*)(MODELI->load_module(MODELC, (guint64)model, (char*)"testncx-mod-top1"));
  EXPECT_TRUE(top1);

}

static void check_rw_types( YangmodelPluginNodeIface *iface, YangmodelPluginNode *klass, rw_yang_node_t* node)
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
    rw_yang_node_t* yn = (rw_yang_node_t*)(iface->search_child(klass, (guint64)node, (char*)p->name, (char*)nullptr));
    ASSERT_TRUE(yn);
    rw_yang_type_t* yt = (rw_yang_type_t*)(iface->type(klass, (guint64)yn));
    ASSERT_TRUE(yt);
    //rw_yang_value_t* yv = NULL;
    //rw_status_t rs = (rw_status_t)(iface->yang_type_parse_value(klass, (guint64)yt, p->value, &yv));
    //EXPECT_EQ(rs, RW_STATUS_SUCCESS);
    //if (RW_STATUS_SUCCESS != rs) {
      //std::cout << "parse of " << p->name << " failed for value " << p->value << std::endl;
    //}
  }
}

TEST_F(YangModelPluginTest, RwTypesYang)
{
  TEST_DESCRIPTION("Test rw-yang-types.yang");

  rw_yang_model_t* model = (rw_yang_model_t*)(MODELI->alloc(MODELC));
  ASSERT_TRUE(model);

  rw_yang_module_t* trwt = (rw_yang_module_t*)(MODELI->load_module(MODELC, (guint64)model, (char*)"test-rw-yang-types"));
  EXPECT_TRUE(trwt);

  rw_yang_node_t* root = (rw_yang_node_t*)MODELI->get_root_node(MODELC, (guint64)model);
  ASSERT_TRUE(root);


  rw_yang_node_t* tuses = (rw_yang_node_t*)(NODEI->search_child(NODEC, (guint64)root, (char*)"tuses", (char*)nullptr));
  ASSERT_TRUE(tuses);
  check_rw_types(NODEI, NODEC, tuses);
  rw_yang_node_t* tcont = (rw_yang_node_t*)(NODEI->search_child(NODEC, (guint64)root, (char*)"tcont", (char*)nullptr));
  ASSERT_TRUE(tcont);
  check_rw_types(NODEI, NODEC, tcont);
  rw_yang_node_t* ttypes = (rw_yang_node_t*)(NODEI->search_child(NODEC, (guint64)root, (char*)"ttypes", (char*)nullptr));
  ASSERT_TRUE(ttypes);
  check_rw_types(NODEI, NODEC, ttypes);
}

TEST_F(YangModelPluginTest, OriginalTest)
{
  TEST_DESCRIPTION("Creates and destroy a YangModelNcx");

  // this function will print a message from "python"
  {
    rw_yang_model_t* model;
    rw_yang_module_t* module;
    rw_yang_node_t *root, *top;
    char *location_str;
    model = (rw_yang_model_t*)(MODELI->alloc(MODELC));
    fprintf(stderr, "yang_model_create_libncx() returned model = %p\n", model);

    module = (rw_yang_module_t*)(MODELI->load_module(MODELC, (guint64)model, (char*)"testncx"));
    fprintf(stderr, "yang_model_load_module() returned module = %p\n", module);

    MODULEI->location(MODULEC, (guint64)module, &location_str);
    fprintf(stderr, "yang_module_get_location :%s\n", location_str);

    fprintf(stderr, "yang_module_get_description :%s\n", MODULEI->description(MODULEC, (guint64)module));

    root = (rw_yang_node_t*)(MODELI->get_root_node(MODELC, (guint64)model));
    fprintf(stderr, "yang_model_get_root_node() returned node = %p\n", root);

    fprintf(stderr, "yang_node_get_description :%s\n", NODEI->description(NODEC, (guint64)root));
    fprintf(stderr, "yang_node_get_name :%s\n", NODEI->name(NODEC, (guint64)root));
    top = (rw_yang_node_t*)(NODEI->first_child(NODEC, (guint64)root));
    fprintf(stderr, "yang_node_get_first_child() returned node = %p\n", top);
    fprintf(stderr, "yang_node_get_name :%s\n", NODEI->name(NODEC, (guint64)top));
  }
}

TEST_F(YangModelPluginTest, DumpTree)
{
  TEST_DESCRIPTION("Basic model tests");

  rw_yang_model_t* model = (rw_yang_model_t*)(MODELI->alloc(MODELC));
  ASSERT_TRUE(model);

  rw_yang_module_t* test_ncx = (rw_yang_module_t*)(MODELI->load_module(MODELC, (guint64)model, (char*)"testncx"));
  EXPECT_TRUE(test_ncx);

  rw_yang_node_t* root = (rw_yang_node_t*)MODELI->get_root_node(MODELC, (guint64)model);
  ASSERT_TRUE(root);

  EXPECT_STREQ("data", NODEI->name(NODEC, (guint64)root));
}
