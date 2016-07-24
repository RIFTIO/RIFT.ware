/*
 * Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: xsd_yang.c

        
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
04feb08      abb      split out from xsd_typ.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include <xmlstring.h>
#include <xmlreader.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_cfg
#include "cfg.h"
#endif

#ifndef _H_def_reg
#include "def_reg.h"
#endif

#ifndef _H_dlq
#include "dlq.h"
#endif

#ifndef _H_ext
#include "ext.h"
#endif

#ifndef _H_ncx
#include "ncx.h"
#endif

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncxmod
#include "ncxmod.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_rpc
#include "rpc.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_typ
#include "typ.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_xmlns
#include "xmlns.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

#ifndef _H_xml_val
#include "xml_val.h"
#endif

#ifndef _H_xsd_typ
#include "xsd_typ.h"
#endif

#ifndef _H_xsd_util
#include "xsd_util.h"
#endif

#ifndef _H_xsd_yang
#include "xsd_yang.h"
#endif

#ifndef _H_yang
#include "yang.h"
#endif

#ifndef _H_yangconst
#include "yangconst.h"
#endif

#define RIFT


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*        F O R W A R D - - R E F    S T A T I C  F N S              * 
*                                                                   *
*********************************************************************/

static status_t
    do_yang_elem_btype (struct ncx_instance_t_ *instance,
                        ncx_module_t *mod,
                        obj_template_t *obj,
                        obj_template_t *augtargobj,
                        boolean iskey,
                        val_value_t *val);

static status_t
    do_local_typedefs (struct ncx_instance_t_ *instance,
                       ncx_module_t *mod,
                       obj_template_t *obj,
                       dlq_hdr_t *typnameQ);


/********************************************************************
* FUNCTION add_type_mapping
* 
*   Create a xsd_typname_t struct for a local typedef,
*   and add it to the specified Q
*
* INPUTS
*    typ == typ_template_t struct to add to the Q
*    que == Q of xsd_typname_t struct to check
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_type_mapping (ncx_instance_t *instance,
                      typ_template_t *typ,
                      dlq_hdr_t *que)
{
    ncx_typname_t   *tn;
    xmlChar         *buff, *p;
    uint32           idnum;
    boolean          done;

    /* !!! THIS MAY NOT PRODUCE THE SAME XSD TYPENAMES IF
     * !!! RUN FROM A SUBMODULE, OR FROM THE TOP MODULE
     * !!! A SUBMODULE TOP FILE XSD IS ONLY SELF-CONSISTENT
     * !!! DEPENDING ON THE SUBSET OF SUBMODULES INCLUDED
     * !!! DIFFERENT TYPENAMES ACROSS INCLUDES == BROKEN XSDs 
     */

    tn = ncx_new_typname(instance);
    if (!tn) {
        return ERR_INTERNAL_MEM;
    }

    if (ncx_find_typname_type(instance, que, typ->name)) {
        /* need to create a mapped typename */
        buff = m__getMem(instance, xml_strlen(instance, typ->name) + 8);
        if (!buff) {
            ncx_free_typname(instance, tn);
            return ERR_INTERNAL_MEM;
        }
        p = buff;
        p += xml_strcpy(instance, p, typ->name);
        *p++ = '_';    /* separate the typename and the mapping ID */

        idnum = 1;
        done = FALSE;
        while (!done) {
            sprintf((char *)p, "%u", idnum);
            if (ncx_find_typname_type(instance, que, buff)) {
                if (++idnum > 999999) {
                    ncx_free_typname(instance, tn);
                    m__free(instance, buff);
                    return SET_ERROR(instance, ERR_BUFF_OVFL);
                }
            } else {
                tn->typ = typ;
                tn->typname_malloc = buff;
                tn->typname = tn->typname_malloc;
                dlq_enque(instance, tn, que);
                done = TRUE;
            }
        }
    } else {
        /* this type name not in use yet -- use a direct mapping */
        tn->typ = typ;
        tn->typname = typ->name;
        dlq_enque(instance, tn, que);
    }

    return NO_ERR;

}  /* add_type_mapping */


/********************************************************************
* FUNCTION add_augtarget_subgrp
* 
*   Add a substitutionGroup attribute for an augment target
*
* INPUTS:
*    targobj == obj_template_t for the augment target parent node
*    elem == element value strut to add the substitutionGroup attribute into
*
* OUTPUT:
*    elem has substitutionGroup attribute added
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_augtarget_subgrp (ncx_instance_t *instance,
                          obj_template_t *targobj,
                          val_value_t *elem)
{
    xmlChar    *qname, *buff;
    status_t    res;

    res = obj_gen_aughook_id(instance, targobj, &buff);
    if (res != NO_ERR) {
        return res;
    }

    qname = xml_val_make_qname(instance, targobj->tkerr.mod->nsid, buff);
    m__free(instance, buff);
    if (!qname) {
        return res;
    }

    /* add the substitutionGroup attribute */
    res = xml_val_add_attr(instance, XSD_SUB_GRP, 0, qname, elem);
    if (res != NO_ERR) {
        m__free(instance, qname);

    }
    return res;    

}  /* add_augtarg_subgrp */


/********************************************************************
* FUNCTION do_typedefs_container
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of typname_map_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each local type within this
*        container or any groups, or any children
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_typedefs_container (ncx_instance_t *instance,
                           ncx_module_t *mod,
                           obj_template_t *obj,
                           dlq_hdr_t *typnameQ)
{
    obj_container_t  *con;
    typ_template_t   *typ;
    status_t          res;

    con = obj->def.container;

    /* check any local typedefs in this container */
    for (typ = (typ_template_t *)dlq_firstEntry(instance, con->typedefQ);
         typ != NULL;
         typ = (typ_template_t *)dlq_nextEntry(instance, typ)) {
        res = add_type_mapping(instance, typ, typnameQ);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* check any local typedefs in any child objects */
    res = xsd_do_typedefs_datadefQ(instance, mod, con->datadefQ, typnameQ);
    if (res != NO_ERR) {
        return res;
    }

    /* check any typedefs in any groupings in this object */
    res = xsd_do_typedefs_groupingQ(instance, mod, con->groupingQ, typnameQ);
    return res;

} /* do_typedefs_container */


/********************************************************************
* FUNCTION do_yang_elem_container
* 
*   Generate the element node for a container object type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == object to convert to XSD element
*    augtargobj == obj_template_t for the target of the augment clause
*            if this is a call from add_yang_augment.  This node will
*            be used to create the substitutionGroup for the top-level
*            element.  Set to NULL if not used.
*    val == struct parent to contain child nodes for each type
*           of the schema element to contain types if
*           this is a typonly
*
* OUTPUTS:
*    val->v.childQ has entries added for simple element
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_yang_elem_container (ncx_instance_t *instance,
                            ncx_module_t *mod,
                            obj_template_t *obj,
                            obj_template_t *augtargobj,
                            val_value_t *val)
{
    val_value_t      *elem, *cpx, *seq, *annot;
    obj_template_t   *child;
    status_t          res;
    xmlns_id_t        xsd_id;

    res = NO_ERR;
    xsd_id = xmlns_xs_id(instance);

    /* element struct of N arbitrary nodes */
    elem = xml_val_new_struct(instance, XSD_ELEMENT, xsd_id);
    if (!elem) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, elem, val);  /* add early */
    } 

    /* add the name attribute */
    res = xml_val_add_cattr(instance,
                            NCX_EL_NAME, 
                            0, 
                            obj_get_name(instance, obj), 
                            elem);
    if (res != NO_ERR) {
        return res;
    }

    if (1 < obj_get_level(instance, obj)) {
        ncx_iqual_t iqual = obj_get_iqualval(instance, obj);
        if (NCX_IQUAL_OPT == iqual) {
            res = xml_val_add_cattr(instance, 
                                    XSD_MIN_OCCURS, 
                                    0, 
                                    XSD_ZERO, 
                                    elem);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    /* check if a substitutionGroup is needed */
    if (augtargobj) {
        res = add_augtarget_subgrp(instance, augtargobj, elem);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* check if an annotation is needed for the element */
    annot = xsd_make_obj_annotation(instance, obj, &res);
    if (res != NO_ERR) {
        return res;
    } else if (annot) {
        val_add_child(instance, annot, elem);
    }

    /* next level is complexType */
    cpx = xml_val_new_struct(instance, XSD_CPX_TYP, xsd_id);
    if (!cpx) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, cpx, elem);  /* add early */
    }

    /* struct of N arbitrary nodes */
    seq = xml_val_new_struct(instance, XSD_SEQUENCE, xsd_id);
    if (!seq) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, seq, cpx);   /* add early */
    }

    /* go through all the child nodes and generate elements for them */
    for (child = (obj_template_t *)
             dlq_firstEntry(instance, obj_get_datadefQ(instance, obj));
         child != NULL && res==NO_ERR;
         child = (obj_template_t *)dlq_nextEntry(instance, child)) {
        res = do_yang_elem_btype(instance, mod, child, NULL, FALSE, seq);
    }

    if (res == NO_ERR) {
        res = xsd_add_aughook(instance, seq);
    }

    return res;

}  /* do_yang_elem_container */


/********************************************************************
* FUNCTION do_yang_elem_anyxml
* 
*   Generate the element node for an anyxml object type
*
* INPUTS:
*    obj == object to convert to XSD element
*    val == struct parent to contain child nodes
*
* OUTPUTS:
*    val->v.childQ has entries added for complex content (xs:anyType)
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_yang_elem_anyxml (ncx_instance_t *instance,
                         obj_template_t *obj,
                         val_value_t *val)
{
    val_value_t      *elem, *cpx, *cpxcon, *annot, *ext;
    xmlChar          *qname;
    status_t          res;
    xmlns_id_t        xsd_id;
    
    res = NO_ERR;
    xsd_id = xmlns_xs_id(instance);

    /* element struct of N arbitrary nodes */
    elem = xml_val_new_struct(instance, XSD_ELEMENT, xsd_id);
    if (!elem) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, elem, val);  /* add early */
    } 

    /* add the name attribute */
    res = xml_val_add_cattr(instance, 
                            NCX_EL_NAME, 
                            0, 
                            obj_get_name(instance, obj), 
                            elem);
    if (res != NO_ERR) {
        return res;
    }

    /* add the minOccurs="0" attribute */
    res = xml_val_add_cattr(instance, 
                            XSD_MIN_OCCURS, 
                            0, 
                            XSD_ZERO, 
                            elem);
    if (res != NO_ERR) {
        return res;
    }

    /* check if an annotation is needed for the element */
    annot = xsd_make_obj_annotation(instance, obj, &res);
    if (res != NO_ERR) {
        return res;
    } else if (annot) {
        val_add_child(instance, annot, elem);
    }

    /* next level is complexType */
    cpx = xml_val_new_struct(instance, XSD_CPX_TYP, xsd_id);
    if (!cpx) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, cpx, elem);  /* add early */
    }

    /* complex content */
    cpxcon = xml_val_new_struct(instance, XSD_CPX_CON, xsd_id);
    if (!cpxcon) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, cpxcon, cpx);   /* add early */
    }

    /* extension */
    ext = xml_val_new_flag(instance, XSD_EXTENSION, xsd_id);
    if (!ext) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, ext, cpxcon);   /* add early */
    }

    /* add baseType attribute */
    qname = xml_val_make_qname(instance, xsd_id, XSD_ANY_TYPE);
    if (!qname) {
        return ERR_INTERNAL_MEM;
    }

    /* pass off qname memory here */
    res = xml_val_add_attr(instance, XSD_BASE, 0, qname, ext);
    if (res != NO_ERR) {
        m__free(instance, qname);
    }
    
    return res;

}  /* do_yang_elem_anyxml */


/********************************************************************
* FUNCTION do_typedefs_list
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of typname_map_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each local type within this
*        list or any groups, or any children
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_typedefs_list (ncx_instance_t *instance,
                      ncx_module_t *mod,
                      obj_template_t *obj,
                      dlq_hdr_t *typnameQ)
{
    obj_list_t       *list;
    typ_template_t   *typ;
    status_t          res;

    list = obj->def.list;

    /* check any local typedefs in this list */
    for (typ = (typ_template_t *)dlq_firstEntry(instance, list->typedefQ);
         typ != NULL;
         typ = (typ_template_t *)dlq_nextEntry(instance, typ)) {
        res = add_type_mapping(instance, typ, typnameQ);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* check any local typedefs in any child objects */
    res = xsd_do_typedefs_datadefQ(instance, mod, list->datadefQ, typnameQ);
    if (res != NO_ERR) {
        return res;
    }

    /* check any typedefs in any groupings in this object */
    res = xsd_do_typedefs_groupingQ(instance, mod, list->groupingQ, typnameQ);
    return res;

} /* do_typedefs_list */


/********************************************************************
* FUNCTION do_yang_elem_list
* 
*   Generate the element node for a list object type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == object to convert to XSD element
*    augtargobj == obj_template_t for the target of the augment clause
*            if this is a call from add_yang_augment.  This node will
*            be used to create the substitutionGroup for the top-level
*            element.  Set to NULL if not used.
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for simple element
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_yang_elem_list (ncx_instance_t *instance,
                       ncx_module_t *mod,
                       obj_template_t *obj,
                       obj_template_t *augtargobj,
                       val_value_t *val)
{
    val_value_t       *elem, *cpx, *annot, *seq, *key, *chnode, *uniqval;
    obj_template_t    *child;
    obj_list_t        *list;
    obj_key_t         *idx;
    obj_unique_t      *uniq;
    obj_unique_comp_t *uniqcomp;
    xmlChar           *buff, *str;
    status_t           res;
    xmlns_id_t         xsd_id;
    boolean            iskey;
    uint32             uniqcnt, namelen;
    xmlChar            numbuff[NCX_MAX_NUMLEN];

    res = NO_ERR;
    xsd_id = xmlns_xs_id(instance);
    list = obj->def.list;

    elem = xml_val_new_struct(instance, XSD_ELEMENT, xsd_id);
    if (!elem) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, elem, val);
    }

    /* add the name attribute to the element */
    res = xml_val_add_cattr(instance, NCX_EL_NAME, 0, list->name, elem);
    if (res != NO_ERR) {
        return res;
    }

    /* check if a substitutionGroup is needed */
    if (augtargobj) {
        res = add_augtarget_subgrp(instance, augtargobj, elem);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* do not put minOccurs or maxOccurs on top-level elements */
    if (!obj_is_top(instance, obj)) {
        /* add minOccurs and maxOccurs attributes to the element */
        if (list->minset) {
            sprintf((char *)numbuff, "%u", list->minelems);
            res = xml_val_add_cattr(instance, XSD_MIN_OCCURS, 0, numbuff, elem);
        } else {
            res = xml_val_add_cattr(instance, XSD_MIN_OCCURS, 0, XSD_ZERO, elem);
        }
        if (res != NO_ERR) {
            return res;
        }
        if (list->maxset && list->maxelems) {
            sprintf((char *)numbuff, "%u", list->maxelems);
            res = xml_val_add_cattr(instance, XSD_MAX_OCCURS, 0, numbuff, elem);
        } else {
            res = xml_val_add_cattr(instance, XSD_MAX_OCCURS, 0, XSD_UNBOUNDED, elem);
        }
        if (res != NO_ERR) {
            return res;
        }
    }

    /* add an annotation node if needed */
    annot = xsd_make_obj_annotation(instance, obj, &res);
    if (res != NO_ERR) {
        return res;
    } else if (annot) {
        val_add_child(instance, annot, elem);
    }

    /* next level is complexType */
    cpx = xml_val_new_struct(instance, XSD_CPX_TYP, xsd_id);
    if (!cpx) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, cpx, elem);
    }

    /* next level is sequence */
    seq = xml_val_new_struct(instance, XSD_SEQUENCE, xsd_id);
    if (!seq) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, seq, cpx);
    }

    /* go through all the key nodes and generate elements for them */
    for (idx = (obj_key_t *)dlq_firstEntry(instance, &list->keyQ);
         idx != NULL;
         idx = (obj_key_t *)dlq_nextEntry(instance, idx)) {

        res = do_yang_elem_btype(instance, mod, idx->keyobj, NULL, TRUE, seq);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* go through all the child nodes and generate elements for them,
     * skipping any key leafs already done
     */
    for (child = (obj_template_t *)dlq_firstEntry(instance, list->datadefQ);
         child != NULL;
         child = (obj_template_t *)dlq_nextEntry(instance, child)) {

        if (child->objtype == OBJ_TYP_LEAF) {
            iskey = (obj_find_key(instance, &list->keyQ, obj_get_name(instance, child))) 
                ? TRUE : FALSE;
        } else {
            iskey = FALSE;
        }

        if (!iskey) {
            res = do_yang_elem_btype(instance, mod, child, NULL, iskey, seq);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    res = xsd_add_aughook(instance, seq);
    if (res != NO_ERR) {
        return res;
    }

    if (!dlq_empty(instance, &list->keyQ)) {
        /* create a 'key' node */
        key = xml_val_new_struct(instance, XSD_KEY, xsd_id);
        if (!key) {
            return ERR_INTERNAL_MEM;
        } else {
            val_add_child(instance, key, elem);
        }

        /* generate a key name */
        buff = m__getMem(instance, xml_strlen(instance, list->name) + 
                         xml_strlen(instance, XSD_KEY) + 
                         NCX_MAX_NUMLEN + 1);

        if (!buff) {
            return ERR_INTERNAL_MEM;
        }
        str = buff;
        str += xml_strcpy(instance, str, list->name);
        str += xml_strcpy(instance, str, XSD_KEY);
        sprintf((char *)str, "%u", get_next_seqnum());

        /* add the name attribute to the key node */
        res = xml_val_add_attr(instance, NCX_EL_NAME, 0, buff, key);
        if (res != NO_ERR) {
            m__free(instance, buff);
            return ERR_INTERNAL_MEM;
        }

        /* add the selector child node */
        chnode = xml_val_new_flag(instance, XSD_SELECTOR, xsd_id);
        if (!chnode) {
            return ERR_INTERNAL_MEM;
        } else {
            val_add_child(instance, chnode, key);   /* add early */
        }

#ifdef RIFT
        /* generate xpath for selector  */
        buff = m__getMem(instance, xml_strlen(instance, list->name) + 
                         xml_strlen(instance, XSD_KEY) + 
                         NCX_MAX_NUMLEN + 1);

        if (!buff) {
            return ERR_INTERNAL_MEM;
        }
        str = buff;
        str += xml_strcpy(instance, str, (const xmlChar *)".//");
        str += xml_strcpy(instance, str, list->name);

        /* selector is the current node
         * add the xpath attribute to the selector node
         */
        res = xml_val_add_cattr(instance, NCX_EL_XPATH, 0, 
                                buff, chnode);
#else
        /* selector is the current node
         * add the xpath attribute to the selector node
         */
        res = xml_val_add_cattr(instance, NCX_EL_XPATH, 0, 
                                (const xmlChar *)".", chnode);
#endif
        if (res != NO_ERR) {
            return res;
        }

        /* add all the field child nodes
         * go through all the index nodes as field nodes
         */
        for (idx = (obj_key_t *)dlq_firstEntry(instance, &list->keyQ);
             idx != NULL;
             idx = (obj_key_t *)dlq_nextEntry(instance, idx)) {

            /* add the selector child node */
            chnode = xml_val_new_flag(instance, XSD_FIELD, xsd_id);
            if (!chnode) {
                return ERR_INTERNAL_MEM;
            } else {
                val_add_child(instance, chnode, key);
            }

            /* add the xpath attribute to the field node */
            res = xml_val_add_cattr(instance, NCX_EL_XPATH, 0,
                                    obj_get_name(instance, idx->keyobj),
                                    chnode);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    
    /* add a unique element for each unique-stmt in this list */
    uniqcnt = 0;
    namelen = xml_strlen(instance, list->name);

    for (uniq = (obj_unique_t *)dlq_firstEntry(instance, &list->uniqueQ);
         uniq != NULL;
         uniq = (obj_unique_t *)dlq_nextEntry(instance, uniq)) {

        uniqval = xml_val_new_struct(instance, XSD_UNIQUE, xsd_id);
        if (!uniqval) {
            return ERR_INTERNAL_MEM;
        } else {
            val_add_child(instance, uniqval, elem);
        }

        sprintf((char *)numbuff, "%u", ++uniqcnt);
        
        /* generate a unique name */
        buff = m__getMem(instance, namelen +
                         xml_strlen(instance, XSD_UNIQUE_SUFFIX) +
                         xml_strlen(instance, numbuff) + 1);
        if (!buff) {
            return ERR_INTERNAL_MEM;
        }
        str = buff;
        str += xml_strcpy(instance, str, list->name);
        str += xml_strcpy(instance, str, XSD_UNIQUE_SUFFIX);
        str += xml_strcpy(instance, str, numbuff);

        /* add the name attribute to the unique node */
        res = xml_val_add_attr(instance, NCX_EL_NAME, 0, buff, uniqval);
        if (res != NO_ERR) {
            m__free(instance, buff);
            return ERR_INTERNAL_MEM;
        }

        /* add the selector child node */
        chnode = xml_val_new_flag(instance, XSD_SELECTOR, xsd_id);
        if (!chnode) {
            return ERR_INTERNAL_MEM;
        } else {
            val_add_child(instance, chnode, uniqval);   /* add early */
        }

        /* selector is the current node
         * add the xpath attribute to the selector node
         */
        res = xml_val_add_cattr(instance, NCX_EL_XPATH, 0, 
                                (const xmlChar *)".", chnode);
        if (res != NO_ERR) {
            return res;
        }

        /* add all the field child nodes
         * go through all the unique comp nodes as field nodes
         */
        for (uniqcomp = (obj_unique_comp_t *)dlq_firstEntry(instance, &uniq->compQ);
             uniqcomp != NULL;
             uniqcomp = (obj_unique_comp_t *)dlq_nextEntry(instance, uniqcomp)) {

            /* add the selector child node */
            chnode = xml_val_new_flag(instance, XSD_FIELD, xsd_id);
            if (!chnode) {
                return ERR_INTERNAL_MEM;
            } else {
                val_add_child(instance, chnode, uniqval);
            }

            /* add the xpath attribute to the field node */
            res = xml_val_add_cattr(instance, NCX_EL_XPATH, 0,
                                    uniqcomp->xpath, chnode);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    return res;

}  /* do_yang_elem_list */


/********************************************************************
* FUNCTION do_typedefs_case
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of typname_map_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each local type within any
*        children data definitions
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_typedefs_case (ncx_instance_t *instance,
                      ncx_module_t *mod,
                      obj_template_t *obj,
                      dlq_hdr_t *typnameQ)
{
    const obj_case_t       *cas;
    status_t                res;

    cas = obj->def.cas;

    /* check any local typedefs in any objects in any cases */
    res = xsd_do_typedefs_datadefQ(instance, mod, cas->datadefQ, typnameQ);
    return res;

} /* do_typedefs_case */


/********************************************************************
* FUNCTION do_typedefs_choice
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of typname_map_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each local type within any
*        children data definitions
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_typedefs_choice (ncx_instance_t *instance,
                        ncx_module_t *mod,
                        obj_template_t *obj,
                        dlq_hdr_t *typnameQ)
{
    const obj_choice_t     *choic;
    status_t               res;

    choic = obj->def.choic;
    res = xsd_do_typedefs_datadefQ(instance, mod, choic->caseQ, typnameQ);
    return res;

} /* do_typedefs_choice */


/********************************************************************
* FUNCTION do_yang_elem_choice
* 
*   Generate the element node for a choice object type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == object to convert to XSD element
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for simple element
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_yang_elem_choice (ncx_instance_t *instance,
                         ncx_module_t *mod,
                         obj_template_t *obj,
                         val_value_t *val)
{
    val_value_t          *top, *seq, *annot;
    obj_choice_t   *choic;
    obj_template_t *casobj, *child;
    obj_case_t     *cas;
    xmlns_id_t            xsd_id;
    status_t              res;

    choic = obj->def.choic;
    xsd_id = xmlns_xs_id(instance);
    res = NO_ERR;

    /* YANG choices do not take up a node, so start with a
     * xs:choice instead of a named element, then a choice
     */
    top = xml_val_new_struct(instance, NCX_EL_CHOICE, xsd_id);
    if (!top) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, top, val);  /* add early */
    }

    /* check if an annotation is needed for the element */
    annot = xsd_make_obj_annotation(instance, obj, &res);
    if (res != NO_ERR) {
        return res;
    } else if (annot) {
        val_add_child(instance, annot, top);
    }

    for (casobj = (obj_template_t *)dlq_firstEntry(instance, choic->caseQ);
         casobj != NULL && res==NO_ERR;
         casobj = (obj_template_t *)dlq_nextEntry(instance, casobj)) {

        
        /* next level is sequence */
        seq = xml_val_new_struct(instance, XSD_SEQUENCE, xsd_id);
        if (!seq) {
            return ERR_INTERNAL_MEM;
        } else {
            val_add_child(instance, seq, top);  /* add early */
        }

        /* check if an annotation is needed for the sequence */
        annot = xsd_make_obj_annotation(instance, casobj, &res);
        if (res != NO_ERR) {
            return res;
        } else if (annot) {
            val_add_child(instance, annot, seq);
        }

        cas = casobj->def.cas;
        for (child = (obj_template_t *)dlq_firstEntry(instance, cas->datadefQ);
             child != NULL && res==NO_ERR;
             child = (obj_template_t *)dlq_nextEntry(instance, child)) {
            res = do_yang_elem_btype(instance, mod, child, NULL, FALSE, seq);
        }
        if (res == NO_ERR) {
            res = xsd_add_aughook(instance, seq);
        }
    }

    if (res == NO_ERR) {
        res = xsd_add_aughook(instance, top);
    }

    return res;

}  /* do_yang_elem_choice */


/********************************************************************
* FUNCTION do_yang_elem_union
* 
*   Generate the element node for a union object type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == object to convert to XSD element
*    augtargobj == obj_template_t for the target of the augment clause
*            if this is a call from add_yang_augment.  This node will
*            be used to create the substitutionGroup for the top-level
*            element.  Set to NULL if not used.
*    iskey == TRUE if this is a key node, FALSE if not
*    val == struct parent to contain child nodes for each type
*             
* OUTPUTS:
*    val->v.childQ has entries added for simple element
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_yang_elem_union (ncx_instance_t *instance,
                        ncx_module_t *mod,
                        obj_template_t *obj,
                        obj_template_t *augtargobj,
                        boolean iskey,
                        val_value_t *val)

{

    val_value_t          *elem, *typnode, *annot;
    typ_def_t            *typdef;
    status_t              res;

    typdef = obj_get_typdef(obj);

    annot = xsd_make_obj_annotation(instance, obj, &res);
    if (res != NO_ERR) {
        return res;
    }

    elem = xsd_new_leaf_element(instance, mod, obj,
                                (annot) ? TRUE : FALSE,
                                (typdef->tclass == NCX_CL_NAMED),
                                iskey);
    if (!elem) {
        if (annot) {
            val_free_value(instance, annot);
        }
        return ERR_INTERNAL_MEM;
    }

    if (annot) {
        val_add_child(instance, annot, elem);
    }
    val_add_child(instance, elem, val);

    /* check if a substitutionGroup is needed */
    if (augtargobj) {
        res = add_augtarget_subgrp(instance, augtargobj, elem);
        if (res != NO_ERR) {
            return res;
        }
    }

    if (elem->btyp == NCX_BT_EMPTY) {
        return NO_ERR;
    }

    /* next level is simpleType */
    typnode = xml_val_new_struct(instance, XSD_SIM_TYP, xmlns_xs_id(instance));
    if (!typnode) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, typnode, elem);  /* add early */
    }

    /* generate the union of simpleTypes or list of memberTypes */
    res = xsd_finish_union(instance, mod, typdef, typnode);
    return res;

}  /* do_yang_elem_union */


/********************************************************************
* FUNCTION do_yang_elem_simple
* 
*   Generate the element node for a leaf object
*
* INPUTS:
*    mod == module conversion in progress
*    obj == object to convert to XSD element
*    augtargobj == obj_template_t for the target of the augment clause
*            if this is a call from add_yang_augment.  This node will
*            be used to create the substitutionGroup for the top-level
*            element.  Set to NULL if not used.
*    iskey == TRUE if this is a key node, FALSE if not
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for simple element
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_yang_elem_simple (ncx_instance_t *instance,
                         ncx_module_t *mod,
                         obj_template_t *obj,
                         obj_template_t *augtargobj,
                         boolean iskey,
                         val_value_t *val)

{
    typ_def_t           *typdef;
    val_value_t         *elem, *toptyp, *annot;
    typ_simple_t        *simtyp;
    boolean              empty, simtop;
    status_t             res;

    res = NO_ERR;
    empty = FALSE;
    simtop = TRUE;

    typdef = obj_get_typdef(obj);
    if (!typdef) {
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    /* check if an annotation is needed for the element */
    annot = xsd_make_obj_annotation(instance, obj, &res);
    if (res != NO_ERR) {
        return res;
    }

    res = test_basetype_attr(instance, mod, typdef);

    switch (typdef->tclass) {
    case NCX_CL_BASE:
        empty = TRUE;
        break;
    case NCX_CL_SIMPLE:
        simtyp = &typdef->def.simple;
        empty = (dlq_empty(instance, &simtyp->range.rangeQ) &&
                 dlq_empty(instance, &simtyp->unionQ) &&
                 dlq_empty(instance, &simtyp->valQ) &&
                 dlq_empty(instance, &simtyp->patternQ) &&
                 dlq_empty(instance, &simtyp->metaQ));
        break;
    case NCX_CL_COMPLEX:
        switch (typ_get_basetype(instance, typdef)) {
        case NCX_BT_ANY:
            empty = TRUE;
            simtop = FALSE;
            break;
        default:
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
        break;
    case NCX_CL_NAMED:
        if (typdef->def.named.newtyp) {
            simtyp = &typdef->def.named.newtyp->def.simple;
            empty = (dlq_empty(instance, &simtyp->range.rangeQ) &&
                     dlq_empty(instance, &simtyp->valQ) &&
                     dlq_empty(instance, &simtyp->patternQ) &&
                     dlq_empty(instance, &simtyp->metaQ));
        } else {
            empty = TRUE;
            switch (typ_get_basetype(instance, typdef)) {
            case NCX_BT_ANY:
                simtop = FALSE;
                break;
            default:
                break;
            }
        }
        break;
    default:
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    if (empty) {
        if (annot) {
            elem = xsd_new_leaf_element(instance, mod, obj, TRUE, TRUE, iskey);
            if (elem) {
                val_add_child(instance, annot, elem);
            }
        } else {
            /* get an empty element */
            elem = xsd_new_leaf_element(instance, mod, obj, FALSE, TRUE, iskey);
        }
    } else {
        elem = xsd_new_leaf_element(instance, mod, obj, TRUE, FALSE, iskey);
        if (elem && annot) {
            val_add_child(instance, annot, elem);
        }
    }
    if (!elem) {
        if (annot) {
            val_free_value(instance, annot);
        }
        return ERR_INTERNAL_MEM;
    }

    /* check if a substitutionGroup is needed */
    if (augtargobj) {
        res = add_augtarget_subgrp(instance, augtargobj, elem);
        if (res != NO_ERR) {
            return res;
        }
    }

    if (!empty) {
        if (simtop) {
            toptyp = xml_val_new_struct(instance, XSD_SIM_TYP, xmlns_xs_id(instance));
        } else {
            toptyp = xml_val_new_struct(instance, XSD_CPX_TYP, xmlns_xs_id(instance));
        }
        if (!toptyp) {
            val_free_value(instance, elem);
            return ERR_INTERNAL_MEM;
        } else {
            val_add_child(instance, toptyp, elem);  /* add early */
        }

        switch (typdef->tclass) {
        case NCX_CL_SIMPLE:
            res = xsd_finish_simpleType(instance, mod, typdef, toptyp);
            break;
        case NCX_CL_NAMED:
            res = xsd_finish_namedType(instance, mod, typdef, toptyp);
            break;
        default:
            res = SET_ERROR(instance, ERR_INTERNAL_VAL);
        }
        if (res != NO_ERR) {
            val_free_value(instance, elem);
            return res;
        }
    }

    val_add_child(instance, elem, val);
    return NO_ERR;

}   /* do_yang_elem_simple */


/********************************************************************
* FUNCTION do_yang_elem_btype
* 
*   Generate the element node for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    augtargobj == obj_template_t for the target of the augment clause
*            if this is a call from add_yang_augment.  This node will
*            be used to create the substitutionGroup for the top-level
*            element.  Set to NULL if not used.
*    iskey == TRUE if this is a key node, FALSE if not
*    val == struct parent to contain child nodes for each type
*
* OUTPUTS:
*    val->v.childQ has entries added for the child type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_yang_elem_btype (ncx_instance_t *instance,
                        ncx_module_t *mod,
                        obj_template_t *obj,
                        obj_template_t *augtargobj,
                        boolean iskey,
                        val_value_t *val)

{
    typ_def_t  *typdef;
    status_t    res;

    /* augmented objects from a different module or submodule should
     * be skipped, and handled in the module that defined the node
     *
     * may want to disable this check in the CLI to provide a complete
     * fully-augmented XSD for a given target, except there are other
     * details, like complete module tree mode instead of subtree mode
     */
    if (obj->tkerr.mod != mod) {
#ifndef RIFT
        return NO_ERR;
#endif
    }

    typdef = obj_get_typdef(obj);

    switch (obj->objtype) {
    case OBJ_TYP_CONTAINER:
        res = do_yang_elem_container(instance, mod, obj, augtargobj, val);
        break;
    case OBJ_TYP_ANYXML:
        res = do_yang_elem_anyxml(instance, obj, val);
        break;
    case OBJ_TYP_LEAF:
        if (typ_get_basetype(instance, typdef) == NCX_BT_UNION) {
            res = do_yang_elem_union(instance, mod, obj, augtargobj, iskey, val);
        } else {
            res = do_yang_elem_simple(instance, mod, obj, augtargobj, iskey, val);
        }
        break;
    case OBJ_TYP_LEAF_LIST:
        if (typ_get_basetype(instance, typdef) == NCX_BT_UNION) {
            res = do_yang_elem_union(instance, mod, obj, augtargobj, iskey, val);
        } else {
            res = do_yang_elem_simple(instance, mod, obj, augtargobj, iskey, val);
        }
        break;
    case OBJ_TYP_LIST:
        res = do_yang_elem_list(instance, mod, obj, augtargobj, val);
        break;
    case OBJ_TYP_CHOICE:
        /* XSD does not allow a choice to be the substitutionGroup
         * replacement for an abstract element, so ignoring choice
         * augment for now!!!
         */
        if (augtargobj) {
            log_debug(instance, 
                      "\nSkipping augment choice node "
                      "'%s', not supported in XSD", 
                      obj_get_name(instance, obj));
        }
        res = do_yang_elem_choice(instance, mod, obj, val);
        break;
    case OBJ_TYP_CASE:
        /* case objects not handled here */
        log_warn(instance, 
                      "\nSkipping case node "
                      "'%s', not supported in XSD", 
                      obj_get_name(instance, obj));
        res = NO_ERR;
        break;
    case OBJ_TYP_USES:
        /* skip all uses because it may be expanded with
         * possibly 1 or more modified child nodes, or
         * a node in the group may be in a key
         * or augmented somehow (inside the grouping or
         * inside the object tree
         *
         */
        res = NO_ERR;
        break;
    case OBJ_TYP_AUGMENT:
        /* skip this nested augment -- it is already been added
         * into the object tree and will be generated there
         */
        res = NO_ERR;
        break;
    case OBJ_TYP_RPC:
    case OBJ_TYP_RPCIO:
    case OBJ_TYP_NOTIF:
        res = SET_ERROR(instance, ERR_INTERNAL_VAL);
        break;
    default:
        res = SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    return res;

}   /* do_yang_elem_btype */


/********************************************************************
* FUNCTION do_typedefs_uses
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of typname_map_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each local type within any
*        children data definitions
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_typedefs_uses (ncx_instance_t *instance,
                      ncx_module_t *mod,
                      obj_template_t *obj,
                      dlq_hdr_t *typnameQ)
{
    obj_uses_t       *uses;
    status_t          res;

    uses = obj->def.uses;
    res = xsd_do_typedefs_datadefQ(instance, mod, uses->datadefQ, typnameQ);
    return res;

} /* do_typedefs_uses */


/********************************************************************
* FUNCTION do_typedefs_augment
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of typname_map_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each local type within any
*        children data definitions
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_typedefs_augment (ncx_instance_t *instance,
                         ncx_module_t *mod,
                         obj_template_t *obj,
                         dlq_hdr_t *typnameQ)
{
    obj_augment_t    *aug;
    status_t          res;

    aug = obj->def.augment;
    res = xsd_do_typedefs_datadefQ(instance, mod, &aug->datadefQ, typnameQ);
    return res;

} /* do_typedefs_augment */


/********************************************************************
* FUNCTION do_typedefs_rpc
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of typname_map_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each local type within this
*        RPC or any groups, or any children
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_typedefs_rpc (ncx_instance_t *instance,
                     ncx_module_t *mod,
                     obj_template_t *obj,
                     dlq_hdr_t *typnameQ)
{
    obj_rpc_t        *rpc;
    typ_template_t   *typ;
    status_t          res;

    rpc = obj->def.rpc;

    /* check any local typedefs in this RPC */
    for (typ = (typ_template_t *)dlq_firstEntry(instance, &rpc->typedefQ);
         typ != NULL;
         typ = (typ_template_t *)dlq_nextEntry(instance, typ)) {
        res = add_type_mapping(instance, typ, typnameQ);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* check any local typedefs in any child objects */
    res = xsd_do_typedefs_datadefQ(instance, mod, &rpc->datadefQ, typnameQ);
    if (res != NO_ERR) {
        return res;
    }

    /* check any typedefs in any groupings in this RPC */
    res = xsd_do_typedefs_groupingQ(instance, mod, &rpc->groupingQ, typnameQ);
    return res;

} /* do_typedefs_rpc */


/********************************************************************
* FUNCTION do_typedefs_rpcio
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of typname_map_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each local type within this
*        RPCIO or any groups, or any children
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_typedefs_rpcio (ncx_instance_t *instance,
                       ncx_module_t *mod,
                       obj_template_t *obj,
                       dlq_hdr_t *typnameQ)
{
    obj_rpcio_t      *rpcio;
    typ_template_t   *typ;
    status_t          res;

    rpcio = obj->def.rpcio;

    /* check any local typedefs in this RPC input/output */
    for (typ = (typ_template_t *)dlq_firstEntry(instance, &rpcio->typedefQ);
         typ != NULL;
         typ = (typ_template_t *)dlq_nextEntry(instance, typ)) {
        res = add_type_mapping(instance, typ, typnameQ);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* check any local typedefs in any child objects */
    res = xsd_do_typedefs_datadefQ(instance, mod, &rpcio->datadefQ, typnameQ);
    if (res != NO_ERR) {
        return res;
    }

    /* check any typedefs in any groupings in this RPC */
    res = xsd_do_typedefs_groupingQ(instance, mod, &rpcio->groupingQ, typnameQ);
    return res;

} /* do_typedefs_rpcio */


/********************************************************************
* FUNCTION do_typedefs_notif
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of typname_map_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each local type within this
*        notification or any groups, or any children
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_typedefs_notif (ncx_instance_t *instance,
                       ncx_module_t *mod,
                       obj_template_t *obj,
                       dlq_hdr_t *typnameQ)
{
    obj_notif_t      *notif;
    typ_template_t   *typ;
    status_t          res;

    notif = obj->def.notif;

    /* check any local typedefs in this notification */
    for (typ = (typ_template_t *)dlq_firstEntry(instance, &notif->typedefQ);
         typ != NULL;
         typ = (typ_template_t *)dlq_nextEntry(instance, typ)) {
        res = add_type_mapping(instance, typ, typnameQ);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* check any local typedefs in any child objects */
    res = xsd_do_typedefs_datadefQ(instance, mod, &notif->datadefQ, typnameQ);
    if (res != NO_ERR) {
        return res;
    }

    /* check any typedefs in any groupings in this RPC */
    res = xsd_do_typedefs_groupingQ(instance, mod, &notif->groupingQ, typnameQ);
    return res;

} /* do_typedefs_notif */


/********************************************************************
* FUNCTION do_local_typedefs
* 
*   Generate the typedef elements for any child type
*
* INPUTS:
*    mod == module conversion in progress
*    obj == obj_template_t for the child node to use
*    typnameQ == Q of xsd_typname_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each type
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    do_local_typedefs (ncx_instance_t *instance,
                       ncx_module_t *mod,
                       obj_template_t *obj,
                       dlq_hdr_t *typnameQ)
{
    status_t   res;

    if (obj_is_cloned(instance, obj)) {
        return NO_ERR;
    }

    res = NO_ERR;

    switch (obj->objtype) {
    case OBJ_TYP_CONTAINER:
        res = do_typedefs_container(instance, mod, obj, typnameQ);
        break;
    case OBJ_TYP_ANYXML:
    case OBJ_TYP_LEAF:
    case OBJ_TYP_LEAF_LIST:
        res = NO_ERR;
        break;
    case OBJ_TYP_LIST:
        res = do_typedefs_list(instance, mod, obj, typnameQ);
        break;
    case OBJ_TYP_CHOICE:
        res = do_typedefs_choice(instance, mod, obj, typnameQ);
        break;
    case OBJ_TYP_CASE:
        res = do_typedefs_case(instance, mod, obj, typnameQ);
        break;
    case OBJ_TYP_USES:
        res = do_typedefs_uses(instance, mod, obj, typnameQ);
        break;
    case OBJ_TYP_AUGMENT:
        res = do_typedefs_augment(instance, mod, obj, typnameQ);
        break;
    case OBJ_TYP_RPC:
        res = do_typedefs_rpc(instance, mod, obj, typnameQ);
        break;
    case OBJ_TYP_RPCIO:
        res = do_typedefs_rpcio(instance, mod, obj, typnameQ);
        break;
    case OBJ_TYP_NOTIF:
        res = do_typedefs_notif(instance, mod, obj, typnameQ);
        break;
    case OBJ_TYP_REFINE:
        break;
    default:
        res = SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    return res;

}   /* do_local_typedefs */


/********************************************************************
* FUNCTION add_yang_rpcio
* 
*   Add the complexType subtree for input or output parameters
*
* INPUTS:
*    mod == module in progress
*    obj == RPC object to add in XSD format
*    iobj == input or output object
*    addtypename == TRUE if a type name attributes should be added
*       to the complexType or FALSE if not
*    val == struct parent to contain the parameters
*
* OUTPUTS:
*    val->childQ has one entry added for the the RPC function
*         and maybe one complexType for the RPC output
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_yang_rpcio (ncx_instance_t *instance,
                    ncx_module_t *mod,
                    obj_template_t *obj,
                    obj_template_t *iobj,
                    boolean addtypename,
                    val_value_t *val)
{
    val_value_t            *cpxtyp, *cpxcon, *ext, *seq, *annot;
    obj_template_t         *chobj;
    obj_rpcio_t            *rpcio;
    xmlChar                *typename, *qname;
    xmlns_id_t              xsd_id, nc_id;
    status_t                res;

    xsd_id = xmlns_xs_id(instance);
    nc_id = xmlns_nc_id(instance);
    rpcio = (iobj) ? iobj->def.rpcio : NULL;

    cpxtyp = xml_val_new_struct(instance, XSD_CPX_TYP, xsd_id);
    if (!cpxtyp) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, cpxtyp, val);
    }

    if (addtypename) {
        typename =  xsd_make_rpc_output_typename(instance, obj);
        if (!typename) {
            return ERR_INTERNAL_MEM;
        }

        res = xml_val_add_attr(instance, NCX_EL_NAME, 0, typename, cpxtyp);
        if (res != NO_ERR) {
            m__free(instance, typename);
            return res;
        }
    }

    if (iobj) {
        annot = xsd_make_obj_annotation(instance, iobj, &res);
        if (res != NO_ERR) {
            return res;
        } else if (annot) {
            val_add_child(instance, annot, cpxtyp);
        }
    }

    cpxcon = xml_val_new_struct(instance, XSD_CPX_CON, xsd_id);
    if (!cpxcon) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, cpxcon, cpxtyp);
    }
    
    ext = xml_val_new_struct(instance, XSD_EXTENSION, xsd_id);
    if (!ext) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, ext, cpxcon);
    }

    if (addtypename) {
        qname = xml_val_make_qname(instance, nc_id, XSD_DATA_INLINE);
    } else {
        qname = xml_val_make_qname(instance, nc_id, XSD_RPC_OPTYPE);
    }
    if (!qname) {
        return ERR_INTERNAL_MEM;
    }
        
    res = xml_val_add_attr(instance, XSD_BASE, 0, qname, ext);
    if (res != NO_ERR) {
        m__free(instance, qname);
        return res;
    }

    seq = xml_val_new_struct(instance, XSD_SEQUENCE, xsd_id);
    if (!seq) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, seq, ext);
    }

    if (rpcio) {
        for (chobj = (obj_template_t *)dlq_firstEntry(instance, &rpcio->datadefQ);
             chobj != NULL;
             chobj = (obj_template_t *)dlq_nextEntry(instance, chobj)) {
            res = do_yang_elem_btype(instance, mod, chobj, NULL, FALSE, seq);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    res = xsd_add_aughook(instance, seq);
    return res;

} /* add_yang_rpcio */


/********************************************************************
* FUNCTION add_yang_rpc
* 
*   Add one or 2 elements representing an RPC function
*
* INPUTS:
*    mod == module in progress
*    obj == RPC object to add in XSD format
*    val == struct parent to contain the RPC function element
*
* OUTPUTS:
*    val->childQ has one entry added for the the RPC function
*         and maybe one complexType for the RPC output
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_yang_rpc (ncx_instance_t *instance,
                  ncx_module_t *mod,
                  obj_template_t *obj,
                  val_value_t *val)
{
    val_value_t          *elem, *annot;
    obj_template_t       *inputobj, *outputobj;
    xmlChar              *qname;
    xmlns_id_t            xsd_id, nc_id;
    status_t              res;

    xsd_id = xmlns_xs_id(instance);
    nc_id = xmlns_nc_id(instance);

    inputobj = obj_find_template(instance,
                                 obj_get_datadefQ(instance, obj),
                                 NULL,
                                 YANG_K_INPUT);
    outputobj = obj_find_template(instance,
                                  obj_get_datadefQ(instance, obj),
                                  NULL,
                                  YANG_K_OUTPUT);

    /* add a named typedef for the output if needed */
    if (outputobj) {
        res = add_yang_rpcio(instance, mod, obj, outputobj, TRUE, val);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* An RPC method is simply an element that
     * hooks into the NETCONF rpcOperation element
     */
    elem = xml_val_new_struct(instance, XSD_ELEMENT, xsd_id);
    if (!elem) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, elem, val);   /* add early */
    }

    /* add the name attribute */
    res = xml_val_add_cattr(instance, 
                            NCX_EL_NAME, 
                            0, 
                            obj_get_name(instance, obj),
                            elem);
    if (res != NO_ERR) {
        return res;
    }

    /* create the NETCONF rpcOperation QName */
    qname = xml_val_make_qname(instance, nc_id, XSD_RPC_OP);
    if (!qname) {
        return res;
    }

    /* add the substitutionGroup attribute */
    res = xml_val_add_attr(instance, XSD_SUB_GRP, 0, qname, elem);
    if (res != NO_ERR) {
        m__free(instance, qname);
        return res;
    }

    /* add the object annotation if any */
    annot = xsd_make_obj_annotation(instance, obj, &res);
    if (res != NO_ERR) {
        return res;
    } else if (annot) {
        val_add_child(instance, annot, elem);
    }

    /* add an inline typedef for the input
     * an extension node is generated even if inputobj is NULL
     */
    res = add_yang_rpcio(instance, mod, obj, inputobj, FALSE, elem);
    return res;

} /* add_yang_rpc */


/********************************************************************
* FUNCTION add_yang_notif
* 
*   Add one element representing a notification
*
* INPUTS:
*    mod == module in progress
*    obj == notification object to add in XSD format
*    val == struct parent to contain the notification element
*
* OUTPUTS:
*    val->childQ has one entry added for the the notification
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_yang_notif (ncx_instance_t *instance,
                    ncx_module_t *mod,
                    obj_template_t *obj,
                    val_value_t *val)
{
    val_value_t          *elem, *annot, *cpxtyp, *cpxcon, *ext, *seq;
    obj_notif_t          *notif;
    obj_template_t       *chobj;
    xmlChar              *qname;
    xmlns_id_t            xsd_id, ncn_id;
    status_t              res;

    xsd_id = xmlns_xs_id(instance);
    ncn_id = xmlns_ncn_id(instance);

    notif = obj->def.notif;

    /* A notification is simply an element that
     * hooks into the NETCONF notificationrpcOperation element
     */
    elem = xml_val_new_struct(instance, XSD_ELEMENT, xsd_id);
    if (!elem) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, elem, val);   /* add early */
    }

    /* add the name attribute */
    res = xml_val_add_cattr(instance, NCX_EL_NAME, 0, notif->name, elem);
    if (res != NO_ERR) {
        return res;
    }

    /* create the NETCONF notificationContent QName */
    qname = xml_val_make_qname(instance, ncn_id, XSD_NOTIF_CONTENT);
    if (!qname) {
        return res;
    }

    /* add the substitutionGroup attribute */
    res = xml_val_add_attr(instance, XSD_SUB_GRP, 0, qname, elem);
    if (res != NO_ERR) {
        m__free(instance, qname);
        return res;
    }

    /* add the object annotation if any */
    annot = xsd_make_obj_annotation(instance, obj, &res);
    if (res != NO_ERR) {
        return res;
    } else if (annot) {
        val_add_child(instance, annot, elem);
    }

    /* next node is complexType */
    cpxtyp = xml_val_new_struct(instance, XSD_CPX_TYP, xsd_id);
    if (!cpxtyp) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, cpxtyp, elem);
    }

    /* next node is complexContent */
    cpxcon = xml_val_new_struct(instance, XSD_CPX_CON, xsd_id);
    if (!cpxcon) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, cpxcon, cpxtyp);
    }
    
    /* next node is extension */
    ext = xml_val_new_struct(instance, XSD_EXTENSION, xsd_id);
    if (!ext) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, ext, cpxcon);
    }
    qname = xml_val_make_qname(instance, ncn_id, XSD_NOTIF_CTYPE);
    if (!qname) {
        return ERR_INTERNAL_MEM;
    }
    res = xml_val_add_attr(instance, XSD_BASE, 0, qname, ext);
    if (res != NO_ERR) {
        m__free(instance, qname);
        return res;
    }

    /* next node is sequence */
    seq = xml_val_new_struct(instance, XSD_SEQUENCE, xsd_id);
    if (!seq) {
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(instance, seq, ext);
    }

    /* add any data defined for this notification */
    for (chobj = (obj_template_t *)dlq_firstEntry(instance, &notif->datadefQ);
         chobj != NULL;
         chobj = (obj_template_t *)dlq_nextEntry(instance, chobj)) {
        res = do_yang_elem_btype(instance, mod, chobj, NULL, FALSE, seq);
        if (res != NO_ERR) {
            return res;
        }
    }

    res = xsd_add_aughook(instance, seq);
    return res;

} /* add_yang_notif */


/********************************************************************
* FUNCTION add_yang_augment
* 
*   Add one element representing a top-level augment for
*   a target in a different namespace.  Only top-level augment
*   clauses can have this form.  Nested YANG augment-stmts
*   are ignored for XSD purposes, since the cloned objects
*   will get generated instead.
*
* INPUTS:
*    mod == module in progress
*    obj == augment object to add in XSD format
*    val == struct parent to contain the augment element
*
* OUTPUTS:
*    val->childQ has one entry added for the the augmenting element
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    add_yang_augment (ncx_instance_t *instance,
                      ncx_module_t *mod,
                      obj_template_t *obj,
                      val_value_t *val)
{
    obj_augment_t     *aug;
    obj_template_t    *chobj;
    status_t           res;

    aug = obj->def.augment;

    /* check that the target is in another namespace */
    if (!aug->targobj || !aug->targobj->tkerr.mod ||
        (aug->targobj->tkerr.mod->nsid == mod->nsid)) {
        return NO_ERR;
    }

    /* A top-level augment is a top-level element that is a substitutionGroup
     * replacement for the abstract element identified in the augment target.
     * Each datadef node declared in the augment clause generates a top-level
     * element to replace the abstract element representing the target.
     */
    for (chobj = (obj_template_t *)dlq_firstEntry(instance, &aug->datadefQ);
         chobj != NULL;
         chobj = (obj_template_t *)dlq_nextEntry(instance, chobj)) {

        res = do_yang_elem_btype(instance, mod, chobj, aug->targobj, FALSE, val);
        if (res != NO_ERR) {
            return res;
        }
    }

    return NO_ERR;

} /* add_yang_augment */


/************* E X T E R N A L   F U N C T I O N S *****************/


/********************************************************************
* FUNCTION xsd_add_groupings
* 
*   Add the required group nodes
*
* INPUTS:
*    mod == module in progress
*    val == struct parent to contain child nodes for each group
*
* OUTPUTS:
*    val->childQ has entries added for the groupings required
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xsd_add_groupings (ncx_instance_t *instance,
                       ncx_module_t *mod,
                       val_value_t *val)
{
    grp_template_t *grp;
    obj_template_t *obj;
    val_value_t    *grpval, *seqval, *annot;
    xmlns_id_t      xsd_id;
    status_t        res;

    xsd_id = xmlns_xs_id(instance);

    /* generate top-level groupings only */
    for (grp = (grp_template_t *)dlq_firstEntry(instance, &mod->groupingQ);
         grp != NO_ERR;
         grp = (grp_template_t *)dlq_nextEntry(instance, grp)) {

        /* check if an annotation node is needed */
        annot = xsd_make_group_annotation(instance, grp, &res);
        if (res != NO_ERR) {
            return res;
        }

        /* check if an empty group or regular group is needed */
        if (!annot && dlq_empty(instance, &grp->datadefQ)) {
            grpval = xml_val_new_flag(instance, XSD_GROUP, xsd_id);
        } else {
            grpval = xml_val_new_struct(instance, XSD_GROUP, xsd_id);
        }
        if (!grpval) {
            if (annot) {
                val_free_value(instance, annot);
            }
            return ERR_INTERNAL_MEM;
        } else {
            val_add_child(instance, grpval, val);
            if (annot) {
                val_add_child(instance, annot, grpval);
            }
        }
        
        /* set the group name */
        res = xml_val_add_cattr(instance, NCX_EL_NAME, 0, grp->name, grpval);
        if (res != NO_ERR) {
            return res;
        }

        /* generate all the objects in the datadefQ */
        if (!dlq_empty(instance, &grp->datadefQ)) {
            seqval = xml_val_new_struct(instance, XSD_SEQUENCE, xsd_id);
            if (!seqval) {
                return ERR_INTERNAL_MEM;
            } else {
                val_add_child(instance, seqval, grpval);
            }

            for (obj = (obj_template_t *)dlq_firstEntry(instance, &grp->datadefQ);
                 obj != NULL;
                 obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {
                res = do_yang_elem_btype(instance, mod, obj, NULL, FALSE, seqval);
                if (res != NO_ERR) {
                    return res;
                }
            }
        }
    }

    return NO_ERR;

}   /* xsd_add_groupings */


/********************************************************************
* FUNCTION xsd_add_objects
* 
*   Add the required element nodes for each object in the module
*   RPC methods and notifications are mixed in with the objects
*
* INPUTS:
*    mod == module in progress
*    val == struct parent to contain child nodes for each object
*
* OUTPUTS:
*    val->childQ has entries added for the RPC methods in this module
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xsd_add_objects (ncx_instance_t *instance,
                     ncx_module_t *mod,
                     val_value_t *val)
{
    obj_template_t   *obj;
    status_t          res;

    /* go through all the objects and create complexType constructs */
    for (obj = (obj_template_t *)dlq_firstEntry(instance, &mod->datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {

        if (obj_is_hidden(instance, obj)) {
            continue;
        }

        switch (obj->objtype) {
        case OBJ_TYP_RPC:
            res = add_yang_rpc(instance, mod, obj, val);
            break;
        case OBJ_TYP_NOTIF:
            res = add_yang_notif(instance, mod, obj, val);
            break;
        case OBJ_TYP_AUGMENT:
            res = add_yang_augment(instance, mod, obj, val);
            break;
        case OBJ_TYP_USES:
            /* A uses-stmt is ignored in XSD translation and only the
             * cloned objects from the uses expansion are included instead
             */
            res = NO_ERR;
            break;
        default:
            res = do_yang_elem_btype(instance, mod, obj, NULL, FALSE, val);
        }
        if (res != NO_ERR) {
            return res;
        }

    }

    return NO_ERR;

}   /* xsd_add_objects */


/********************************************************************
* FUNCTION xsd_do_typedefs_groupingQ
* 
* Analyze the entire 'groupingQ' within the module struct
* Generate local typedef mappings as needed
*
* INPUTS:
*    mod == module conversion in progress
*    groupingQ == Q of grp_template_t structs to check
*    typnameQ == Q of xsd_typname_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each type
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xsd_do_typedefs_groupingQ (ncx_instance_t *instance,
                               ncx_module_t *mod,
                               dlq_hdr_t *groupingQ,
                               dlq_hdr_t *typnameQ)
{
    grp_template_t  *grp;
    typ_template_t  *typ;
    obj_template_t  *obj;
    status_t         res;

    /* go through the groupingQ and check the local types/groupings
     * and data-def statements
     */
    for (grp = (grp_template_t *)dlq_firstEntry(instance, groupingQ);
         grp != NULL;
         grp = (grp_template_t *)dlq_nextEntry(instance, grp)) {

        /* check any local typedefs in this grouping */
        for (typ = (typ_template_t *)dlq_firstEntry(instance, &grp->typedefQ);
             typ != NULL;
             typ = (typ_template_t *)dlq_nextEntry(instance, typ)) {
            res = add_type_mapping(instance, typ, typnameQ);
            if (res != NO_ERR) {
                return res;
            }
        }

        /* check any local typedefs in the objects in this grouping */
        for (obj = (obj_template_t *)dlq_firstEntry(instance, &grp->datadefQ);
             obj != NULL;
             obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {
            res = do_local_typedefs(instance, mod, obj, typnameQ);
            if (res != NO_ERR) {
                return res;
            }
        }

        /* check any nested groupings */
        res = xsd_do_typedefs_groupingQ(instance, mod, &grp->groupingQ, typnameQ);
        if (res != NO_ERR) {
            return res;
        }
    }

    return NO_ERR;

}  /* xsd_do_typedefs_groupingQ */


/********************************************************************
* FUNCTION xsd_do_typedefs_datadefQ
* 
* Analyze the entire 'datadefQ' within the module struct
* Generate local typedef mappings as needed
*
* INPUTS:
*    mod == module conversion in progress
*    datadefQ == Q of obj_template_t structs to check
*    typnameQ == Q of xsd_typname_t to hold new local type names
*
* OUTPUTS:
*    typnameQ has entries added for each type
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xsd_do_typedefs_datadefQ (ncx_instance_t *instance,
                              ncx_module_t *mod,
                              dlq_hdr_t *datadefQ,
                              dlq_hdr_t *typnameQ)
{
    obj_template_t  *obj;
    status_t         res;

    for (obj = (obj_template_t *)dlq_firstEntry(instance, datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(instance, obj)) {
        res = do_local_typedefs(instance, mod, obj, typnameQ);
        if (res != NO_ERR) {
            return res;
        }
    }

    return NO_ERR;

}  /* xsd_do_typedefs_datadefQ */


/* END file xsd_yang.c */
