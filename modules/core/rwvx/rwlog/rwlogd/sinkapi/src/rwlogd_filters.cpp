/* Standard Rift.IO copyright */

#include "rwlogd_filters.hpp"
#include <time.h>
#include <dlfcn.h>


rwlogd_filter::filter_stats rwlogd_filter::stats;
rwlogd_filter:: rwlogd_filter():num_filters(0)
{
  RWLOG_FILTER_DEBUG_PRINT("sink : Filter Created******\n");
  memset(&filter_hdr_,0,sizeof(filter_hdr_));
  filter_hdr_.magic=RWLOG_FILTER_MAGIC;
  filter_hdr_.rwlogd_pid=getpid();
  filter_info=NULL;
  deny_filter_info=NULL;
  filter_hdr_.next_call_filter = FALSE;
  filter_hdr_.failed_call_filter = FALSE;
  filter_hdr_.rwlogd_ticks = 0;
  filter_hdr_.rwlogd_flow = FALSE;
  next_failed_call_filter = FALSE;
  filter_hdr_.num_categories = 0;
  category_filter_ = NULL;
  protocol_filter_set_ = FALSE;
  num_filters = 0;
}

rwlogd_filter::~rwlogd_filter()
{
  RWLOG_FILTER_DEBUG_PRINT("Filter: Destructor Called \n");
  filter_hdr_.num_categories = 0;

  //Loop thru UT Hash and delete all names
  filter_attribute *current_filter=NULL, *tmp = NULL;
  HASH_ITER(hh, filter_info, current_filter, tmp) {
    HASH_DEL(filter_info,current_filter);  /* delete; filter_info advances to next */
    free(current_filter);           
  }

  HASH_ITER(hh, deny_filter_info, current_filter, tmp) {
    HASH_DEL(deny_filter_info,current_filter);  /* delete; deny_filter_info advances to next */
    free(current_filter);           
  }

  if(category_filter_) {
    RW_FREE(category_filter_);
    category_filter_ = NULL;
  }
  //if num_filters is zero
  //Call merge Filter with DEL flag
  //Call merge severity with DEL flag
}

void rwlogd_filter::rwlog_dump_info(int verbosity)
{
  printf ("\tFilter Info \n");
  printf ("\t-------------\n");

  for(uint32_t i =0 ; i<filter_hdr_.num_categories; i++)
  {
    printf("Index: %-4d, Severity: %-10d Bitmap: %-32lx Critical-Info: %-2x\n",i, category_filter_[i].severity,category_filter_[i].bitmap,category_filter_[i].critical_info);
  }

  filter_attribute *filter = filter_info;
  int i = 0;
  for(; filter != NULL;
      filter=(filter_attribute*)(filter->hh.next))
  {
    i++;
    printf("\t%-4d %s\n",i,filter->attribute_value_string);
  }
  stats.rwlog_dump_stats(1);
}
rwlog_severity_t rwlogd_filter:: get_severity(uint32_t category)
{
  RW_ASSERT(category < filter_hdr_.num_categories);
  if (category >= filter_hdr_.num_categories) {
    return RW_LOG_LOG_SEVERITY_DEBUG;
  }
  return category_filter_[category].severity;
}

void  rwlogd_filter:: set_severity(uint32_t category,
                                   rwlog_severity_t sev)
{
  RW_ASSERT(category < filter_hdr_.num_categories);
  if (category >= filter_hdr_.num_categories) {
    return;
  }
  category_filter_[category].severity = sev;
}

void  rwlogd_filter:: set_critical_info_filter(uint32_t category,RwLog__YangEnum__OnOffType__E critical_info_filter)
{
  RW_ASSERT(category < filter_hdr_.num_categories);
  if (category >= filter_hdr_.num_categories) {
    return;
  }
  category_filter_[category].critical_info = critical_info_filter;
}

void  rwlogd_filter::set_protocol_filter(uint32_t protocol,
                                   bool enable)
{
  if(protocol< RW_LOG_PROTOCOL_TYPE_MAX_VALUE) {
    RWLOG_FILTER_DEBUG_PRINT("Value before setting is %x-%x-%x-%x\n",
           filter_hdr_.protocol_filter[0], filter_hdr_.protocol_filter[1], filter_hdr_.protocol_filter[2], filter_hdr_.protocol_filter[3]);
    if(enable) {
      RWLOG_FILTER_SET_PROTOCOL(&filter_hdr_,protocol);
    }
    else {
      RWLOG_FILTER_RESET_PROTOCOL(&filter_hdr_,protocol);
    }
    RWLOG_FILTER_DEBUG_PRINT("Value after setting for %d protocol: %u is %x-%x-%x-%x index: %lu bit: %lu\n",enable,protocol,
           filter_hdr_.protocol_filter[0], filter_hdr_.protocol_filter[1], filter_hdr_.protocol_filter[2], filter_hdr_.protocol_filter[3],
           (protocol/sizeof(filter_hdr_.protocol_filter[0])), (protocol%sizeof(filter_hdr_.protocol_filter[0])));

    RWLOG_UPDATE_PROTOCOL_FILTER_FLAG(&filter_hdr_,protocol_filter_set_);
  }
}

bool rwlogd_filter::is_packet_direction_match(uint32_t direction)
{
  bool enable = RWLOG_FILTER_IS_PACKET_DIRECTION_SET(&filter_hdr_,direction);
  return enable;
}


bool rwlogd_filter::is_protocol_filter_set(uint32_t protocol)
{
  bool enable = FALSE;
  if(protocol< RW_LOG_PROTOCOL_TYPE_MAX_VALUE) {
    enable = RWLOG_FILTER_IS_PROTOCOL_SET(&filter_hdr_,protocol);
  }
  return enable;
}

bool rwlogd_filter::is_attribute_filter_set(uint32_t cat)
{
  return !!(category_filter_[cat].bitmap);
}
bool rwlogd_filter::check_string_filter(char *cat_str,
                                        uint32_t cat,
                                         char *name, char *value)
{
  RWLOG_FILTER_DEBUG_PRINT("sink : check_string_filter %s %s\n", name,value);
  if (!category_filter_[cat].bitmap)
  {
    stats.str_filter_no_bitmap++;
    return RWLOG_CHECK_MORE_FILTERS;
  }

  if(find_exact_match_filter_string(&filter_info,cat_str,name,value)!=NULL)
  {
    stats.str_filter_matched++;
    RWLOG_FILTER_DEBUG_PRINT("sink : check_string_filter matched\n");
    return RWLOG_FILTER_MATCHED;
  }
  stats.str_filter_nomatch++;
  return RWLOG_CHECK_MORE_FILTERS;
}

bool rwlogd_filter:: check_deny_event_filter(const char *name, const char *value)
  {
    RWLOG_FILTER_DEBUG_PRINT("sink : check_deny_event_filter %s %s\n", name,value);
    if(find_exact_match_deny_filter_string(&deny_filter_info,name,value)==RWLOG_FILTER_MATCHED)
    {
      RWLOG_FILTER_DEBUG_PRINT("sink : check_string_filter matched\n");
      return RWLOG_FILTER_MATCHED;
    }
    return RWLOG_CHECK_MORE_FILTERS;
  }
bool rwlogd_filter:: check_groupid_filter(char *name, rw_call_id_t *value)
  {
    if (!value)
    {
      return RWLOG_FILTER_DROP;
    }

    RWLOG_FILTER_DEBUG_PRINT("sink : check_groupid_filter matched %s %llu:%llu \n",
                             name,
                             (long long unsigned int) value->groupid,
                             (long long unsigned int) value->callid);
    if(find_exact_match_filter_uint64(&filter_info,name,value->groupid)==RWLOG_FILTER_MATCHED)
    {
      RWLOG_FILTER_DEBUG_PRINT("sink : check_groupid_filter matched\n");
      return RWLOG_FILTER_MATCHED;
    }

    if(find_exact_match_filter_uint64(&filter_info,name,value->callid)==RWLOG_FILTER_MATCHED)
    {
      RWLOG_FILTER_DEBUG_PRINT("sink : check_callid_filter matched\n");
      return RWLOG_FILTER_MATCHED;
    }
    return RWLOG_CHECK_MORE_FILTERS;
  }

bool rwlogd_filter:: check_uint64_filter(const char *name, uint64_t value)
  {
    RWLOG_FILTER_DEBUG_PRINT("sink : check_uint64_filter %s %llu \n",
                             name, (long long unsigned int) value);

    if(find_exact_match_filter_uint64(&filter_info,name,value)==RWLOG_FILTER_MATCHED)
    {
      RWLOG_FILTER_DEBUG_PRINT("sink : check_uint64_filter matched\n");
      return RWLOG_FILTER_MATCHED;
    }
    return RWLOG_CHECK_MORE_FILTERS;
  }

  bool rwlogd_filter::check_severity(rwlog_severity_t sev, uint32_t cat)
  {
     RWLOG_FILTER_DEBUG_PRINT("sink : check_severity Matched %d %d %d\n",
                              cat,
                              filter_hdr_.severity[cat],
                              sev);

    if ((cat < filter_hdr_.num_categories) &&
        (category_filter_[cat].severity >= sev))
    {
      RWLOG_FILTER_DEBUG_PRINT("sink : check_severity Matched \n");
      stats.severity_check_passed++;
      return RWLOG_FILTER_MATCHED;
    }
    stats.severity_check_failed++;
    return RWLOG_FILTER_DROP;
  }

bool rwlogd_filter:: check_critical_info_filter(uint16_t critical_info_flag, uint32_t cat)
{
  if(critical_info_flag  && category_filter_[cat].critical_info == RW_LOG_ON_OFF_TYPE_ON) {
    return RWLOG_FILTER_MATCHED;
  }
  return RWLOG_FILTER_DROP;
}

bool rwlogd_filter:: check_int_filter(const char *name, int value)
  {
    RWLOG_FILTER_DEBUG_PRINT("sink : check_int_filter matchedn");
    return 0;
  }

/*****************************************************************************
 * find_exact_match_filter_string 
 * Params: rwlog_ctx_t
 * Params: Name of the field in the log
 * Params: The value passed by the caller
 *
 * Returns 0 for match
 *         non-zero for no-match
 * TBD: 1 for gt and -1 for lt 
 ****************************************************************************/
filter_attribute* find_exact_match_filter_string(filter_attribute **filter_info,
                                                 char *category_str,
                                                 const char *field_name, 
                                                 const char *value)
{
  char *filter_key;

  if (!field_name || !value)
  {
    RWLOG_ERROR_PRINT("find_bloom:Incorrect Parameters\n");
    return NULL;
  }
  int r = asprintf (&filter_key, 
                    "%s:%s:%s",
                    field_name,
                    value,
                    category_str);
  RW_ASSERT(r);
  if (!r) {
    return NULL;
  }

  filter_attribute *name =  NULL;
  HASH_FIND_STR(*filter_info, filter_key , name);
  free(filter_key);
  if (name) 
  {
    return name;
  }
  return NULL;// 1 gt , not equal
}

/*****************************************************************************
 * find_exact_match_deny_filter_string 
 * Params: rwlog_ctx_t
 * Params: Name of the field in the log
 * Params: The value passed by the caller
 *
 * Returns 0 for match
 *         non-zero for no-match
 * TBD: 1 for gt and -1 for lt 
 ****************************************************************************/
rwlog_status_t find_exact_match_deny_filter_string(filter_attribute **deny_filter_info,
						   const char *field_name, 
						   const char *value)
{
  filter_attribute *name =  NULL;
  HASH_FIND_STR((*deny_filter_info), value , name);
  if (name) 
  {
    return RWLOG_FILTER_MATCHED;
  }
  return RWLOG_FILTER_DROP;// 1 gt , not equal
}

/*****************************************************************************
 * find_exact_match_filter_uint64
 * Params: rwlog_ctx_t
 * Params: Name of the field in the log
 * Params: The value passed by the caller
 *
 * Returns 0 for match
 *         non-zero for no-match
 * TBD: 1 for gt and -1 for lt 
 ****************************************************************************/
rwlog_status_t find_exact_match_filter_uint64(filter_attribute **filter_info,
                             const char *field_name, 
                             uint64_t value)
{
  filter_attribute *name =  NULL;
  char filter_key[MAX_ATTRIBUTE_LEN];

  strncpy(filter_key,field_name,MAX_ATTRIBUTE_LEN);
  if(MAX_ATTRIBUTE_LEN - strlen(filter_key) > 1) {
    snprintf(filter_key+strlen(filter_key),(MAX_ATTRIBUTE_LEN-strlen(filter_key)),":%lu",value);
  }
  filter_key[MAX_ATTRIBUTE_LEN-1] = '\0';


  HASH_FIND_STR(*filter_info, filter_key, name);
  if (name) 
  {
    return RWLOG_FILTER_MATCHED;
  }
  return RWLOG_CHECK_MORE_FILTERS;// 1 gt , not equal
}

rw_status_t find_exact_match_filter_uint64_new(filter_attribute **filter_info,
                             uint32_t cat,
                             const char *field_name,
                             uint64_t value)
{
  char *filter_key;
  int r = asprintf (&filter_key,
                    "%s:%lu",
                    field_name,
                    value);
  RW_ASSERT(r);
  if (!r) {
    return RW_STATUS_FAILURE;
  }

  filter_attribute *name =  NULL;

  HASH_FIND_STR(*filter_info, filter_key, name);
  free(filter_key);
  if (name)
  {
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_FAILURE;// 1 gt , not equal
}
/*****************************************************************************
 * remove_exact_match_filter_string 
 * Params: 
 * Params: Name of the field in the log
 * Params: The value passed by the caller
 *
 * Returns 0 for match
 *         non-zero for no-match
 * TBD: 1 for gt and -1 for lt 
 ****************************************************************************/
rw_status_t remove_exact_match_filter_string(filter_attribute **filter_info,
                                             char *category_str,
                                             char *field_name, 
                                             char *value)
{
  filter_attribute *name =  NULL;
  rw_status_t status = RW_STATUS_FAILURE;
  char *filter_key;
  int r = asprintf (&filter_key, 
                    "%s:%s:%s",
                    field_name,
                    value,
                    category_str);
  RW_ASSERT(r);
  if (!r) {
    return RW_STATUS_FAILURE;
  }
  HASH_FIND_STR(*filter_info,filter_key,name);
  if (name) 
  {
    HASH_DEL(*filter_info, name);
    free(name);
    status = RW_STATUS_SUCCESS;
  }
  free(filter_key);
  return status;
}
/*****************************************************************************
 * remove_exact_match_filter_uint64
 * Params: 
 * Params: Name of the field in the log
 * Params: The value passed by the caller
 *
 * Returns 0 for match
 *         non-zero for no-match
 * TBD: 1 for gt and -1 for lt 
 ****************************************************************************/
rw_status_t remove_exact_match_filter_uint64(filter_attribute **filter_info,
                                       char *field_name, 
                                       uint64_t value)
{
  filter_attribute *name =  NULL;
  char filter_key[MAX_ATTRIBUTE_LEN];

  strncpy(filter_key,field_name,MAX_ATTRIBUTE_LEN);
  if(MAX_ATTRIBUTE_LEN - strlen(filter_key) > 1) {
    snprintf(filter_key+strlen(filter_key),(MAX_ATTRIBUTE_LEN-strlen(filter_key)),":%lu",value);
  }
  filter_key[MAX_ATTRIBUTE_LEN-1] = '\0';

  HASH_FIND_STR(*filter_info, filter_key, name);
  if (name) 
  {
    HASH_DEL(*filter_info, name);
    free(name);
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_FAILURE;
}

rw_status_t remove_exact_match_filter_uint64_new(filter_attribute **filter_info,
                                       uint32_t cat,
                                       char *field_name,
                                       uint64_t value)
{
  filter_attribute *name =  NULL;
  char *filter_key;
  rw_status_t status = RW_STATUS_FAILURE;

  int r = asprintf (&filter_key,
                    "%s:%lu:%d",
                    field_name,
                    value,
                    cat);
  RW_ASSERT(r);
  if (!r) {
    return RW_STATUS_FAILURE;
  }
  HASH_FIND_STR(*filter_info, filter_key, name);
  if (name)
  {
    HASH_DEL(*filter_info, name);
    free(name);
    status= RW_STATUS_SUCCESS;
  }
  free(filter_key);
  return status;
}

/*****************************************************************************
 * add_exact_match_deny_filter_string
 * Insert the filter in the global data structure 
 *
 * Params: 
 *****************************************************************************/
extern "C" rw_status_t add_exact_match_deny_filter_string(filter_attribute **deny_filter_info,
                                    char *field_name,
                                    char *value)
{
  if (find_exact_match_deny_filter_string(deny_filter_info,field_name,value) == 0)
  {
    //RWLOG_TRACE("MATCH FOUND::%s:%s\n", field_name, value);
    return RW_STATUS_SUCCESS;
  }
  filter_attribute *name =  NULL;
  name = (filter_attribute*)malloc(sizeof(filter_attribute));
  if (name == NULL)
  {
    RW_CRASH();
  }
  strncpy(name->attribute_value_string,value,MAX_ATTRIBUTE_LEN);

  HASH_ADD_STR(*deny_filter_info,attribute_value_string,name);

  return RW_STATUS_SUCCESS;
}

/*****************************************************************************
 * add_exact_match_filter_string
 * Insert the filter in the global data structure 
 *
 * Params: 
 *****************************************************************************/
extern "C" rw_status_t add_exact_match_filter_string(filter_attribute **filter_info ,
                                                     char *category_str,
                                                     char *field_name,
                                                     char *value,
                                                     int next_call_flag)
{
  if (find_exact_match_filter_string(filter_info, category_str, field_name,value) != NULL)
  {
    //RWLOG_TRACE("MATCH FOUND::%s:%s\n", field_name, value);
    return RW_STATUS_DUPLICATE;
  }
  filter_attribute *name =  NULL;
  name = (filter_attribute*)malloc(sizeof(filter_attribute));
  if (name == NULL)
  {
    RW_CRASH();
  }
  int r = snprintf (name->attribute_value_string, 
                    MAX_ATTRIBUTE_LEN,
                    "%s:%s:%s", 
                    field_name, 
                    value, 
                    category_str);
  RW_ASSERT(r);
  if(!r) {
    return RW_STATUS_FAILURE;
  }
  name->next_call_flag =  next_call_flag;

  HASH_ADD_STR(*filter_info,attribute_value_string,name);
  return RW_STATUS_SUCCESS;
}
/*****************************************************************************
 * add_exact_match_filter_uint64
 * Insert the filter in the global data structure 
 *
 * Params: 
 *****************************************************************************/
extern "C" rw_status_t add_exact_match_filter_uint64(filter_attribute **filter_info,
                                    char *field_name,
                                    uint64_t value)
{
  char filter_key[MAX_ATTRIBUTE_LEN];

  if (find_exact_match_filter_uint64(filter_info,field_name,value) == 0)
  {
    //RWLOG_TRACE("MATCH FOUND::%s:%s\n", field_name, value);
    return RW_STATUS_SUCCESS;
  }
  filter_attribute *name =  NULL;
  name = (filter_attribute*)malloc(sizeof(filter_attribute));
  if (name == NULL)
  {
    RW_CRASH();
  }


  strncpy(filter_key,field_name,MAX_ATTRIBUTE_LEN);
  if(MAX_ATTRIBUTE_LEN - strlen(filter_key) > 1) {
    snprintf(filter_key+strlen(filter_key),(MAX_ATTRIBUTE_LEN-strlen(filter_key)),":%lu",value);
  }
  filter_key[MAX_ATTRIBUTE_LEN-1] = '\0';

  RWLOG_FILTER_DEBUG_PRINT("Adding uthash filter for %s \n",filter_key);

  memcpy(name->attribute_value_string,filter_key,MAX_ATTRIBUTE_LEN);

  HASH_ADD_STR(*filter_info,attribute_value_string,name);
  return RW_STATUS_SUCCESS;
}

extern "C" rw_status_t add_exact_match_filter_uint64_new(filter_attribute **filter_info,
                                    uint32_t cat,
                                    char *field_name,
                                    uint64_t value)
{
  if (find_exact_match_filter_uint64_new(filter_info,cat,field_name,value) == RW_STATUS_SUCCESS)
  {
    //RWLOG_TRACE("MATCH FOUND::%s:%s\n", field_name, value);
    return RW_STATUS_SUCCESS;
  }
  filter_attribute *name =  NULL;
  name = (filter_attribute*)malloc(sizeof(filter_attribute));
  if (name == NULL)
  {
    RW_CRASH();
  }

  int r = snprintf (name->attribute_value_string,
                    MAX_ATTRIBUTE_LEN,
                    "%s:%lu",
                    field_name,
                    value);
  RW_ASSERT(r);
  if (!r) {
    return RW_STATUS_FAILURE;
  }
  HASH_ADD_STR(*filter_info,attribute_value_string,name);
  return RW_STATUS_SUCCESS;
}
