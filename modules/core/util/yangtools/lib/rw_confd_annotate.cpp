/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Creation Date: 3/24/16
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#include "rw_confd_annotate.hpp"

const char* YANGMODEL_ANNOTATION_CONFD_NS = "confd_ns_annotation";
const char* YANGMODEL_ANNOTATION_CONFD_NAME = "confd_name_annotation";

static rw_status_t rw_confd_annotate_ynode  (YangNode *ynode,
                                             namespace_map_t& map,
                                             ymodel_map_func_ptr_t func,
                                             const char *ns_ext,
                                             const char *name_ext)
{
  AppDataToken<intptr_t,rw_yang::AppDataTokenDeleterNull>
      ns (RW_ANNOTATIONS_NAMESPACE, ns_ext);
  AppDataToken<intptr_t,rw_yang::AppDataTokenDeleterNull>
      name (RW_ANNOTATIONS_NAMESPACE, name_ext);
  uint32_t ns_hash = 0, name_hash = 0;

  rw_yang_stmt_type_t stmt_type = ynode->get_stmt_type();
  YangModel *model = ynode->get_model();

  switch (stmt_type) {
    case RW_YANG_STMT_TYPE_ROOT:
      break;
    case RW_YANG_STMT_TYPE_RPC:
    case RW_YANG_STMT_TYPE_RPCIO:
      break;
    default:
      {
        namespace_map_t::const_iterator iter = map.find (ynode->get_ns());
        if (map.end() == iter) {
          // ATTN: RIFT-9411: RW_ASSERT(0);
          //   return RW_STATUS_FAILURE;
          return RW_STATUS_SUCCESS;
        }
        ns_hash = iter->second;

        name_hash = func (ynode->get_name());
        if (!name_hash) {
          // returning failure causes siblings to not get annotated
          // THIS should be logged appropriately. Will not appear in confd CLI.
          // so no processign errors will present at run time
          return RW_STATUS_SUCCESS;
        }
        // set the app data
        ynode->app_data_set_and_give_ownership(&model->adt_confd_ns_, intptr_t(ns_hash));
        ynode->app_data_set_and_give_ownership(&model->adt_confd_name_, intptr_t(name_hash));
      }
  }

  // recurse through children
  for (YangNode *cnode = ynode->get_first_child(); NULL != cnode; cnode = cnode->get_next_sibling() ) {
    if (RW_STATUS_SUCCESS != rw_confd_annotate_ynode (cnode, map, func, ns_ext, name_ext)) {
      return RW_STATUS_FAILURE;
    }
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_confd_annotate_ynodes (YangModel *model,
                                      namespace_map_t& map,
                                      ymodel_map_func_ptr_t func,
                                      const char *ns_ext,
                                      const char *name_ext)
{
  YangNode *root = model->get_root_node();

  if (nullptr == root) {
    RW_ASSERT(0);
    return RW_STATUS_FAILURE;
  }

  return rw_confd_annotate_ynode (root, map, func, ns_ext, name_ext);
}

YangNode* rw_confd_search_child_tags (YangNode* ynode,
                                      uint32_t ns, 
                                      uint32_t tag)
{
  YangModel *model = ynode->get_model();
  bool status = false;

  for (YangNodeIter yni = ynode->child_begin(); yni != ynode->child_end(); ++yni) {
    intptr_t tmp = (intptr_t)0;
    status = yni->app_data_check_and_get (&model->adt_confd_ns_, &tmp);
    if ((!status) || (ns != tmp)) {
      continue;
    }

    status = yni->app_data_check_and_get (&model->adt_confd_name_, &tmp);
    if ((status) && (tag == tmp)) {
      return &*yni;
    }
  }

  return nullptr;
}
