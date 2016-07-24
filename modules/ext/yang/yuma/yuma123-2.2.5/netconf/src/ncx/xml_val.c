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
/*  FILE: xml_val.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
24nov06      abb      begun; split from xsd.c
16jan07      abb      spit core functions from ncxdump/xml_val_util.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <memory.h>
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

#ifndef _H_ncx
#include "ncx.h"
#endif

#ifndef _H_ncx_num
#include "ncx_num.h"
#endif

#ifndef _H_ncxtypes
#include "ncxtypes.h"
#endif

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncxmod
#include "ncxmod.h"
#endif

#ifndef _H_rpc
#include "rpc.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_tstamp
#include "tstamp.h"
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


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#define DATETIME_BUFFSIZE  64


/********************************************************************
*                                                                   *
*                           T Y P E S                               *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

/************ L O C A L F U N C T I O N S   ******/

/********************************************************************
* FUNCTION new_string_attr
* 
*   Set up a new string val 
*
* INPUTS:
*    name == attr name
*    nsid == namespace ID of attr
*    str  == the string value to use
*
* RETURNS:
*   A newly created string value or NULL
*********************************************************************/
static val_value_t*
    new_string_attr(ncx_instance_t *instance, 
                      const xmlChar    *name, 
                     const xmlns_id_t  nsid,
                     xmlChar          *strval )
{
    val_value_t *newval;

    newval = val_new_value(instance);
    if (!newval) {
        return NULL;
    }   

    newval->btyp = NCX_BT_STRING;
    newval->typdef = typ_get_basetype_typdef(instance, NCX_BT_STRING);
    newval->name = name;
    newval->nsid = nsid;
    newval->v.str = strval;

    return newval;
}

/************ E X T E R N A L    F U N C T I O N S   ******/

/********************************************************************
* FUNCTION xml_val_make_qname
* 
*   Build output value: add child node to a struct node
*   Malloc a string buffer and create a QName string
*   This is complete; The m__free function must be called
*   with the return value if it is non-NULL;
*
* INPUTS:
*    nsid == namespace ID to use
*    name == condition clause (may be NULL)
*
* RETURNS:
*   malloced value string or NULL if malloc error
*********************************************************************/
xmlChar *
    xml_val_make_qname (ncx_instance_t *instance,
                        xmlns_id_t  nsid,
                        const xmlChar *name)
{
    xmlChar          *str, *str2;
    const xmlChar    *pfix;
    uint32            len;

    pfix = xmlns_get_ns_prefix(instance, nsid);
    if (!pfix) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);   /* catch no namespace error */
        return xml_strdup(instance, name);
    }

    len = xml_strlen(instance, name) + xml_strlen(instance, pfix) + 2;
    str = m__getMem(instance, len);
    if (!str) {
        return NULL;
    }

    str2 = str;
    str2 += xml_strcpy(instance, str2, pfix);
    *str2++ = ':';
    str2 += xml_strcpy(instance, str2, name);

    return str;

}   /* xml_val_make_qname */


/********************************************************************
* FUNCTION xml_val_qname_len
* 
*   Determine the length of the qname string that would be generated
*   with the xml_val_make_qname function
*
* INPUTS:
*    nsid == namespace ID to use
*    name == condition clause (may be NULL)
*
* RETURNS:
*   length of string needed for this QName
*********************************************************************/
uint32
    xml_val_qname_len (ncx_instance_t *instance,
                       xmlns_id_t  nsid,
                       const xmlChar *name)
{
    const xmlChar    *pfix;

    pfix = xmlns_get_ns_prefix(instance, nsid);
    if (!pfix) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);   /* catch no namespace error */
        return xml_strlen(instance, name);
    }

    return xml_strlen(instance, name) + xml_strlen(instance, pfix) + 1;

}   /* xml_val_qname_len */


/********************************************************************
* FUNCTION xml_val_sprintf_qname
* 
*   construct a QName into a buffer
*
* INPUTS:
*    buff == buffer
*    bufflen == size of buffer
*    nsid == namespace ID to use
*    name == condition clause (may be NULL)
*
* RETURNS:
*   number of bytes written to the buffer
*********************************************************************/
uint32
    xml_val_sprintf_qname (ncx_instance_t *instance,
                           xmlChar *buff,
                           uint32 bufflen,
                           xmlns_id_t  nsid,
                           const xmlChar *name)
{
    xmlChar          *str;
    const xmlChar    *pfix;
    uint32            len;

    pfix = xmlns_get_ns_prefix(instance, nsid);
    if (!pfix) {
        SET_ERROR(instance, ERR_INTERNAL_VAL);   /* catch no namespace error */
        return 0;
    }

    len = xml_strlen(instance, name) + xml_strlen(instance, pfix) + 2;
    if (len > bufflen) {
        SET_ERROR(instance, ERR_BUFF_OVFL);
        return 0;
    }

    /* construct the QName string */
    str = buff;
    str += xml_strcpy(instance, str, pfix);
    *str++ = ':';
    str += xml_strcpy(instance, str, name);

    return len-1;

}   /* xml_val_sprintf_qname */


/********************************************************************
* FUNCTION xml_val_add_attr
* 
*   Set up a new attr val and add it to the specified val
*   hand off a malloced attribute string
*
* INPUTS:
*    name == attr name
*    nsid == namespace ID of attr
*    attrval  == attr val to add (do not use strdup)
*    val == parent val struct to hold the new attr
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xml_val_add_attr (ncx_instance_t *instance,
                      const xmlChar *name,
                      xmlns_id_t nsid,
                      xmlChar *attrval,
                      val_value_t *val)
{
    val_value_t *newval;

    if (!val) {
        return ERR_INTERNAL_MEM;
    }

    /* create a new value to hold the attribute name value pair */
    newval = new_string_attr(instance,  name, nsid, attrval );
    if (!newval) {
        return ERR_INTERNAL_MEM;
    }

    dlq_enque(instance, newval, &val->metaQ);
    return NO_ERR;
}   /* xml_val_add_attr */


/********************************************************************
* FUNCTION xml_val_add_cattr
* 
*   Set up a new const attr val and add it to the specified val
*   copy a const attribute string
*
* INPUTS:
*    name == attr name
*    nsid == namespace ID of attr
*    cattrval  == const attr val to add (use strdup)
*    val == parent val struct to hold the new attr
*
* RETURNS:
*   status
*********************************************************************/
status_t
    xml_val_add_cattr (ncx_instance_t *instance,
                       const xmlChar *name,
                       xmlns_id_t nsid,
                       const xmlChar *cattrval,
                       val_value_t *val)
{
    xmlChar     *str;
    status_t     res;
    str = xml_strdup(instance,  cattrval );
    if (!str) {
        return ERR_INTERNAL_MEM;
    }

    res = xml_val_add_attr(instance,  name, nsid, str, val );
    if ( NO_ERR != res ) {
        m__free(instance,  str );
    }
    return res;
}   /* xml_val_add_cattr */

/********************************************************************
* FUNCTION xml_val_new_struct
* 
*   Set up a new struct
*
* INPUTS:
*    name == element name
*    nsid == namespace ID of name
* 
* RETURNS:
*   new struct or NULL if malloc error
*********************************************************************/
val_value_t *
    xml_val_new_struct (ncx_instance_t *instance,
                        const xmlChar *name,
                        xmlns_id_t     nsid)
{
    val_value_t *val;

    val = val_new_value(instance);
    if (!val) {
        return NULL;
    }
    val_init_complex(instance, val, NCX_BT_CONTAINER);
    val->typdef = typ_get_basetype_typdef(instance, NCX_BT_CONTAINER);
    val->name = name;
    val->nsid = nsid;
    val->obj = ncx_get_gen_container(instance);

    return val;

}   /* xml_val_new_struct */


/********************************************************************
* FUNCTION xml_val_new_string
* 
*  Set up a new string element; reuse the value instead of copying it
*  hand off a malloced string
*
* INPUTS:
*    name == element name
*    nsid == namespace ID of name
*    strval == malloced string value that will be freed later
* 
* RETURNS:
*   new string or NULL if malloc error
*********************************************************************/
val_value_t *
    xml_val_new_string (ncx_instance_t *instance,
                        const xmlChar *name,
                        xmlns_id_t     nsid,
                        xmlChar *strval)
{
    val_value_t *val;

    val = new_string_attr(instance,  name, nsid, strval );
    if (!val) {
        return NULL;
    }
    val->obj = ncx_get_gen_string(instance);
    return val;
}   /* xml_val_new_string */


/********************************************************************
* FUNCTION xml_val_new_cstring
* 
*   Set up a new string from a const string
*   copy a const string
*
* INPUTS:
*    name == element name
*    nsid == namespace ID of name
*    strval == const string value that will strduped first
* 
* RETURNS:
*   new string or NULL if malloc error
*********************************************************************/
val_value_t *
    xml_val_new_cstring (ncx_instance_t *instance,
                         const xmlChar *name,
                         xmlns_id_t     nsid,
                         const xmlChar *strval)
{
    xmlChar     *str;
    val_value_t *val;
    str = xml_strdup(instance, strval);
    if (!str) {
        return NULL;
    }

    val = new_string_attr(instance,  name, nsid, str );
    if ( !val ) {
        m__free(instance,  str );
        return NULL;
    }
    val->obj = ncx_get_gen_string(instance);
    return val;
}   /* xml_val_new_cstring */


/********************************************************************
* FUNCTION xml_val_new_flag
* 
*   Set up a new flag
*
* INPUTS:
*    name == element name
*    nsid == namespace ID of name
* 
* RETURNS:
*   new struct or NULL if malloc error
*********************************************************************/
val_value_t *
    xml_val_new_flag (ncx_instance_t *instance,
                      const xmlChar *name,
                      xmlns_id_t     nsid)
{
    val_value_t *val;

    val = val_new_value(instance);
    if (!val) {
        return NULL;
    }
    val->btyp = NCX_BT_EMPTY;
    val->v.boo = TRUE;
    val->typdef = typ_get_basetype_typdef(instance, NCX_BT_EMPTY);
    val->name = name;
    val->nsid = nsid;
    val->obj = ncx_get_gen_empty(instance);

    return val;

}   /* xml_val_new_flag */


/********************************************************************
* FUNCTION xml_val_new_boolean
* 
*   Set up a new boolean
*
* INPUTS:
*    name == element name
*    nsid == namespace ID of name
*    boo == boolean value to set
*
* RETURNS:
*   new struct or NULL if malloc error
*********************************************************************/
val_value_t *
    xml_val_new_boolean (ncx_instance_t *instance,
                         const xmlChar *name,
                         xmlns_id_t     nsid,
                         boolean boo)
{
    val_value_t *val;

    val = xml_val_new_flag(instance,  name, nsid );
    if (!val) {
        return NULL;
    }
    val->v.boo = boo;

    return val;

}   /* xml_val_new_boolean */


/********************************************************************
* FUNCTION xml_val_new_number
* 
*   Set up a new number
*
* INPUTS:
*    name == element name
*    nsid == namespace ID of name
*    num == number value to set
*    btyp == base type of 'num'
*
* RETURNS:
*   new struct or NULL if malloc error
*********************************************************************/
val_value_t *
    xml_val_new_number (ncx_instance_t *instance,
                        const xmlChar *name,
                        xmlns_id_t     nsid,
                        ncx_num_t *num,
                        ncx_btype_t btyp)
{
    val_value_t *val;
    status_t     res;

    val = val_new_value(instance);
    if (!val) {
        return NULL;
    }
    val->btyp = btyp;

    res = ncx_copy_num(instance, num, &val->v.num, btyp);
    if (res != NO_ERR) {
        val_free_value(instance, val);
        return NULL;
    }

    val->typdef = typ_get_basetype_typdef(instance, btyp);
    val->name = name;
    val->nsid = nsid;

    /* borrowing the generic string object
     * not sure it will really matter
     * since the val(nsid,name) is used instead
     */
    val->obj = ncx_get_gen_string(instance);
    return val;

}   /* xml_val_new_number */


/* END file xml_val.c */
