/*STANDARD_RIFT_IO_COPYRIGHT */
/*!
 * @file rwlog_common_macros.h
 * @brief RiftWare event log support macros
 */

#ifndef __RWLOG_COMMON_MACROS_H__
#define __RWLOG_COMMON_MACROS_H__

#include <sys/syscall.h>	/* SYS_gettid */

#include "uthash.h"
#include <string.h>
#include "rwlog_filters.h"

/*!
 * Search the shared memory L1 bloom filter for the value string and 
 * set filter_matched_ variable to 1 if bloom filter is set for this 
 * string. Used by Event Log macros to search first level attribute filters
 * for string type leafs in yang notification definition
 *
 * @param context - the log context
 * @param field_name - yang leaf name (not used for L1 now)
 * @param value - string argument
 * @param filter_matched - boolean flag
 * @param category - log event category 
 */ 
 
#define RWLOG_FILTER_STRING(context, field_name, value, filter_matched, category) \
do { \
  if (filter_matched) break; \
  RWLOG_FILTER_DEBUG_PRINT("Blooming::%s %s category: %d bitmap is %lx\n", \
                           field_name, value, category, \
                           context->category_filter[category].bitmap); \
  if (context->category_filter[category].bitmap == 0) { \
    break; \
  } \
  BLOOM_IS_SET(context->category_filter[category].bitmap,value,filter_matched); \
  RWLOG_FILTER_DEBUG_PRINT("MATCH::%s:%s Found %d\n", \
                           field_name, value,filter_matched); \
}while (0)


#define FIND_BLOOM_FILTER_UINT64(context, field_name, key, filter_matched, category); \
do {\
  if (filter_matched) break; \
  /*BLOOM_IS_SET_UINT64(mem_hdr->bitmap[category],&((uint64_t)(key)),filter_matched);*/ \
  BLOOM_IS_SET_UINT64(context->category_filter[0].bitmap,&((uint64_t)(key)),filter_matched); \
  RWLOG_FILTER_DEBUG_PRINT("Blooming::%llu bitmap is %x\n", \
                           (unsigned long long )key, \
                           context->category_filter[category].bitmap); \
  RWLOG_FILTER_DEBUG_PRINT("MATCH::%llu\n, Found: %d",  (unsigned long long ) key,filter_matched); \
} while (0)


/*!
 * Shared memory L1 bloom filter search for IMSI. Set filter_matched flag to 1 
 * if match is found. Used by RWLOG_EVENT_CALLID macro for session based logging
 *
 * @params context - event log context 
 * @params value_ - imsi value
 * @params filter_matched - flag
 * @params category - log event category
 */

#define RWLOG_FILTER_SESSION_PARAMS( context, value_, filter_matched, category) \
do { \
  if (filter_matched) break; \
  if(value_->has_imsi) { \
    RWLOG_FILTER_STRING(context, "sess_param:imsi", value_->imsi, filter_matched,RW_LOG_LOG_CATEGORY_ALL); \
  } \
  if(value_->has_ip_address) { \
    filter_matched = 1; \
  } \
} while(0)

/*!
 * Shared memory L1 bloom filter for call-identifie
 * used by session based logging macro RWLOG_EVENT_CALLID
 *
 * @param context - log event context
 * @param field_name - for future use
 * @param value - value of callid/groupid
 * @param filter_matched - flag 
 * @param category - log event category
 */

#define RWLOG_FILTER_CALLID( context, field_name, value, filter_matched, category) \
do { \
  if (filter_matched) break; \
  if (context->category_filter[0].bitmap == 0) { \
    break; \
  } \
  if (!value)  { \
    RWLOG_ERROR_PRINT("Blank Blank \n"); \
    break; \
  } \
  BLOOM_IS_SET_UINT64(context->category_filter[0].bitmap,&value->callid,filter_matched); \
  RWLOG_FILTER_DEBUG_PRINT("MATCH::%s:%lu Found %d\n", \
                             "callid", value->callid, filter_matched); \
  if (filter_matched) break; \
} while (0)

/*!
 * Shared memory L1 bloom filter for group identifier
 * used by session based logging macro RWLOG_EVENT_CALLID
 *
 * @param context - log event context
 * @param field_name - for future use
 * @param value - value of callid/groupid
 * @param filter_matched - flag 
 * @param category - log event category
 */
#define RWLOG_FILTER_GROUPID(context, field_name, value, filter_matched, category) \
do {\
  if (filter_matched) break; \
  if (context->category_filter[0].bitmap == 0) { \
    break; \
  } \
  if (!value)  { \
    RWLOG_ERROR_PRINT("Blank Blank \n"); \
    break; \
  } \
  BLOOM_IS_SET_UINT64(context->category_filter[0].bitmap,&value->groupid,filter_matched); \
  RWLOG_FILTER_DEBUG_PRINT("MATCH::%s:%lu Found %d\n", \
                             "groupcallid", value->groupid, filter_matched); \
  if (filter_matched) break; \
} while(0)

/*!
 * Will filter out event logs based on severity setting. L1 filter macro 
 * which does just a comparison of configured severity vs event notification
 * severity for the category and set filter_matched flag to 1 based on the result
 * @param context - log event context
 * @param cat - category
 * @param sev - severity
 * @param filter_matched - flag (will be set to 1 if filter matched)
 */

#define RWLOG_FILTER_SEVERITY(context, cat , sev,critical_info,filter_matched)	\
do {									\
  RWLOG_FILTER_DEBUG_PRINT("Severity fm:%d cat:%u  in-sev:%d\n",		\
                           filter_matched, cat, sev); \
  if (!filter_matched) { \
    if(critical_info || (((rwlog_severity_t)sev) <= context->category_filter[cat].severity)) {\
      RWLOG_FILTER_DEBUG_PRINT("Severity sev: %d critical-info %d MATCHED for log for app: %s\n",sev,critical_info,context->appname);\
      filter_matched = 1;						\
        break;								\
      }\
  }									\
} while (0)

/*!
 * filling common log event fields - invoked only if L1 filtering is passed. 
 * line number field is not filled in, line number is filled in another place
 *
 * @param _ctx - log context
 * @param template_params - common log event fields structure
 * @param category - log event cateogry
 * @result - filled template structure sans line number
 */ 

#define RWLOG_PROTO_FILL_COMMON_ATTR_EXCLUDING_FILE_LINENO(_ctx, template_params, category) \
    /* Much of this should be deferred to send time. */			\
    (template_params).version          = RWLOG_VER;				\
    /* Note that there will be seqno gaps due to L2 filter and/or	\
       discards of speculative buffered events. */			\
    (template_params).sequence = rwlog_ctx_tls.sequence++;		\
    (template_params).processid        = (_ctx)->pid;				\
    if ((template_params).has_sec == 0) { \
      struct timeval _tv;						\
      gettimeofday(&_tv, NULL);						\
      rwlog_ctx_tls.tv_cached.tv_sec = (template_params).sec = _tv.tv_sec;				\
      rwlog_ctx_tls.tv_cached.tv_usec = (template_params).usec = _tv.tv_usec;			        \
      (template_params).has_sec              = 1;					\
      (template_params).has_usec             = 1;					\
    }									\
    /* Oops, this next bit is fugly.  Normally avoids a syscall, but	\
       not a branch.  Linux TID is slightly obscure, but correlates	\
       better to observable "stuff" than does pthread_t.  We could use	\
       &rwlog_ctx_tls to eliminate the branch, but that doesn't much	\
       correlate to anything at all.  Doesn't matter too much, this is	\
       after L1 */							\
    (template_params).threadid         = ({ if (!rwlog_ctx_tls.tid) { rwlog_ctx_tls.tid = syscall(SYS_gettid); } rwlog_ctx_tls.tid; }); \
    (template_params).has_version          = 1;					\
    (template_params).has_sequence         = 1;					\
    (template_params).has_severity         = 1;					\
    (template_params).has_crit_info    = 1;					\
    (template_params).has_processid        = 1;					\
    (template_params).has_threadid         = 1;					\
    (template_params).has_appname          = 1;         \
    (template_params).has_hostname         = 1;         \
    strncpy((template_params).appname,(_ctx)->appname,63);					\
    (template_params).appname[63] = '\0'; \
    strncpy((template_params).hostname,(_ctx)->hostname, 63); \
    (template_params).hostname[63] = '\0'; 
    /* strlcpy not found */

/*!
 * fill common log event attributes which includes line number
 * @param _ctx - log context
 * @param proto - google protobuf of log event yang notif
 * @param category - log cateogry
 * @result - filled protobuf with common log event params
 */

#define RWLOG_PROTO_FILL_COMMON_ATTR(_ctx, common_params, category, line, file)		\
    common_params->has_linenumber       = 1;					\
    common_params->has_event_id       = 1;					\
    common_params->has_filename          = 1;         \
    common_params->linenumber       = line;				\
    int file_name_start = (strlen((char *)file) - 63); \
    if(file_name_start < 0) file_name_start = 0; \
    strncpy(common_params->filename, (char *)(file+ file_name_start),63); \
    common_params->filename[63] = '\0'; \
    { struct timeval _tv;						\
      gettimeofday(&_tv, NULL);						\
      rwlog_ctx_tls.tv_cached.tv_sec = common_params->sec = _tv.tv_sec;				\
      rwlog_ctx_tls.tv_cached.tv_usec = common_params->usec = _tv.tv_usec;			        \
      common_params->has_sec              = 1;					\
      common_params->has_usec             = 1;					\
    }									\

#define RWLOG_PROTO_FILL_STRING(proto, field, value) \
    proto.has_##field = 1; \
    strcpy(proto.field, value);

#define RWLOG_PROTO_FILL_INT32(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;

#define RWLOG_PROTO_FILL_INT64(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;

#define RWLOG_PROTO_FILL_UINT32(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;

#define RWLOG_PROTO_FILL_BOOL(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;

#define RWLOG_PROTO_FILL_DOUBLE(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;

#define RWLOG_PROTO_FILL_FLOAT(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;

#define RWLOG_PROTO_FILL_UINT64(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;

#define RWLOG_PROTO_FILL_CALLID_IN_TEMPLATE_PARAMS(common_params, field, value)  \
do { \
    if (value) {                                                           \
      common_params->has_##field = 1;                               \
      common_params->field.has_callid = 1;                          \
      common_params->field.has_groupcallid = 1;                     \
      common_params->field.callid = value->callid;                  \
      common_params->field.groupcallid = value->groupid;            \
      common_params->has_call_arrived_time = 1;                     \
      common_params->call_arrived_time = value->call_arrived_time;  \
    }\
}while(0)

#define RWLOG_PROTO_FILL_SESSION_PARAMS_IN_TEMPLATE_PARAMS(common_params, session_params_)  \
    common_params->session_params = session_params_;\

#define RWLOG_PROTO_ASSIGN_STRING(proto, field, value) \
    proto.field = (char *)value;

#define RWLOG_PROTO_FILL_BYTE(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;\

#define RWLOG_FILL_INT32(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;\

#define RWLOG_FILL_INT(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;\

#define RWLOG_FILL_UINT32(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;\

#define RWLOG_FILL_UINT64(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;\

#define RWLOG_FILL_INT64(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;\

#define RWLOG_PROTO_FILL_ENUM(proto, field, value) \
    proto.has_##field = 1; \
    proto.field = value;\

#define RWLOG_PROTO_FILL_BUF(proto, field, value , sz) \
    proto.has_##field = 1; \
    proto.field = value;\

#define RWLOG_PROTO_ASSIGN_BUF(proto, field, value , sz) \
    proto.n_##field = 1; \
    proto.field = value;

/*
 * Notes on Macro meta programming sections:
 *  - Beware the preprocessor!  Some things the preprocessor does are
 *    subtle.  Please read about macro replacement in the ISO C spec
 *    before contemplating changes to this code.  (C11: N1570, section
 *    6.10.3)
 *  - Almost everything in here is prefixed with RWLOG_impl, in order
 *    to ensure no name collisions.  With token pasting, that sometimes
 *    results in 2 or 3 or more RWLOG_impl's in a single identifier.
 *    DO NOT TRY TO REMOVE THOSE.  They are an important guarantee that
 *    no-one else's random short-word #define will wreck this code.
 *    Sorry for all the ugly, but at least it is hidden away in here.
 *  - This code makes extensive use of macro expansion, through
 *    multiple levels.  Strictly speaking, several of these macros
 *    could be collapsed into one.  However, it is better to err on the
 *    side of verbosity and explaination with code like this.  Try not
 *    to do too much at once.  Explain everything.
 */


/*
 * This macro token-pastes 2 tokens together with a separating
 * underscore, producing another token.  The result will typically
 * be long - sorry, that is the price you pay for macro
 * meta-programming.
 *
 * @param a - The first token.
 * @param b - The second token.
 * @return - Expands to a new token.
 */
#define RWLog_impl_paste2(a, b)  \
  a##_##b

/*
 * This macro token-pastes 3 tokens together with separating
 * underscores, producing another token.  The result will typically be
 * very long - sorry, that is the price you pay for macro
 * meta-programming.
 *
 * @param a - The first token.
 * @param b - The second token.
 * @param c - The third token.
 * @return - Expands to a new token.
 */
#define RWLog_impl_paste3(a, b, c)  \
  a##_##b##_##c


/*
 * This macro token-pastes 4 tokens together with separating
 * underscores, producing another token.  The result will typically be
 * obscenely long - sorry, that is the price you pay for macro
 * meta-programming.
 *
 * @param a - The first token.
 * @param b - The second token.
 * @param c - The third token.
 * @param d - The fourth token.
 * @return - Expands to a new token.
 */
#define RWLog_impl_paste4(a, b, c, d)  \
  a##_##b##_##c##_##d


/* Macro to get category from module name */
#define RWLog_impl_get_category(module_) \
        RWLog_impl_paste2(RW_LOG_LOG_CATEGORY, module_)

/* Macro to be used by application to support format specifiers for string 
  * Eg:
  * RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, RWLOG_ATTR_SPRINTF("Sprintf args %s,test32: %d, test64:%lu", user_string, test32,test64)); 
  */
#define RWLOG_ATTR_SPRINTF(fmt,...) \
  (snprintf(&msg_[0],sizeof(msg_),fmt,__VA_ARGS__),\
  msg_)

/**
 * macros used by appln for printing ip address in dotted form.
 */
#define RWLOG_INET_NTOA(addr_) inet_ntoa(addr_)
#define RWLOG_INET_NTOP(af, addr_) inet_ntoa(af, addr_, &msg_[0],sizeof(msg_))

/* 
 * Utility macro to remove extra paranthesis used while extracting data from generated meta data
 *
 * @param  - __VA_ARGS__ enclosed in paranthesis 
 * @return - expands into step 2 macro which results in __VA_ARGS__ with outer parenthisis removed
 *
 */ 
#define RWLog_impl_remove_paren(...) RWLog_impl_remove_paren_2 __VA_ARGS__

/*
 * This macro exists to see the parenthesis and and removes it
 */
#define RWLog_impl_remove_paren_2(...) __VA_ARGS__

/*
 * Extract the meta data information in 3 steps resulting in applying specific operation
 * (filter/assign) for all attribute, argument pairs. 
 * Step 1 - extract number of attributes from meta data and construct RWLog_impl_op_all_fields_n
 *          n equal to the number of attributes for this event.
 * Step 2 invoke the macro with attribute list and __VA__ARGS__ list to apply the operation
 * Step 3 - apply op to each attribute argument pair
 */

/*
 * First step in expanding the macro to apply operation to all __VA_ARGS__
 * Get the number of fields and construct the RWLog_impl_op_all_fields_n macro
 * @param ctx_            - Log context
 * @param path_           - Yang path
 * @param op_             - operation to apply - either filter or assign
 * @param xx_             - argument used by the op. For assign this is proto, 
                            for filter this is match flag
 * @param n_fields_       - number of fields/arguments for this log
 * @param field_name_list - list of field names separated by comas
 * @param __VA_ARGS__     - event log arguments supplied by application
 * @return                - expands into next 2 steps of macro which apply 
                            the op to all field,argument pairs
 *
 */ 
#define RWLog_impl_op_all_flds_event1(ctx_, path_, op_, xx_, n_fields_, field_name_list_, ...) \
    RWLog_impl_op_all_flds_n(RWLog_impl_paste2(RWLog_impl_op_all_flds, n_fields_), ctx_, path_, op_, xx_, \
                             RWLog_impl_remove_paren(field_name_list_), __VA_ARGS__)
                             
/*
 * Step 2 of applying the op to all field, argument pairs
 * @param macro_      - RWLog_impl_op_all_flds_n macro 
 * @param ctx_        - Log context
 * @param path_       - Yang path 
 * @param op_         - operation (filter/assign etc)
 * @param xx_         - argument used by op_ (proto, match flag etc)
 * @param __VA_ARGS__ - field-list and arg-list 
 * @return            - expands to macro which applies the op to all
 *                      arg, value pairs
 */ 
#define RWLog_impl_op_all_flds_n(macro_, ctx_, path_, op_, xx_, ...) \
do {\
    macro_(ctx_, path_, op_, xx_, __VA_ARGS__);\
}while(0);

/*
 * Step 3 for applying op to all attribute, argument pairs
 * A series of macros - which macro to invoke depends on number of arguments
 *
 */

/*
 * Step 3 macro Apply op_ to one field, value pair
 * @param ctx_      - Log context
 * @param path_     - Yang path
 * @param op_       - operation (filter/assign etc)
 * @param temp_params - Macro ignores the common params container 
 * @param anx_ and avx_ - field value pairs to apply op_
 * @return apply the operation to the field, value pair
 *
 */ 

#define RWLog_impl_op_all_flds_1(ctx_, path_, op_, xx_, temp_params_, an1_) 

#define RWLog_impl_op_all_flds_2(ctx_, path_, op_, xx_, temp_params_, an1_, av1_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_) 

/** Step 3 macro Apply op_ to 2 field, value pair **/
#define RWLog_impl_op_all_flds_3(ctx_, path_, op_, xx_, temp_params_, an1_, an2_, av1_, av2_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_) 

/** Step 3 macro Apply op_ to 3 field, value pair **/
#define RWLog_impl_op_all_flds_4(ctx_, path_, op_, xx_, temp_params_, an1_, an2_, an3_, av1_, av2_, av3_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_)

/** Step 3 macro Apply op_ to 4 field, value pair **/
#define RWLog_impl_op_all_flds_5(ctx_, path_, op_, proto_, temp_params_, an1_, an2_, an3_, an4_, av1_, av2_, av3_, av4_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, proto_, an1_, av1_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, proto_, an2_, av2_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, proto_, an3_, av3_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, proto_, an4_, av4_)

/** Step 3 macro Apply op_ to 5 field, value pair **/
#define RWLog_impl_op_all_flds_6(ctx_, path_, op_, xx_, temp_params_, an1_, an2_, an3_, an4_, an5_,\
  av1_, av2_, av3_, av4_, av5_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_)

/** Step 3 macro Apply op_ to 6 field, value pair **/
#define RWLog_impl_op_all_flds_7(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,\
  av1_,av2_,av3_,av4_,av5_,av6_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_)

/** Step 3 macro Apply op_ to 7 field, value pair **/
#define RWLog_impl_op_all_flds_8(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,\
  an7_,av1_,av2_,av3_,av4_,av5_,av6_,av7_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an7_, av7_)

/** Step 3 macro Apply op_ to 8 field, value pair **/
#define RWLog_impl_op_all_flds_9(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,\
  an7_,an8_,av1_,av2_,av3_,av4_,av5_,av6_,av7_,av8_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an7_, av7_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an8_, av8_)
 
/** Step 3 macro Apply op_ to 9 field, value pair **/
#define RWLog_impl_op_all_flds_10(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,\
  an7_,an8_,an9_,av1_,av2_,av3_,av4_,av5_,av6_,av7_,av8_,av9_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an7_, av7_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an8_, av8_) \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an9_, av9_)
 
/** Step 3 macro Apply op_ to 10 field, value pair **/
#define RWLog_impl_op_all_flds_11(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,\
  an7_,an8_,an9_,an10_,av1_,av2_,av3_,av4_,av5_,av6_,av7_,av8_,av9_,av10_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an7_, av7_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an8_, av8_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an9_, av9_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an10_, av10_) 
 
/** Step 3 macro Apply op_ to 11 field, value pair **/
#define RWLog_impl_op_all_flds_12(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,\
  an7_,an8_,an9_,an10_,an11_,av1_,av2_,av3_,av4_,av5_,av6_,av7_,av8_,av9_,av10_,av11_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an7_, av7_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an8_, av8_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an9_, av9_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an10_, av10_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an11_, av11_)
 
/** Step 3 macro Apply op_ to 12 field, value pair **/
#define RWLog_impl_op_all_flds_13(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,\
  an7_,an8_,an9_,an10_,an11_,an12_,av1_,av2_,av3_,av4_,av5_,av6_,av7_,av8_,av9_,av10_,av11_,av12_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an7_, av7_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an8_, av8_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an9_, av9_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an10_, av10_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an11_, av11_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an12_, av12_)
 
/** Step 3 macro Apply op_ to 13 field, value pair **/
#define RWLog_impl_op_all_flds_14(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,an7_,\
  an8_,an9_,an10_,an11_,an12_,an13_,av1_,av2_,av3_,av4_,av5_,av6_,av7_,av8_,av9_,av10_,av11_,av12_,av13_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an7_, av7_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an8_, av8_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an9_, av9_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an10_, av10_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an11_, av11_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an12_, av12_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an13_, av13_)
 
/** Step 3 macro Apply op_ to 14 field, value pair **/
#define RWLog_impl_op_all_flds_15(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,an7_,an8_,\
  an9_,an10_,an11_,an12_,an13_,an14_,av1_,av2_,av3_,av4_,av5_,av6_,av7_,av8_,av9_,av10_,av11_,av12_,av13_,av14_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an7_, av7_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an8_, av8_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an9_, av9_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an10_, av10_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an11_, av11_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an12_, av12_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an13_, av13_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an14_, av14_)
 
/** Step 3 macro Apply op_ to 15 field, value pair **/
#define RWLog_impl_op_all_flds_16(ctx_,path_,op_,xx_,temp_params_,an1_,an2_,an3_,an4_,an5_,an6_,an7_,an8_,an9_,an10_,\
  an11_,an12_,an13_,an14_,an15_,av1_,av2_,av3_,av4_,av5_,av6_,av7_,av8_,av9_,av10_,av11_,av12_,av13_,av14_,av15_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an1_, av1_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an2_, av2_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an3_, av3_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an4_, av4_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an5_, av5_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an6_, av6_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an7_, av7_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an8_, av8_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an9_, av9_)  \
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an10_, av10_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an11_, av11_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an12_, av12_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an13_, av13_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an14_, av14_)\
  RWLog_impl_attr_##op_##_event1(ctx_, path_, xx_, an15_, av15_)



/*
 * Top level macro to fill in all fields of proto struct with user supplied args 
 * This will eventually expand to the RWLog_impl_attr_assign_event1 macros which 
 * fill a specific field with a value.
 * @param ctx_        - Log context
 * @param path_       - Yang path
 * @param proto_      - proto-c struct which requires initialization
 * @param __VA_ARGS__ - field-name list and arg-list for the event
 * @param return      - expands to series of macros to fill in proto fields with arg values
 *
 */
#define RWLog_impl_assign_all_flds_event1(ctx_, path_, proto_, ...)  \
   RWLog_impl_op_all_flds_event1(ctx_, path_, assign, proto_, RWPB_i_message_mmd_pbc(path_, n_fields),\
                        RWPB_i_message_mmd_pbc(path_, l_field_name), __VA_ARGS__)

/*
 * Expands to right assign macro for a single field, value pair depending
 * on the type of field (string/bytes etc)
 * @param ctx_   - Log context
 * @param path_  - Yang path
 * @param proto_ - proto-c struct
 * @param fld_   - field name in protoc struct
 * @param value_ - proto_.fld_.value 
 * @result       - expands to assign macro for this field based on its type
 *
 */
#define RWLog_impl_attr_assign_event1(ctx_, path_, proto_, fld_, value_) \
     RWLog_impl_attr_assign_event2(ctx_, path_, proto_, \
                                   RWPB_i_message_fmd_pbc(path_, fld_, n_opt_req_rep),\
                                   RWPB_i_message_fmd_pbc(path_, fld_, n_type_kind), \
                                   RWPB_i_message_fmd_pbc(path_, fld_, n_flat_pointy), fld_, value_)

/*
 * Second step in expanding to field assign macro 
 * @param ctx_   - Log context
 * @param path_  - Yang path
 * @param proto_ - proto-c struct
 * @label_       - Yang label - optional/required etc   
 * @type         - Yang type - string/int etc
 * @style        - Yang style - inline/repeated etc
 * @param fld_   - field name in protoc struct
 * @param value_ - proto_.fld_.value 
 * @result       - expands to assign macro for this field based on its type
 *
 */
#define RWLog_impl_attr_assign_event2(ctx_, path_, proto_, label_, type_, style_, fld_, value_) \
    RWLog_impl_paste4(RWLog_impl_attr_assign, label_, type_, style_)(ctx_, proto_, fld_, value_)

/*
 * Top level macro to filter in all fields of proto struct with user supplied args 
 * This will eventually expand to the RWLog_impl_attr_filter_event1 macros which 
 * filter a specific field with a value.
 * @param ctx_        - Log context
 * @param path_       - Yang path
 * @param match_      - filter matched flag This will be set if filter is matched
 * @param __VA_ARGS__ - field-name list and arg-list for the event
 * @param return      - expands to series of macros to filter in proto fields with arg values
 *
 */
#define RWLog_impl_filter_all_flds_event1(ctx_, path_, match_, ...)  \
   RWLog_impl_op_all_flds_event1(ctx_, path_, filter, match_, RWPB_i_message_mmd_pbc(path_, n_fields),\
   RWPB_i_message_mmd_pbc(path_, l_field_name), __VA_ARGS__) 

/*
 * Expands to right filter macro for a single field, value pair depending
 * on the type of field (string/bytes etc)
 * @param ctx_   - Log context
 * @param path_  - Yang path
 * @param match_ - match flag
 * @param fld_   - field name in protoc struct
 * @param value_ - value to be matched
 * @result       - expands to fill macro for this field based on its type
 *
 */
#define RWLog_impl_attr_filter_event1(ctx_, path_, match_, fld_, value_) \
  if(match_) break;\
  RWLog_impl_attr_filter_event2(ctx_, path_, match_, \
                                   RWPB_i_message_fmd_pbc(path_, fld_, n_opt_req_rep), \
                                   RWPB_i_message_fmd_pbc(path_, fld_, n_type_kind), \
                                   RWPB_i_message_fmd_pbc(path_, fld_, n_flat_pointy), fld_, value_);\


/*
 * Second step in expanding to field filter macro 
 * @param ctx_   - Log context
 * @param path_  - Yang path
 * @param match_ - match flag
 * @label_       - Yang label - optional/required etc   
 * @type         - Yang type - string/int etc
 * @style        - Yang style - inline/repeated etc
 * @param fld_   - field name in protoc struct
 * @param value_ - value against filtering happens
 * @result       - expands to filter macro for this field based on its type
 *
 */
#define RWLog_impl_attr_filter_event2(ctx_, path_, match_, label_, type_, style_, fld_, value_) \
    RWLog_impl_paste4(RWLog_impl_attr_filter, label_, type_, style_)(ctx_, path_, match_, fld_, value_);

#define RWLog_impl_attr_assign_optional_string_pointy(ctx_, proto_, fld_, value_) \
do {\
   char msg_[1024]; \
   UNUSED(msg_[0]);\
   proto_.fld_ = (char *)(value_);\
}while(0);

#define RWLog_impl_attr_filter_optional_string_pointy(ctx_, path_, fltr_match_, fld_, value_) \
  RWLOG_FILTER_STRING(ctx_, #fld_, value_, fltr_match_, _category_);

#define RWLog_impl_attr_filter_optional_bytes_pointy(ctx_, path_, fltr_match_, fld_, value_) 
#define RWLog_impl_attr_filter_required_bytes_pointy(ctx_, path_, fltr_match_, fld_, value_) 
#define RWLog_impl_attr_filter_optional_bytes_flat(ctx_, path_, fltr_match_, fld_, value_) 
#define RWLog_impl_attr_filter_required_bytes_flat(ctx_, path_, fltr_match_, fld_, value_) 

#define RWLog_impl_attr_assign_required_string_pointy(ctx_, proto_, fld_, value_) \
   proto_.fld_ = (char *)(value_);

#define RWLog_impl_attr_filter_required_string_pointy(ctx_, path_, fltr_match_, fld_, value_) \
  RWLOG_FILTER_STRING(ctx_, #fld_, value_, fltr_match_, _category_); 

#define RWLog_impl_attr_assign_optional_string_flat(ctx_, proto_, fld_, value_) \
   proto_.has_##fld_ = 1; \
   strcpy(proto_.fld_, value_);

#define RWLog_impl_attr_filter_optional_string_flat(ctx_, path_, fltr_match_, fld_, value_) \
  RWLOG_FILTER_STRING(ctx_, #fld_, value_, fltr_match_, _category_);    

#define RWLog_impl_attr_assign_required_string_flat(ctx_, proto_, fld_, value_) \
   strcpy(proto_.fld_, value_);

#define RWLog_impl_attr_filter_required_string_flat(ctx_, path_, filt_match_, fld_, value_) \
  RWLOG_FILTER_STRING(ctx_, #fld_, value_, fltr_match_, _category_);    

#define RWLog_impl_attr_assign_optional_primitive_flat(ctx_, proto_, fld_, value_) \
   proto_.has_##fld_ = 1; \
   proto_.fld_ = value_;

#define RWLog_impl_attr_assign_required_primitive_flat(ctx_, proto_, fld_, value_) \
   proto_.fld_ = value_;

#define RWLog_impl_attr_filter_required_primitive_flat(ctx_, path_, flt_match_, fld_, value_) \
  RWLOG_FILTER_UINT64(ctx_, fld_, value_, flt_match_, _category_);

#define RWLog_impl_attr_filter_optional_primitive_flat(ctx_, path_, flt_match_, fld_, value_) \
  RWLOG_FILTER_UINT64(ctx_, fld_, value_, flt_match_, _category_);


#define RWLog_impl_attr_filter_optional_enum_flat(ctx_, path_, flt_match_, fld_, value_) \
  RWLOG_FILTER_UINT64(ctx_, fld_, value_, flt_match_, _category_);

#define RWLog_impl_attr_filter_required_enum_flat(ctx_, path_, flt_match_, fld_, value_) \
  RWLOG_FILTER_UINT64(ctx_, fld_, value_, flt_match_, _category_);

#define RWLog_impl_attr_assign_optional_enum_flat(ctx_, proto_, fld_, value_) \
   proto_.has_##fld_ = 1; \
   proto_.fld_ = value_;

#define RWLog_impl_attr_assign_required_enum_flat(ctx_, proto_, fld_, value_) \
   proto_.fld_ = value_;

#define RWLog_impl_attr_assign_optional_bytes_pointy(ctx_, proto_, fld_, value_) \
    proto_.has_##fld_ = 1; \
    proto_.fld_.data = value_.data; \
    proto_.fld_.len = value_.len; 

#define RWLog_impl_attr_assign_required_bytes_pointy(ctx_, proto_, fld_, value_) \
    proto_.fld_.data = value_.data; \
    proto_.fld_.len = value_.len; 

#define RWLog_impl_attr_assign_optional_bytes_flat(ctx_, proto_, fld_, value_) \
    proto_.has_##fld_ = 1; \
    memcpy(&proto_.fld_.data , &value_.data, value_.len); \
    proto_.fld_.len = value_.len; 

#define RWLog_impl_attr_assign_required_bytes_flat(ctx_, proto_, fld_, value_) \
    memcpy(&proto_.fld_.data , &value_.data, value_.len); \
    proto_.fld_.len = value_.len; 

#define RWLog_impl_attr_assign_optional_message_flat(ctx_, proto_, fld_, value_) \
   RWLog_impl_attr_assign_optional_##fld_##_flat(ctx_, proto_, value_)

#define RWLog_impl_attr_filter_optional_message_flat(ctx_, path_, filt_match_, fld_, value_) \
   RWLog_impl_attr_filter_optional_##fld_##_flat(ctx_, filt_match_, fld_, value_);

#define RWLog_impl_attr_assign_optional_message_pointy(ctx_, proto_, fld_, value_)

#define RWLog_impl_attr_assign_optional_pkt_info_flat(ctx_, proto_, value_) \
    proto_.has_pkt_info = 1; \
    proto_.pkt_info.has_packet_direction = 1; \
    proto_.pkt_info.has_packet_type = 1; \
    proto_.pkt_info.has_sport = 1; \
    proto_.pkt_info.has_dport = 1; \
    proto_.pkt_info.has_ip_header = 1; \
    proto_.pkt_info.has_fragment = 1; \
    proto_.pkt_info.has_packet_data = 1; \
    proto_.pkt_info.packet_direction = (RwLog__YangEnum__DirectionType__E)value_->packet_direction; \
    proto_.pkt_info.packet_type = (RwLog__YangEnum__ProtocolType__E)value_->packet_type; \
    proto_.pkt_info.sport = value_->sport; \
    proto_.pkt_info.dport = value_->dport; \
    proto_.pkt_info.ip_header = value_->ip_header; \
    proto_.pkt_info.fragment = value_->fragment; \
    proto_.pkt_info.packet_data.data = value_->packet_data.data; \
    proto_.pkt_info.packet_data.len = value_->packet_data.len; \
    packet_info_present = true; \


#define RWLog_impl_attr_filter_optional_pkt_info_flat(ctx_, filt_match_, fld_, value_)
#define RWLOG_FILTER_UINT32(c,f,n,m, category) do { } while (0);
#define RWLOG_FILTER_UINT64(c,f,n,m, category) do { } while (0);
#define RWLOG_FILTER_INT64(c,f,n,m, category) do { } while (0);
#define RWLOG_FILTER_FLOAT(c,f,n,m, category) do { } while (0);
#define RWLOG_FILTER_DOUBLE(c,f,n,m, category) do { } while (0);
#define RWLOG_FILTER_BOOL(c,f,n,m, category) do { } while (0);
#define RWLOG_FILTER_INT(a) do{RWLOG_FILTER_DEBUG_PRINT ("%d\n", a);}while (0);
#define RWLOG_FILTER_BYTE(context, field_name, value, filter_matched, category) do{}while (0);
#define RWLOG_FILTER_INT32(context, field_name, value, filter_matched, category)do{} while (0); 
#define RWLOG_FILTER_ENUM(context, field_name, value, filter_matched, category)do{} while (0); 

#endif
