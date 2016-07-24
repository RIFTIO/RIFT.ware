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
/*  FILE: ncx_feature.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
17feb10      abb      begun; split out from ncx.c

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
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <xmlstring.h>
#include <xmlreader.h>

#include "procdefs.h"
#include "dlq.h"
#include "log.h"
#include "ncx.h"
#include "ncx_appinfo.h"
#include "ncx_feature.h"
#include "ncxconst.h"
#include "status.h"
#include "typ.h"
#include "val.h"
#include "xml_util.h"
#include "yangconst.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                            T Y P E S                              *
*                                                                   *
*********************************************************************/

/* feature code entry */
typedef struct feature_entry_t_ {
    dlq_hdr_t            qhdr;
    xmlChar             *modname;
    xmlChar             *feature;
    ncx_feature_code_t   code;       // deprecated; not used
    boolean              code_set;   // deprecated; not used
    boolean              enable;
    boolean              enable_set;
} feature_entry_t;


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
* FUNCTION split_feature_string
* 
* Split a feature string into its 2 parts
*   modname:feature
*
* INPUTS:
*   featstr == feature string parm to split
*   modnamelen == address of return module name length
*
* OUTPUTS:
*   *modnamelen == module name length
*  
* RETURNS:
*   status
*********************************************************************/
static status_t
    split_feature_string (const xmlChar *featstr,
                          uint32 *modnamelen)
{
    const xmlChar *str, *found;

    found = NULL;
    str = featstr;
    *modnamelen = 0;

    while (*str) {
        if (*str == ':') {
            if (found != NULL) {
                return ERR_NCX_INVALID_VALUE;
            } else {
                found = str;
            }
        }
        str++;
    }

    if (found) {
        *modnamelen = (uint32)(found - featstr);
        return NO_ERR;
    } else {
        return ERR_NCX_INVALID_VALUE;
    }

}  /* split_feature_string */


/********************************************************************
* FUNCTION free_feature_entry
* 
* Free a feature_entry_t
*
* INPUTS:
*   feature_entry == feature entry struct to free
*********************************************************************/
static void
    free_feature_entry (ncx_instance_t *instance, feature_entry_t *feature_entry)
{

    if (feature_entry->modname) {
        m__free(instance, feature_entry->modname);
    }
    if (feature_entry->feature) {
        m__free(instance, feature_entry->feature);
    }
    m__free(instance, feature_entry);

}  /* free_feature_entry */


/********************************************************************
* FUNCTION new_feature_entry
* 
* Create a feature_entry_t
*
* INPUTS:
*   featstr == feature string parm to use
*********************************************************************/
static feature_entry_t *
    new_feature_entry (ncx_instance_t *instance, const xmlChar *featstr)
{
    uint32 len = 0;
    boolean splitdone = FALSE;
    status_t res = split_feature_string(featstr, &len);
    if (res == NO_ERR) {
        splitdone = TRUE;
    }

    feature_entry_t *feature_entry = m__getObj(instance, feature_entry_t);
    if (feature_entry == NULL) {
        return NULL;
    }
    memset(feature_entry, 0x0, sizeof(feature_entry_t));

    if (splitdone) {
        feature_entry->modname = xml_strndup(instance, featstr, len);
        if (feature_entry->modname == NULL) {
            free_feature_entry(instance, feature_entry);
            return NULL;
        }

        feature_entry->feature = xml_strdup(instance, &featstr[len+1]);
        if (feature_entry->feature == NULL) {
            free_feature_entry(instance, feature_entry);
            return NULL;
        }
    } else {
        feature_entry->feature = xml_strdup(instance, featstr);
        if (feature_entry->feature == NULL) {
            free_feature_entry(instance, feature_entry);
            return NULL;
        }
    }

    return feature_entry;

}  /* new_feature_entry */


/********************************************************************
* FUNCTION new_feature_entry2
* 
* Create a feature_entry_t
*
* INPUTS:
*   modname == module name to use
*   name == feature name to use
*********************************************************************/
static feature_entry_t *
    new_feature_entry2 (ncx_instance_t *instance,
                        const xmlChar *modname,
                        const xmlChar *name)
{
    feature_entry_t *feature_entry = m__getObj(instance, feature_entry_t);
    if (feature_entry == NULL) {
        return NULL;
    }
    memset(feature_entry, 0x0, sizeof(feature_entry_t));

    feature_entry->modname = xml_strdup(instance, modname);
    if (feature_entry->modname == NULL) {
        free_feature_entry(instance, feature_entry);
        return NULL;
    }

    feature_entry->feature = xml_strdup(instance, name);
    if (feature_entry->feature == NULL) {
        free_feature_entry(instance, feature_entry);
        return NULL;
    }
    return feature_entry;

}  /* new_feature_entry2 */


/********************************************************************
* FUNCTION find_feature_entry
* 
* Find a feature_entry_t
*
* INPUTS:
*   featstr == feature string parm to use
*   featQ == Q of feature_entry_t to use
*
* RETURNS:
*   pointer to found entry or NULL if not found
*********************************************************************/
static feature_entry_t *
    find_feature_entry (ncx_instance_t *instance,
                        const xmlChar *featstr,
                        dlq_hdr_t  *featQ)
{
    uint32 len = 0;
    boolean splitdone = FALSE;
    status_t res = split_feature_string(featstr, &len);
    if (res == NO_ERR) {
        splitdone = TRUE;
    }

    feature_entry_t  *feature_entry = (feature_entry_t *)dlq_firstEntry(instance, featQ);
    for (; feature_entry != NULL;
         feature_entry = (feature_entry_t *)dlq_nextEntry(instance, feature_entry)) {

        if (splitdone && feature_entry->modname) {
            /* match the module name */
            uint32 len2 = xml_strlen(instance, feature_entry->modname);
            if (len != len2) {
                continue;
            }
            if (xml_strncmp(instance, feature_entry->modname, featstr, len)) {
                continue;
            }
        }

        /* match the feature name */
        if (splitdone) {
            if (xml_strcmp(instance, feature_entry->feature, &featstr[len+1])) {
                continue;
            }
        } else {
            if (xml_strcmp(instance, feature_entry->feature, featstr)) {
                continue;
            }
        }

        return feature_entry;
    }

    return NULL;

}  /* find_feature_entry */


/********************************************************************
* FUNCTION find_feature_entry2
* 
* Find a feature_entry_t 
*
* INPUTS:
*   modname == module name
*   feature == feature name
*   featQ == Q of feature_entry_t to use
*
* RETURNS:
*   pointer to found and removed entry or NULL if not found
*********************************************************************/
static feature_entry_t *
    find_feature_entry2 (ncx_instance_t *instance,
                         const xmlChar *modname,
                         const xmlChar *feature,
                         dlq_hdr_t  *featQ)
{
    feature_entry_t  *feature_entry;

    for (feature_entry = (feature_entry_t *)dlq_firstEntry(instance, featQ);
         feature_entry != NULL;
         feature_entry = (feature_entry_t *)dlq_nextEntry(instance, feature_entry)) {

        /* match the module name */
        if (feature_entry->modname && modname &&
            xml_strcmp(instance, feature_entry->modname, modname)) {
            continue;
        }

        /* match the feature name */
        if (xml_strcmp(instance, feature_entry->feature, feature)) {
            continue;
        }

        return feature_entry;
    }

    return NULL;

}  /* find_feature_entry2 */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION ncx_feature_init
* 
* Init the ncx_feature module
*
* INPUTS:
*    none
* RETURNS:
*    none
*********************************************************************/
void
    ncx_feature_init (ncx_instance_t *instance)
{
    if (instance->feature_init_done) {
        SET_ERROR(instance, ERR_INTERNAL_INIT_SEQ);
        return;
    }

    instance->feature_code_default = NCX_FEATURE_CODE_DYNAMIC;
    instance->feature_enable_default = TRUE;
    dlq_createSQue(instance, &instance->feature_entryQ);
    instance->feature_init_done = TRUE;

}  /* ncx_feature_init */


/********************************************************************
* FUNCTION ncx_feature_cleanup
* 
* Cleanup the ncx_feature module
*
* INPUTS:
*    none
* RETURNS:
*    none
*********************************************************************/
void
    ncx_feature_cleanup (ncx_instance_t *instance)
{
    feature_entry_t  *feature_entry;

    if (!instance->feature_init_done) {
        return;
    }

    while (!dlq_empty(instance, &instance->feature_entryQ)) {
        feature_entry = (feature_entry_t *)dlq_deque(instance, &instance->feature_entryQ);
        free_feature_entry(instance, feature_entry);
    }

    instance->feature_init_done = FALSE;

}  /* ncx_feature_cleanup */


/********************************************************************
* FUNCTION ncx_new_iffeature
* 
* Get a new ncx_iffeature_t struct
*
* INPUTS:
*    none
* RETURNS:
*    pointer to a malloced ncx_iffeature_t struct,
*    or NULL if malloc error
*********************************************************************/
ncx_iffeature_t *
    ncx_new_iffeature (ncx_instance_t *instance)
{
    ncx_iffeature_t *iff;

    iff = m__getObj(instance, ncx_iffeature_t);
    if (!iff) {
        return NULL;
    }
    memset(iff, 0x0, sizeof(ncx_iffeature_t));

    return iff;

} /* ncx_new_iffeature */


/********************************************************************
* FUNCTION ncx_free_iffeature
* 
* Free a malloced ncx_iffeature_t struct
*
* INPUTS:
*    iff == struct to free
*
*********************************************************************/
void 
    ncx_free_iffeature (ncx_instance_t *instance, ncx_iffeature_t *iff)
{

#ifdef DEBUG
    if (!iff) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    if (iff->prefix) {
        m__free(instance, iff->prefix);
    }
    if (iff->name) {
        m__free(instance, iff->name);
    }

    m__free(instance, iff);
    
} /* ncx_free_iffeature */


/********************************************************************
* FUNCTION ncx_clone_iffeature
* 
* Clone a new ncx_iffeature_t struct
*
* INPUTS:
*    srciff == ifffeature struct to clone
* RETURNS:
*    pointer to a malloced ncx_iffeature_t struct,
*    or NULL if malloc error
*********************************************************************/
ncx_iffeature_t *
    ncx_clone_iffeature (ncx_instance_t *instance, ncx_iffeature_t *srciff)
{
    ncx_iffeature_t *iff;

    iff = m__getObj(instance, ncx_iffeature_t);
    if (!iff) {
        return NULL;
    }
    memset(iff, 0x0, sizeof(ncx_iffeature_t));

    if (srciff->prefix) {
        iff->prefix = xml_strdup(instance, srciff->prefix);
        if (iff->prefix == NULL) {
            ncx_free_iffeature(instance, iff);
            return NULL;
        }
    }

    if (srciff->name) {
        iff->name = xml_strdup(instance, srciff->name);
        if (iff->name == NULL) {
            ncx_free_iffeature(instance, iff);
            return NULL;
        }
    }

    iff->feature = srciff->feature;

    ncx_set_error(&iff->tkerr,
                  srciff->tkerr.mod,
                  srciff->tkerr.linenum,
                  srciff->tkerr.linepos);

    //iff->seen not set

    return iff;

} /* ncx_clone_iffeature */


/********************************************************************
* FUNCTION ncx_clean_iffeatureQ
* 
* Clean a Q of malloced ncx_iffeature_t struct
*
* INPUTS:
*    iffeatureQ == address of Q to clean
*
*********************************************************************/
void 
    ncx_clean_iffeatureQ (ncx_instance_t *instance, dlq_hdr_t *iffeatureQ)
{

    ncx_iffeature_t  *iff;

#ifdef DEBUG
    if (!iffeatureQ) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    while (!dlq_empty(instance, iffeatureQ)) {
        iff = (ncx_iffeature_t *)dlq_deque(instance, iffeatureQ);
        ncx_free_iffeature(instance, iff);
    }
    
} /* ncx_clean_iffeatureQ */


/********************************************************************
* FUNCTION ncx_find_iffeature
* 
* Search a Q of ncx_iffeature_t structs for a match
*
* INPUTS:
*    iffeatureQ == address of Q to search
*    prefix == prefix to check for
*              a NULL value indicates the current module
*    name == feature name string to find
*********************************************************************/
ncx_iffeature_t *
    ncx_find_iffeature (ncx_instance_t *instance,
                        dlq_hdr_t *iffeatureQ,
                        const xmlChar *prefix,
                        const xmlChar *name,
                        const xmlChar *modprefix)
{
    ncx_iffeature_t  *iff;

#ifdef DEBUG
    if (!iffeatureQ || !name) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    for (iff = (ncx_iffeature_t *)
             dlq_firstEntry(instance, iffeatureQ);
         iff != NULL;
         iff = (ncx_iffeature_t *)dlq_nextEntry(instance, iff)) {

        /* check if name fields the same */
        if (iff->name && !xml_strcmp(instance, iff->name, name)) {

            /* check if prefix fields reference
             * different modules, if set or implied
             */
            if (!ncx_prefix_different(instance,
                                      prefix,
                                      iff->prefix,
                                      modprefix)) {
                return iff;
            }
        }
    }
    return NULL;
    
} /* ncx_find_iffeature */


/********************************************************************
* FUNCTION ncx_new_feature
* 
* Get a new ncx_feature_t struct
*
* INPUTS:
*    none
* RETURNS:
*    pointer to a malloced ncx_feature_t struct,
*    or NULL if malloc error
*********************************************************************/
ncx_feature_t *
    ncx_new_feature (ncx_instance_t *instance)
{
    ncx_feature_t     *feature;

    feature = m__getObj(instance, ncx_feature_t);
    if (!feature) {
        return NULL;
    }
    memset(feature, 0x0, sizeof(ncx_feature_t));

    dlq_createSQue(instance, &feature->iffeatureQ);
    dlq_createSQue(instance, &feature->appinfoQ);

    /*** setting feature enabled as the default
     *** the agent code needs to adjust this
     *** with agt_enable_feature or 
     *** agt_disable_feature() if needed
     ***/
    feature->enabled = instance->feature_enable_default;
    feature->code = instance->feature_code_default;

    return feature;

} /* ncx_new_feature */


/********************************************************************
* FUNCTION ncx_free_feature
* 
* Free a malloced ncx_feature_t struct
*
* INPUTS:
*    feature == struct to free
*
*********************************************************************/
void 
    ncx_free_feature (ncx_instance_t *instance, ncx_feature_t *feature)
{

#ifdef DEBUG
    if (!feature) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    if (feature->name) {
        m__free(instance, feature->name);
    }

    if (feature->descr) {
        m__free(instance, feature->descr);
    }

    if (feature->ref) {
        m__free(instance, feature->ref);
    }

    ncx_clean_iffeatureQ(instance, &feature->iffeatureQ);

    ncx_clean_appinfoQ(instance, &feature->appinfoQ);

    m__free(instance, feature);
    
} /* ncx_free_feature */


/********************************************************************
* FUNCTION ncx_find_feature
* 
* Find a ncx_feature_t struct in the module and perhaps
* any of its visible submodules
*
* INPUTS:
*    mod == module to search
*    name == feature name to find
*
* RETURNS:
*    pointer to found feature or NULL if not found
*********************************************************************/
ncx_feature_t *
    ncx_find_feature (ncx_instance_t *instance,
                      ncx_module_t *mod,
                      const xmlChar *name)
{
    ncx_feature_t  *feature;
    dlq_hdr_t      *que;
    yang_node_t    *node;
    ncx_include_t  *inc;

#ifdef DEBUG
    if (!mod || !name) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    feature = ncx_find_feature_que(instance, &mod->featureQ, name);
    if (feature) {
        return feature;
    }

    que = ncx_get_allincQ(mod);

    /* check all the submodules, but only the ones visible
     * to this module or submodule
     */
    for (inc = (ncx_include_t *)dlq_firstEntry(instance, &mod->includeQ);
         inc != NULL;
         inc = (ncx_include_t *)dlq_nextEntry(instance, inc)) {

        /* get the real submodule struct */
        if (!inc->submod) {
            node = yang_find_node(instance, 
                                  que, 
                                  inc->submodule,
                                  inc->revision);
            if (node) {
                inc->submod = node->submod;
            }
            if (!inc->submod) {
                /* include not found or errors in it */
                continue;
            }
        }

        /* check the feature Q in this submodule */
        feature = ncx_find_feature_que(instance, &inc->submod->featureQ, name);
        if (feature) {
            return feature;
        }
    }

    return NULL;

} /* ncx_find_feature */


/********************************************************************
* FUNCTION ncx_find_feature_que
* 
* Find a ncx_feature_t struct in the specified Q
*
* INPUTS:
*    featureQ == Q of ncx_feature_t to search
*    name == feature name to find
*
* RETURNS:
*    pointer to found feature or NULL if not found
*********************************************************************/
ncx_feature_t *
    ncx_find_feature_que (ncx_instance_t *instance,
                          dlq_hdr_t *featureQ,
                          const xmlChar *name)
{
    ncx_feature_t *feature;

#ifdef DEBUG
    if (!featureQ || !name) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    for (feature = (ncx_feature_t *)dlq_firstEntry(instance, featureQ);
         feature != NULL;
         feature = (ncx_feature_t *)dlq_nextEntry(instance, feature)) {

        if (!xml_strcmp(instance, feature->name, name)) {
            return feature;
        }
    }
    return NULL;
         
} /* ncx_find_feature_que */


/********************************************************************
* FUNCTION ncx_find_feature_all
* 
* Find a ncx_feature_t struct in the module and perhaps
* any of its submodules
*
* INPUTS:
*    mod == module to search
*    name == feature name to find
*
* RETURNS:
*    pointer to found feature or NULL if not found
*********************************************************************/
ncx_feature_t *
    ncx_find_feature_all (ncx_instance_t *instance,
                          ncx_module_t *mod,
                          const xmlChar *name)
{
    ncx_feature_t  *feature;
    dlq_hdr_t      *que;
    yang_node_t    *node;

#ifdef DEBUG
    if (!mod || !name) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    feature = ncx_find_feature_que(instance, &mod->featureQ, name);
    if (feature) {
        return feature;
    }

    que = ncx_get_allincQ(mod);

    /* check all the submodules */
    for (node = (yang_node_t *)dlq_firstEntry(instance, que);
         node != NULL;
         node = (yang_node_t *)dlq_nextEntry(instance, node)) {

        if (node->submod) {
            /* check the feature Q in this submodule */
            feature = ncx_find_feature_que(instance, &node->submod->featureQ, name);
            if (feature) {
                return feature;
            }
        }
    }

    return NULL;

} /* ncx_find_feature_all */


/********************************************************************
* FUNCTION ncx_for_all_features
* 
* Execute a callback function for all features in this module
* and any submodules
*
* INPUTS:
*    mod == module to search for features
*    cbfn == feature callback function
*    cookie == cookie value to pass to each iteration of the callback
*    enabledonly == TRUE if only callbacks for enabled features
*                   FALSE if all features should invoke callbacks
*********************************************************************/
void
    ncx_for_all_features (ncx_instance_t *instance,
                          const ncx_module_t *mod,
                          ncx_feature_cbfn_t  cbfn,
                          void *cookie,
                          boolean enabledonly)
{
    ncx_feature_t        *feature;
    const dlq_hdr_t      *que;
    yang_node_t          *node;
    ncx_include_t        *inc;
    boolean               keepgoing;

#ifdef DEBUG
    if (!mod || !cbfn) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    keepgoing = TRUE;

    for (feature = (ncx_feature_t *)dlq_firstEntry(instance, &mod->featureQ);
         feature != NULL && keepgoing;
         feature = (ncx_feature_t *)dlq_nextEntry(instance, feature)) {

        if (enabledonly && !ncx_feature_enabled(instance, feature)) {
            continue;
        }

        keepgoing = (*cbfn)(instance, mod, feature, cookie);
    }   
        
    que = ncx_get_const_allincQ(mod);

    /* check all the submodules, but only the ones visible
     * to this module or submodule
     */
    for (inc = (ncx_include_t *)dlq_firstEntry(instance, &mod->includeQ);
         inc != NULL && keepgoing;
         inc = (ncx_include_t *)dlq_nextEntry(instance, inc)) {

        /* get the real submodule struct */
        if (!inc->submod) {
            node = yang_find_node(instance, 
                                  que, 
                                  inc->submodule,
                                  inc->revision);
            if (node) {
                inc->submod = node->submod;
            }
            if (!inc->submod) {
                /* include not found or errors in it */
                continue;
            }
        }

        for (feature = (ncx_feature_t *)
                 dlq_firstEntry(instance, &inc->submod->featureQ);
             feature != NULL && keepgoing;
             feature = (ncx_feature_t *)dlq_nextEntry(instance, feature)) {

            if (enabledonly && !ncx_feature_enabled(instance, feature)) {
                continue;
            }

            keepgoing = (*cbfn)(instance, mod, feature, cookie);
        }
    }

} /* ncx_for_all_features */


/********************************************************************
* FUNCTION ncx_feature_count
* 
* Get the total feature count for this module
* and any submodules
*
* INPUTS:
*    mod == module to search for features
*    enabledonly == TRUE to only count enabled features
*                   FALSE to count all features
*
* RETURNS:
*   total number of features
*********************************************************************/
uint32
    ncx_feature_count (ncx_instance_t *instance,
                       const ncx_module_t *mod,
                       boolean enabledonly)
{
    const ncx_feature_t  *feature;
    const yang_node_t    *node;
    const dlq_hdr_t      *que;
    ncx_include_t        *inc;
    uint32                count;

#ifdef DEBUG
    if (!mod) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return 0;
    }
#endif

    count = 0;

    for (feature = (const ncx_feature_t *)dlq_firstEntry(instance, &mod->featureQ);
         feature != NULL;
         feature = (const ncx_feature_t *)dlq_nextEntry(instance, feature)) {

        if (enabledonly && !ncx_feature_enabled(instance, feature)) {
            continue;
        }

        count++;
    }   
        
    que = ncx_get_const_allincQ(mod);

    /* check all the submodules, but only the ones visible
     * to this module or submodule
     */
    for (inc = (ncx_include_t *)dlq_firstEntry(instance, &mod->includeQ);
         inc != NULL;
         inc = (ncx_include_t *)dlq_nextEntry(instance, inc)) {

        /* get the real submodule struct */
        if (!inc->submod) {
            node = yang_find_node(instance, 
                                  que, 
                                  inc->submodule,
                                  inc->revision);
            if (node) {
                inc->submod = node->submod;
            }
            if (!inc->submod) {
                /* include not found or errors in it */
                continue;
            }
        }

        for (feature = (const ncx_feature_t *)
                 dlq_firstEntry(instance, &inc->submod->featureQ);
             feature != NULL;
             feature = (const ncx_feature_t *)dlq_nextEntry(instance, feature)) {

            if (enabledonly && !ncx_feature_enabled(instance, feature)) {
                continue;
            }
            count++;
        }
    }
    return count;

} /* ncx_feature_count */


/********************************************************************
* FUNCTION ncx_feature_enabled
* 
* Check if the specified feature and any referenced
* if-features are enabled
*
* INPUTS:
*    feature == feature to check
*
* RETURNS:
*   TRUE if feature is completely enabled
*   FALSE if feature is not enabled, or partially enabled
*********************************************************************/
boolean
    ncx_feature_enabled (ncx_instance_t *instance, const ncx_feature_t *feature)
{
    const ncx_iffeature_t  *iffeature;

#ifdef DEBUG
    if (!feature) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return FALSE;
    }
#endif

    if (!feature->enabled) {
        return FALSE;
    }

    /* make sure all nested if-features are also enabled */
    for (iffeature = (const ncx_iffeature_t *)
             dlq_firstEntry(instance, &feature->iffeatureQ);
         iffeature != NULL;
         iffeature = (const ncx_iffeature_t *)
             dlq_nextEntry(instance, iffeature)) {

        if (!iffeature->feature) {
            /* feature was not found, so call it disabled */
            return FALSE;
        }

        if (!ncx_feature_enabled(instance, iffeature->feature)) {
            return FALSE;
        }
    }

    return TRUE;

} /* ncx_feature_enabled */


/********************************************************************
* FUNCTION ncx_feature_enabled_str
* 
* Check if the specified feature and any referenced
* if-features are enabled
*
* INPUTS:
*    modname == name of module to search
*    revision == module revision string (may be NULL)
*    name == feature name to find
* RETURNS:
*   TRUE if feature is completely enabled
*   FALSE if feature is not enabled, or partially enabled
*********************************************************************/
boolean
    ncx_feature_enabled_str (ncx_instance_t *instance,
                             const xmlChar *modname,
                             const xmlChar *revision,
                             const xmlChar *name)
{
#ifdef DEBUG
    if (!modname || !name) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return FALSE;
    }
#endif
    ncx_module_t *mod = ncx_find_module(instance, modname, revision);
    if (mod == NULL) {
        return FALSE;
    }

    const ncx_feature_t *feature = ncx_find_feature(instance, mod, name);
    if (feature == NULL) {
        return FALSE;
    }
    return ncx_feature_enabled(instance, feature);

} /* ncx_feature_enabled_str */


/********************************************************************
* FUNCTION ncx_set_feature_enable_default
* 
* Set the feature_enable_default flag
*
* INPUTS:
*   flag == feature enabled flag value
*********************************************************************/
void
    ncx_set_feature_enable_default (ncx_instance_t *instance, boolean flag)
{
    instance->feature_enable_default = flag;
}  /* ncx_set_feature_enabled_default */


/********************************************************************
* FUNCTION ncx_set_feature_code_default
* 
* Set the feature_code_default enumeration
*
* !!! THIS FUNCTION IS DEPRECATED!!!
* !!! The --feature-code and --feature-code-default parameters are ignored
* !!! Feature code generation is not controlled by this parameter 
*
* INPUTS:
*   code == feature code value
*********************************************************************/
void
    ncx_set_feature_code_default (ncx_instance_t *instance, ncx_feature_code_t code)
{
    instance->feature_code_default = code;
}  /* ncx_set_feature_code_default */


/********************************************************************
* FUNCTION ncx_set_feature_code_entry
* 
* Create or set a feature_entry struct for the specified 
* feature code parameter
*
* !!! THIS FUNCTION IS DEPRECATED!!!
* !!! The --feature-code and --feature-code-default parameters are ignored
* !!! Feature code generation is not controlled by this parameter 
*
* INPUTS:
*   featstr == feature parameter string
*   featcode == ncx_feature_code_t enumeration to set
*
* RETURNS:
*   status
*********************************************************************/
status_t
    ncx_set_feature_code_entry (ncx_instance_t *instance,
                                const xmlChar *featstr,
                                ncx_feature_code_t featcode)
{
    feature_entry_t  *fentry;
    status_t          res;
    uint32            cnt;

#ifdef DEBUG
    if (featstr == NULL) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    res = NO_ERR;
    fentry = find_feature_entry(instance, featstr, &instance->feature_entryQ);
    if (fentry != NULL) {
        if (fentry->code_set) {
            if (fentry->code != featcode) {
                log_error(instance,
                          "\nError: feature '%s' already set with "
                          "conflicting value",
                          featstr);
                res = ERR_NCX_INVALID_VALUE;
            } else {
                log_info(instance,
                          "\nFeature '%s' already set with "
                          "same value",
                          featstr);
            }
        } else {
            fentry->code_set = TRUE;
            fentry->code = featcode;
        }
    } else {
        cnt = 0;
        res = split_feature_string(featstr, &cnt);
        if (res == NO_ERR) {
            fentry = new_feature_entry(instance, featstr);
            if (fentry == NULL) {
                res = ERR_INTERNAL_MEM;
            } else {
                fentry->code_set = TRUE;
                fentry->code = featcode;
                dlq_enque(instance, fentry, &instance->feature_entryQ);
            }
        }
    }
    return res;

}  /* ncx_set_feature_code_entry */


/********************************************************************
* FUNCTION ncx_set_feature_enable_entry
* 
* Create or set a feature_entry struct for the specified 
* feature enabled parameter
*
* Called from CLI/conf handler code
*
* INPUTS:
*   featstr == feature parameter string
*   flag == enabled flag
*
* RETURNS:
*   status
*********************************************************************/
status_t
    ncx_set_feature_enable_entry (ncx_instance_t *instance,
                                  const xmlChar *featstr,
                                  boolean flag)
{
#ifdef DEBUG
    if (featstr == NULL) {
        return SET_ERROR(instance, ERR_INTERNAL_PTR);
    }
#endif

    status_t res = NO_ERR;
    feature_entry_t *fentry = find_feature_entry(instance, featstr, &instance->feature_entryQ);
    if (fentry != NULL) {
        if (fentry->enable_set) {
            if (fentry->enable != flag) {
                log_info(instance,
                         "\nFeature '%s' already %s so ignoring new value",
                         (flag) ? "disabled" : "enabled", featstr);
                res = ERR_NCX_INVALID_VALUE;
            }
        } else {
            fentry->enable_set = TRUE;
            fentry->enable = flag;
        }
    } else {
        fentry = new_feature_entry(instance, featstr);
        if (fentry == NULL) {
            res = ERR_INTERNAL_MEM;
        } else {
            fentry->enable_set = TRUE;
            fentry->enable = flag;
            dlq_enque(instance, fentry, &instance->feature_entryQ);
        }
    }
    return res;

}  /* ncx_set_feature_enable_entry */


/********************************************************************
* FUNCTION ncx_set_feature_enable
* 
* Create or set a feature_entry struct for the specified 
* feature enabled parameter
*
* Called from SIL init code
*
* INPUTS:
*   modname == name of module defining the feature
*   name == feature name
*   flag == feature enabled flag
*
* RETURNS:
*   status
*********************************************************************/
status_t
    ncx_set_feature_enable (ncx_instance_t *instance,
                            const xmlChar *modname,
                            const xmlChar *name,
                            boolean flag)
{
    assert( modname && "modname is NULL!" );
    assert( name && "modname is NULL!" );

    status_t res = NO_ERR;
    feature_entry_t *fentry = 
        find_feature_entry2(instance, modname, name, &instance->feature_entryQ);
    if (fentry != NULL) {
        if (fentry->enable_set) {
            if (fentry->enable != flag) {
                if (flag) {
                    /* SIL enabled, so previous CLI disable is allowed */
                    log_debug(instance, "\nFeature '%s' already disabled from CLI, "
                             "ignoring SIL disable", name);
                } else {
                    /* SIL disabled so override CLI enable */
                    log_info(instance, "\nFeature '%s' disabled in SIL, "
                             "overriding CLI enable", name);
                    fentry->enable = FALSE;
                }
            } /* else same value so ignore */
        } else {
            fentry->enable_set = TRUE;
            fentry->enable = flag;
        }
    } else {
        fentry = new_feature_entry2(instance, modname, name);
        if (fentry == NULL) {
            res = ERR_INTERNAL_MEM;
        } else {
            fentry->enable_set = TRUE;
            fentry->enable = flag;
            dlq_enque(instance, fentry, &instance->feature_entryQ);
        }
    }
    return res;

}  /* ncx_set_feature_enable */


/********************************************************************
* FUNCTION ncx_set_feature_parms
* 
* Check if any feature parameters were set for the specified 
* feature struct
*
* INPUTS:
*   feature == feature struct to check
*
* OUTPUTS:
*   feature->code and/or feature->enabled may be set
*********************************************************************/
void
    ncx_set_feature_parms (ncx_instance_t *instance, ncx_feature_t *feature)
{

    feature_entry_t  *fentry;

#ifdef DEBUG
    if (feature == NULL) {
        SET_ERROR(instance, ERR_INTERNAL_PTR);
        return;
    }
#endif

    if (feature->name == NULL ||
        feature->tkerr.mod == NULL ||
        ncx_get_modname(feature->tkerr.mod) == NULL) {
        /* feature not filled in properly */
        return;
    }

    fentry = find_feature_entry2(instance,
                                 ncx_get_modname(feature->tkerr.mod),
                                 feature->name,
                                 &instance->feature_entryQ);
    if (fentry != NULL) {
        if (fentry->code_set) {
            feature->code = fentry->code;
        }
        if (fentry->enable_set) {
            feature->enabled = fentry->enable;
        }
    }

}  /* ncx_set_feature_parms */


/* END file ncx_feature.c */

