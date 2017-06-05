/* STANDARD_RIFT_IO_COPYRIGHT */
/*!
 * @file rwdts_api.h
 * @brief Core API for RW.DTS
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 */

#ifndef __RWDTS_MACROS_H__
#define __RWDTS_MACROS_H__

#define RW_KEYSPEC_PATH_FREE(keyspec,instance,ud) \
  do \
  { \
    if ((instance == NULL) && (ud == NULL)) \
    { \
      rw_keyspec_path_free(keyspec, instance);  \
    }  \
  } while (0)

#define RW_KEYSPEC_PATH_GET_PRINT_BUFFER(k,i,s,b,bl,ud) \
  do \
  { \
    if ((i == NULL) && (ud == NULL)) \
    { \
      rw_keyspec_path_get_print_buffer(k,i,s,b,bl); \
    }  \
  } while (0)
#define RW_KEYSPEC_PATH_CREATE_DUP(in,instance,out,ud) rw_keyspec_path_create_dup(in,instance,out)
#define RW_KEYSPEC_PATH_CREATE_DUP_OF_TYPE(in,instance,out,ud) rw_keyspec_path_create_dup_of_type(in,instance,out)
#define RW_KEYSPEC_PATH_IS_MATCH_DETAIL(a,b,c,d,e,ud) rw_keyspec_path_is_match_detail(a, b, c, d, e)
#define RW_KEYSPEC_PATH_CREATE_WITH_DOMPATH_BINARY_DATA(a,b,ud) rw_keyspec_path_create_with_dompath_binary_data(a,b)
#define RW_KEYSPEC_PATH_SET_CATEGORY(a,b,c,ud) rw_keyspec_path_set_category(a, b, c)
#define RW_KEYSPEC_PATH_GET_BINPATH(a,b,c,d,ud) rw_keyspec_path_get_binpath(a,b,c,d)
#define RW_KEYSPEC_PATH_SERIALIZE_DOMPATH(a,b,c,ud) rw_keyspec_path_serialize_dompath(a,b,c)
#define RW_KEYSPEC_PATH_TRUNC_SUFFIX_N(a,b,c,ud) rw_keyspec_path_trunc_suffix_n(a,b,c)
#define RW_KEYSPEC_PATH_REROOT_ITER_INIT(a,b,c,d,e,ud)  rw_keyspec_path_reroot_iter_init(a,b,c,d,e)
#define RW_KEYSPEC_PATH_FROM_XPATH(a,b,c,d,ud) rw_keyspec_path_from_xpath(a,b,c,d)
#define RW_KEYSPEC_PATH_REROOT_AND_MERGE_OPAQUE(a,b,c,d,e,f,g,ud) rw_keyspec_path_reroot_and_merge_opaque(a,b,c,d ,e,f,g);
#define RW_KEYSPEC_PATH_REROOT_AND_MERGE(a,b,c,d,e,f,ud) rw_keyspec_path_reroot_and_merge(a,b,c,d ,e,f);
#define RW_KEYSPEC_PATH_REROOT(a,b,c,d,ud) rw_keyspec_path_reroot(a,b,c,d)
#define RW_KEYSPEC_ENTRY_CREATE_FROM_PROTO(a,b,ud) rw_keyspec_entry_create_from_proto(a,b)
#define RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(a,b,c,ud) rw_keyspec_path_is_a_sub_keyspec(a,b,c)
#define RW_KEYSPEC_PATH_SCHEMA_REROOT(a,b,c,d,ud) rw_keyspec_path_schema_reroot(a,b,c,d)
#define RW_KEYSPEC_PATH_IS_ENTRY_AT_TIP(a,b,c,d,e,ud) rw_keyspec_path_is_entry_at_tip(a,b,c,d,e)
#define RW_KEYSPEC_PATH_DELETE_PROTO_FIELD(a,b,c,d,ud) rw_keyspec_path_delete_proto_field(a,b,c,d)
#define RW_KEYSPEC_PATH_IS_EQUAL(a,b,c,ud) rw_keyspec_path_is_equal(a,b,c)
#define RW_KEYSPEC_PATH_MATCHES_MESSAGE(a,b,c,ud) rw_keyspec_path_matches_message(a,b,c)
#define RW_KEYSPEC_PATH_APPEND_ENTRY(a,b,c,ud) rw_keyspec_path_append_entry(a,b,c)
#define RW_KEYSPEC_PATH_ADD_KEYS_TO_ENTRY(a,b,c,d,ud) rw_keyspec_path_add_keys_to_entry(a,b,c,d)
#define RW_KEYSPEC_PATH_CREATE_WITH_BINPATH_BUF(a,b,c,ud) rw_keyspec_path_create_with_binpath_buf(a,b,c)


#ifdef ENABLE_VAR_LOG_MESSAGES
#define DTS_PRINT(_fmt, _args...) g_log(0, G_LOG_LEVEL_MESSAGE , _fmt, ##_args)
#define ENABLE_VAR_LOG_MESSAGES
#define RWDTS_ASSERT_MESSAGE(_expr, _apih, _keyspec, _xact, _fmt, _args...)    \
{                                                                           \
  if (!(_expr)) {                                                           \
    uint32_t _suppressed;                                                   \
    _RW_ASSERT_SUPPRESSED_(_expr, _suppressed);                             \
    if(!_suppressed)                                                        \
    {                                                                       \
      gint64 fd = -1;                                                       \
      fd = open("/var/log/messages", O_APPEND|O_WRONLY);                    \
      if (fd < 0)                                                           \
      {                                                                     \
        fd = 2;                                                             \
      }                                                                     \
      g_log_set_handler(0,G_LOG_LEVEL_MESSAGE, rwdts_log_handler, (gpointer)fd);    \
      DTS_PRINT("Function %s\n", __func__);                                 \
      if (_apih) rwdts_api_print(_apih);                                    \
      if (_regh) rwdts_reg_print(_regh);                                    \
      if (_xact) rwdts_xact_print(_xact);                                   \
      g_log(0,  G_LOG_LEVEL_MESSAGE, _fmt, ##_args);                        \
      g_error(_fmt, ##_args);                                               \
    }                                                                       \
  }                                                                         \
}
#else
#define DTS_PRINT(_fmt, _args...) fprintf(stderr, _fmt, ##_args)
#define RWDTS_ASSERT_MESSAGE(_expr, _apih, _ks, _xact, _fmt, _args...)    \
{                                                                           \
  if (!(_expr)) {                                                           \
    uint32_t _suppressed;                                                   \
    _RW_ASSERT_SUPPRESSED_(_expr, _suppressed);                             \
    if(!_suppressed)                                                        \
    {                                                                       \
      if (_ks)                                                              \
      {                                                                     \
        char *tmp_str = NULL;                                               \
        rw_keyspec_path_get_new_print_buffer(_ks,                           \
                                           NULL,                            \
                                           NULL,                            \
                                           &tmp_str);                       \
        DTS_PRINT("KeySpec %s\n", tmp_str);                                 \
        free(tmp_str);                                                      \
      }                                                                     \
      DTS_PRINT("Function %s\n", __func__);                                 \
      if (_apih) rwdts_api_print(_apih);                                    \
      if (_xact) rwdts_xact_print(_xact);                                   \
      g_error(_fmt, ##_args);                                               \
    }                                                                       \
  }                                                                         \
}

#define RWDTS_ROUTER__LOG_PRINT_TEMPL(t_logdump, args...) \
{ \
  char *fill_char = NULL; \
  int r = asprintf(&fill_char, args); \
  RW_ASSERT((r > 0) && fill_char); \
  if ((t_logdump)) { \
    char *tmp = (t_logdump); \
    (t_logdump) = NULL; \
    r = asprintf (&(t_logdump), "%s %s", tmp, fill_char); \
    RW_ASSERT((r > 0) && (t_logdump)); \
    RW_FREE(tmp); \
  } \
  else { \
    r = asprintf (&(t_logdump), "%s", fill_char); \
    RW_ASSERT((r > 0) && (t_logdump)); \
  } \
  RW_FREE (fill_char); \
  fill_char = NULL; \
}

#define PRINT_XACT_INFO_ELEM(x, logdump) {\
  RWDTS_PRINTF(" %30s = %10d\n", #x, xact->x);\
  if (logdump) { \
    char *tmp = logdump; \
    logdump = NULL; \
    int r = asprintf (&logdump, "%s %30s = %10d", tmp, #x, xact->x);\
    RW_ASSERT((r > 0) && (logdump));                                \
    RW_FREE (tmp);\
  }\
}

#define RWDTS_SIZE_TO_BUCKET(len, which_bucket) \
   which_bucket = (32 - __builtin_clz(len|0x20) - 6);\
   if (which_bucket >14) \
      which_bucket = 9; \
   else if (which_bucket >10) \
      which_bucket = 8; \
   else if (which_bucket>6)\
      which_bucket = 7;
#define RWDTS_LOG_CODEPOINT(xact, _val) \
    xact->evt[xact->n_evt%RWDTS_CP_MAX]=RWDTS_MEMBER_##_val; xact->n_evt++;

#endif /*ENABLE_VAR_LOG_MESSAGES*/

#define RWDTS_MEMBER_SEND_ERROR(xact, keyspec, query, apih, resp, status, _fmt, _args...) \
  do { \
    char _err_msg[512];                                                 \
    snprintf(_err_msg, 512, "%s[%d]:" _fmt, __FUNCTION__, __LINE__, ##_args); \
    if (xact || query) { \
      rwdts_member_send_error(xact, keyspec, query, apih, resp,           \
                              status, _err_msg);                          \
    } \
  } while(0)

#define RWDTS_XACT_ABORT(xact, status, _fmt, _args...) \
  do { \
     char _err_msg[512]; \
     snprintf(_err_msg, 512, "%s[%d]:" _fmt, __FUNCTION__, __LINE__, ##_args); \
     rwdts_xact_abort(xact, status, _err_msg); \
  } while (0)

#endif /*__RWDTS_MACROS_H__*/
