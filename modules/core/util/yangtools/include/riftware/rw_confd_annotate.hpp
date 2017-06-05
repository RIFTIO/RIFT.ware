/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Creation Date: 3/24/16
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#ifndef RW_CONFD_ANNOTATE_HPP__
#define RW_CONFD_ANNOTATE_HPP__

#include "rwlib.h"
#include "yangmodel.h"

using namespace rw_yang;

extern const char* YANGMODEL_ANNOTATION_CONFD_NS;
extern const char* YANGMODEL_ANNOTATION_CONFD_NAME;

/*!
 * TAILF/confd works by mapping strings and name spaces to integers for easier
 * lookup in CDB and other TAILF libraries. To work nicely with Riftware, these
 * integer values also need to be used to traverse rw_xml doms, and extended
 * protobuf structures.
 *
 * This function takes as an input a map that provides mapping for name space
 * values to integers, and a function pointer that can be invoked to translate
 * names to integers. With these available, this function annotate a node and
 * its children with a node name number value and name space number value.
 * 
 * At the model level, the root's annotate_nodes is called.
 *
 * @return RW_STATUS_SUCCESS, if the namespace and node names of all the nodes 
 *         in the model and its subtree can be mapped
 * @return RW_STATUS_FAILURE otherwise. Mapping stops on encountering a node
 *         which does not map correctly
 */
rw_status_t rw_confd_annotate_ynodes ( 
    YangModel* model, /*!< [in] The Yang Model pointer */
    namespace_map_t& map, /*!< [in] A map containing string to integer associations for namespaces */
    ymodel_map_func_ptr_t func, /*!< [in] a function that can map a string to a number */
    const char *ns_ext, /*!< [in] extension value to be used for name space */
    const char *name_ext /*!< [in] extension value to be used for name  */
);

/*!
 * When a YANG model is decorated with associated confd tags, each node
 * in the model has a associated tag and namespace. This is generated
 * by confd, but the namespace and tag should uniquely identify a child
 * node.
 *
 * @return the child node if found, nullptr otherwise
 *
 * @return the child node if found, nullptr otherwise
 */
YangNode* rw_confd_search_child_tags (
    YangNode* ynode,
    uint32_t ns, 
    uint32_t tag
);

#endif // RW_CONFD_ANNOTATE_HPP__
