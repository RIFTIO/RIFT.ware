/* STANDARD_RIFT_IO_COPYRIGHT */
/*!
 * @file rwdts_xpath.h
 * @brief XPath support for RW.DTS
 * @author Philip Joseph (philip.joseph@riftio.com)
 * @date 2015/09/04
 *
 * @note based on xpath implementation is yuma ncx library
 */

#ifndef __RWDTS_XPATH_H
#define __RWDTS_XPATH_H

// NCX library includes
#include <yuma/ncx/ncx.h>
#include <yuma/ncx/ncxmod.h>
#include <yuma/ncx/xpath.h>
#include <yuma/ncx/xpath1.h>

#include <rwdts.h>
#include <rwdts_int.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_keyspec.h>

__BEGIN_DECLS

typedef enum rwdts_xpath_action_e {
  RWDTS_XPATH_ACTION_NONE = 0,

  /* XPath 1.0 sec 2.2 AxisName */
  RWDTS_XPATH_ACTION_AXIS_ANCESTOR,
  RWDTS_XPATH_ACTION_AXIS_ANCESTOR_OR_SELF,
  RWDTS_XPATH_ACTION_AXIS_ATTRIBUTE,
  RWDTS_XPATH_ACTION_AXIS_CHILD,
  RWDTS_XPATH_ACTION_AXIS_DESCENDANT,
  RWDTS_XPATH_ACTION_AXIS_DESCENDANT_OR_SELF,
  RWDTS_XPATH_ACTION_AXIS_FOLLOWING,
  RWDTS_XPATH_ACTION_AXIS_FOLLOWING_SIBLING,
  RWDTS_XPATH_ACTION_AXIS_NAMESPACE,
  RWDTS_XPATH_ACTION_AXIS_PARENT,
  RWDTS_XPATH_ACTION_AXIS_PRECEDING,
  RWDTS_XPATH_ACTION_AXIS_PRECEDING_SIBLING,
  RWDTS_XPATH_ACTION_AXIS_SELF,

  /* XPath 1.0 Function library + current() from XPath 2.0 */
  RWDTS_XPATH_ACTION_FN_BEGIN,

  RWDTS_XPATH_ACTION_FN_BOOLEAN = RWDTS_XPATH_ACTION_FN_BEGIN,
  RWDTS_XPATH_ACTION_FN_CEILING,
  RWDTS_XPATH_ACTION_FN_CONCAT,
  RWDTS_XPATH_ACTION_FN_CONTAINS,
  RWDTS_XPATH_ACTION_FN_COUNT,
  RWDTS_XPATH_ACTION_FN_CURRENT,
  RWDTS_XPATH_ACTION_FN_FALSE,
  RWDTS_XPATH_ACTION_FN_FLOOR,
  RWDTS_XPATH_ACTION_FN_ID,
  RWDTS_XPATH_ACTION_FN_LANG,
  RWDTS_XPATH_ACTION_FN_LAST,
  RWDTS_XPATH_ACTION_FN_LOCAL_NAME,
  RWDTS_XPATH_ACTION_FN_NAME,
  RWDTS_XPATH_ACTION_FN_NAMESPACE_URI,
  RWDTS_XPATH_ACTION_FN_NORMALIZE_SPACE,
  RWDTS_XPATH_ACTION_FN_NOT,
  RWDTS_XPATH_ACTION_FN_NUMBER,
  RWDTS_XPATH_ACTION_FN_POSITION,
  RWDTS_XPATH_ACTION_FN_ROUND,
  RWDTS_XPATH_ACTION_FN_STARTS_WITH,
  RWDTS_XPATH_ACTION_FN_STRING,
  RWDTS_XPATH_ACTION_FN_STRING_LENGTH,
  RWDTS_XPATH_ACTION_FN_SUBSTRING,
  RWDTS_XPATH_ACTION_FN_SUBSTRING_AFTER,
  RWDTS_XPATH_ACTION_FN_SUBSTRING_BEFORE,
  RWDTS_XPATH_ACTION_FN_SUM,
  RWDTS_XPATH_ACTION_FN_TRANSLATE,
  RWDTS_XPATH_ACTION_FN_TRUE,

  /* yuma function extensions */
  RWDTS_XPATH_ACTION_FN_MODULE_LOADED,
  RWDTS_XPATH_ACTION_FN_FEATURE_ENABLED,

  /* Rift specific custom functions */
  RWDTS_XPATH_ACTION_FN_MIN,
  RWDTS_XPATH_ACTION_FN_MAX,
  RWDTS_XPATH_ACTION_FN_AVG,

  RWDTS_XPATH_ACTION_FN_END =  RWDTS_XPATH_ACTION_FN_AVG,

  /* XPath NodeType values */
  RWDTS_XPATH_ACTION_NT_COMMENT,
  RWDTS_XPATH_ACTION_NT_TEXT,
  RWDTS_XPATH_ACTION_NT_PROCESSING_INSTRUCTION,
  RWDTS_XPATH_ACTION_NT_NODE,

  RWDTS_XPATH_ACTION_EXOP_BEGIN,
  RWDTS_XPATH_ACTION_EXOP_AND = RWDTS_XPATH_ACTION_EXOP_BEGIN,         /* keyword 'and' */
  RWDTS_XPATH_ACTION_EXOP_OR,          /* keyword 'or' */
  RWDTS_XPATH_ACTION_EXOP_EQUAL,       /* equals '=' */
  RWDTS_XPATH_ACTION_EXOP_NOTEQUAL,    /* bang equals '!=' */
  RWDTS_XPATH_ACTION_EXOP_LT,          /* left angle bracket '<' */
  RWDTS_XPATH_ACTION_EXOP_GT,          /* right angle bracket '>' */
  RWDTS_XPATH_ACTION_EXOP_LEQUAL,      /* l. angle-equals '<= */
  RWDTS_XPATH_ACTION_EXOP_GEQUAL,      /* r. angle-equals '>=' */
  RWDTS_XPATH_ACTION_EXOP_ADD,         /* plus sign '+' */
  RWDTS_XPATH_ACTION_EXOP_SUBTRACT,    /* minus '-' */
  RWDTS_XPATH_ACTION_EXOP_MULTIPLY,    /* asterisk '*' */
  RWDTS_XPATH_ACTION_EXOP_DIV,         /* keyword 'div' */
  RWDTS_XPATH_ACTION_EXOP_MOD,         /* keyword 'mod' */
  RWDTS_XPATH_ACTION_EXOP_NEGATE,      /* unary '-' */
  RWDTS_XPATH_ACTION_EXOP_UNION,       /* vert. bar '|' */
  RWDTS_XPATH_ACTION_EXOP_FILTER1,     /* fwd slash '/' */
  RWDTS_XPATH_ACTION_EXOP_FILTER2,      /* double fwd slash (C++ comment) */

  RWDTS_XPATH_ACTION_EXOP_END = RWDTS_XPATH_ACTION_EXOP_FILTER2,

  /* Custom action to note a predicate */
  RWDTS_XPATH_ACTION_PREDICATE,

  /* Add above this */
  RWDTS_XPATH_ACTION_MAX_VAL

}rwdts_xpath_action_t;

#define RWDTS_XPATH_PATH_MAX_SIZE 1023

#define RWDTS_XPATH_EXPR_ALLOC_CHUNK_SIZE 5

#define RWDTS_XPATH_FN_MAX_PARAMS 3

#define RWDTS_XPATH_RESP_ALLOC_CHUNK_SIZE 3

// Multiple flags can be set, so make sure they are
// in different bit positions within uint32
// Used in the RWDtsReduction structure
typedef enum rwdts_xpath_flags_e {
  RWDTS_XPATH_FLAG_NONE = 0,
  RWDTS_XPATH_FLAG_PARSED = 1,
  RWDTS_XPATH_FLAG_QUERY = 2, // Action is in progress
  RWDTS_XPATH_FLAG_DONE = 4, // This action is completed

  RWDTS_XPATH_FLAG_ERROR = 64,  // Error in processing the action
} rwdts_xpath_flags_t;

// Flags in the resp to specify the type
typedef enum rwdts_xpath_resp_flags_e {
  RWDTS_XPATH_RESP_NONE = 0,
  RWDTS_XPATH_RESP_DONE = 1, // Flag to specify if the result is complete
  RWDTS_XPATH_RESP_ERR  = 2, // Specify if there are any errors in resp
  RWDTS_XPATH_RESP_PATH = 4, // If path
  RWDTS_XPATH_RESP_PATH_PART = 8, // If partial path
  RWDTS_XPATH_RESP_OVERFLOW = 16,
  RWDTS_XPATH_RESP_RESULT = 32, // Used for function results
} rwdts_xpath_resp_flags_t;

// Protobuf type in response
typedef enum rwdts_xpath_pbcm_type_e {
  RWDTS_XPATH_PBCM_NONE = 0,
  RWDTS_XPATH_PBCM_NC,
  RWDTS_XPATH_PBCM_LEAF,
  RWDTS_XPATH_PBCM_MSG,
} rwdts_xpath_pbcm_type_t;

#define RWDTS_XPATH_MAX_DEPTH 16

#define RWDTS_XPATH_ERR_MSG_MAX_SIZE 1023
/*
 * XPath parser control block to keep track of
 * partial results, further processing, etc.
 */
typedef struct rwdts_xpath_pcb_s {
  // Copy of the xpath query
  char *query;

  // Pointer to library instance
  void *lib_inst;
  bool local_inst; // locally created

  // Pointer to apih
  rwdts_api_t *apih;

  // Ypbc schema
  const rw_yang_pb_schema_t* schema;

  // xpath pcb in use
  void *pcb;

  // XPath expression
  RWDtsRednExpression *expr;
  // Partial results
  RWDtsRednResp *results;
  // Keep track of the iterations
  uint32_t pass;
  // Count of the number of queries generated
  uint32_t qcount;
  // track the next id to use
  int32_t next_id;
  // Keep track of the last location path
  int32_t loc_path_id;
  // Keep track if we are processing predicate
  int32_t predicate;
  // Keep track of the part depth
  int32_t depth;
  // Keep track of the chain
  int32_t ids[RWDTS_XPATH_MAX_DEPTH];

  // Store keyspec instance
  rw_keyspec_instance_t* ksi;

  // Category of key, DATA or CONFIG
  // For now only DATA
  RwSchemaCategory cat;

  // Store the client callbacks
  rwdts_event_cb_t xact_cb;
  rwdts_event_cb_t blk_cb;

  // Pointer to xact to be used for client callbacks
  rwdts_xact_t *xact;
  // Other query params passed
  RWDtsQueryAction action;
  uint32_t flags;
  const ProtobufCMessage* msg;

  // Error meessage from parser
  char errmsg[RWDTS_XPATH_ERR_MSG_MAX_SIZE+1];

  // Parse complete
  bool parsed;

  // Print debug info
  bool debug;

} rwdts_xpath_pcb_t;

// Create xpath library instance
void*
rwdts_xpath_lib_inst_new(const rw_yang_pb_schema_t* schema);

// Load additional schema
rw_status_t
rwdts_xpath_load_schema(void* instance, const rw_yang_pb_schema_t* schema);

// Free the library instance
rw_status_t
rwdts_xpath_lib_inst_free(void *inst);

// Free the rwdts_xpath_pcb_t instance
rw_status_t
rwdts_xpath_pcb_free(rwdts_xpath_pcb_t *rwpcb);

// Tokenize, validate and generate the parsed XPath
// epxression with a new parse control block
rw_status_t
rwdts_xpath_parse(const char* xpath,
                  rw_xpath_type_t xpath_type,
                  rw_keyspec_instance_t* ks,
                  rwdts_api_t *apih,
                  rwdts_xpath_pcb_t **rwpcb);

// Initiate the queries based on the parsed expression
rw_status_t
rwdts_xpath_eval(rwdts_xpath_pcb_t *rwpcb);

// Execute the queries
rw_status_t
rwdts_xpath_query_int(rwdts_xpath_pcb_t*       rwpcb,
                      RWDtsQueryAction         action,
                      uint32_t                 flags,
                      rwdts_event_cb_t*        cb,
                      const ProtobufCMessage*  msg);

// Check if we have a single rooted path for expression
// eg: /entry1/entry2[pred1=const1]
// returns keyspec if successful
rw_keyspec_path_t*
rwdts_xpath_get_single_query_ks(rwdts_xpath_pcb_t *rwpcb);

// Add a numeric constant to the expression
rw_status_t
rwdts_xpath_build_expr_float(rwdts_xpath_pcb_t *rwpcb,
                             double val);

// Add a string constant to the expression
rw_status_t
rwdts_xpath_build_expr_str(rwdts_xpath_pcb_t *rwpcb,
                           const char* val);

// Add a xpath function to the expression
rw_status_t
rwdts_xpath_build_expr_fn(rwdts_xpath_pcb_t *rwpcb,
                          const char* name,
                          int32_t ids[],
                          int32_t count);

// Add a operator like equal, add, etc to the expression
// with multiple dependent parts to the expression
rw_status_t
rwdts_xpath_build_oper_ids(rwdts_xpath_pcb_t *rwpcb,
                           uint32_t action,
                           int32_t ids[],
                           int32_t count);

// Add an operator with 1 dependent part to the expression
rw_status_t
rwdts_xpath_build_oper_1_id (rwdts_xpath_pcb_t *rwpcb,
                             uint32_t action,
                             int32_t id1);

// Add an operator with 2 dependent parts to the expression
rw_status_t
rwdts_xpath_build_oper_2_ids (rwdts_xpath_pcb_t *rwpcb,
                              uint32_t action,
                              int32_t id1,
                              int32_t id2);

// Add a location path part to the expression
rw_status_t
rwdts_xpath_build_path(rwdts_xpath_pcb_t *rwpcb,
                       char* path,
                       int32_t prev_id);

// Add a predicate to the expression
rw_status_t
rwdts_xpath_build_predicate(rwdts_xpath_pcb_t *rwpcb,
                            int32_t id);

// Find the action enum for a name
rwdts_xpath_action_t
rwdts_xpath_map_fn(const char* fname);

#define RWDTS_XPATH_TK_DUMP_BUF_LEN 8095

// Utility function to give as string the
// generated expression with the tokens
char*
rwdts_xpath_dump_tokens(rwdts_xpath_pcb_t *rwpcb);

// Utility to dump the results
// as a string
char*
rwdts_xpath_dump_results(rwdts_xpath_pcb_t *rwpcb);

// Print the pcb details for debugging to stderr
void
rwdts_xpath_print_pcb(rwdts_xpath_pcb_t *rwpcb, const char* func);

__END_DECLS

#endif /* __RWDTS_XPATH_H*/
