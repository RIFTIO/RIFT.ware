
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include "rwlog_filters.h"
#include <limits.h>
rw_status_t rwlog_app_filter_destroy(rwlog_ctx_t *ctxt)
{
  if ((ctxt->local_shm == true) && (ctxt->filter_shm_fd == -1)) {
    return RW_STATUS_SUCCESS;
  }
  if(ctxt->filter_shm_fd > 0 && ctxt->filter_memory) {
    munmap(ctxt->filter_memory,RWLOG_FILTER_SHM_SIZE);
    ctxt->filter_memory = NULL;
  }
  if(close(ctxt->filter_shm_fd) == -1)
  {
    return RW_STATUS_FAILURE;
  }
  ctxt->filter_shm_fd = -1;
  return RW_STATUS_SUCCESS;
}

rwlogd_shm_ctrl_t *rwlog_static_filter()
{
  static rwlogd_shm_ctrl_t static_shm;
  filter_memory_header * filter_hdr_ = &(static_shm.hdr);

  filter_hdr_->magic=RWLOG_FILTER_MAGIC;
  filter_hdr_->rwlogd_pid=-1;
  filter_hdr_->rotation_serial=0;

  filter_hdr_->static_bitmap =  0;
  filter_hdr_->log_serial_no = UINT_MAX;

  filter_hdr_->skip_L1 = TRUE;
  filter_hdr_->skip_L2 = TRUE;
  filter_hdr_->allow_dup = TRUE;
  filter_hdr_->bootstrap = TRUE;
  filter_hdr_->rwlogd_flow = FALSE;
  filter_hdr_->next_call_filter = FALSE;
  filter_hdr_->failed_call_filter = FALSE;
  filter_hdr_->rwlogd_flow = FALSE;
  filter_hdr_->rwlogd_ticks=0;

  static_shm.last_location = sizeof(rwlogd_shm_ctrl_t);
  return (&static_shm);
}

rw_status_t rwlog_app_filter_setup(rwlog_ctx_t *ctxt)
{
  int perms = 0600;           /* permissions */
  int  oflags  = O_CREAT | O_RDWR; // Write for the apps.
  int mprot = PROT_READ;
  int mflags = MAP_FILE|MAP_SHARED;
  int result;
  char *rwlog_shm;
  rwlogd_shm_ctrl_t *rwlogd_shm_ctrl;
  filter_memory_header * filter_header;
  int shm_fd;
  static rwlog_category_filter_t category_filter;

  RW_ASSERT(ctxt->rwlog_shm);
  if (!ctxt->rwlog_shm) {
    return RW_STATUS_FAILURE;
  }
  rwlog_shm = ctxt->rwlog_shm;

  shm_fd =  shm_open(rwlog_shm,oflags,perms);
  if (shm_fd < 0)
  {
    fprintf(stderr,"opening rwlog shm file %s in host %s failed fd %d;try deleting file in /dev/shm if it exists \n",rwlog_shm,ctxt->hostname,shm_fd); 
    ctxt->error_code |= RWLOG_ERR_FILTER_SHM_FD;
    RWLOG_FILTER_DEBUG_PRINT("Error Open %s for  SHM%s\n", 
                             strerror(errno), rwlog_shm);
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }
  rwlogd_shm_ctrl =
      (rwlogd_shm_ctrl_t *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, shm_fd, 0);
  if (!rwlogd_shm_ctrl)
  {
    fprintf(stderr,"mmap of rwlog shm for fd %d failed\n",shm_fd); 
    ctxt->error_code |= RWLOG_ERR_FILTER_MMAP;
    RWLOG_FILTER_DEBUG_PRINT("Error mmap %s for  SHM%s\n", 
                             strerror(errno), rwlog_shm);
    RW_CRASH();
    close(shm_fd);
    return RW_STATUS_FAILURE;
  }
  result = ftruncate(shm_fd, RWLOG_FILTER_SHM_SIZE);

  if (result == -1) 
  {
    fprintf(stderr,"ftruncate of rwlog shm for fd %d failed\n",shm_fd); 
    ctxt->error_code |= RWLOG_ERR_FILTER_NOMEM;
    RW_CRASH();
    munmap(rwlogd_shm_ctrl,RWLOG_FILTER_SHM_SIZE);
    close(shm_fd);
    return RW_STATUS_FAILURE;
  }

  filter_header = &rwlogd_shm_ctrl->hdr;

  /* if rwlogd has initialized the shared memory and rwlogd running use that */
  if (filter_header->magic == RWLOG_FILTER_MAGIC &&  !kill(filter_header->rwlogd_pid,0)) {
    ctxt->local_shm = false;
    ctxt->category_filter = (rwlog_category_filter_t *)((char *)filter_header + sizeof(rwlogd_shm_ctrl_t));
    ctxt->filter_shm_fd = shm_fd;
    ctxt->filter_memory = &rwlogd_shm_ctrl->hdr;
    return RW_STATUS_SUCCESS;
  }

  /* shared memory not yet read setup local filter pointers for now */
  munmap(rwlogd_shm_ctrl,RWLOG_FILTER_SHM_SIZE);
  close(shm_fd);
  ctxt->filter_shm_fd = -1;
  ctxt->local_shm = true;
  rwlogd_shm_ctrl = rwlog_static_filter();
  ctxt->filter_memory = &rwlogd_shm_ctrl->hdr;
  filter_header =  (filter_memory_header *)ctxt->filter_memory;

  strcpy(category_filter.name,"all");
  category_filter.severity = RW_LOG_LOG_SEVERITY_DEBUG;
  category_filter.bitmap = 0;
  category_filter.critical_info = 1;
  ctxt->category_filter = &category_filter;
  filter_header->num_categories = 1;

  return RW_STATUS_SUCCESS;
}

void rwlog_app_filter_dump(void *filter)
{
  int i;
  filter_memory_header *mem_hdr =  (filter_memory_header *)filter;
  printf("Filter SHM info %p\n", filter);
  printf("---------------------\n");
  printf("Num Categories: %d\n",mem_hdr->num_categories);
  printf ("Index   Category     Severity   BloomBitMap    Critical-Info\n");


  rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)mem_hdr + sizeof(rwlogd_shm_ctrl_t)); 
  for(i = 0; i < mem_hdr->num_categories; i++) {
     printf("%-4d %-20s %-10d %-30lx %-2d\n",i,category_filter[i].name,category_filter[i].severity,category_filter[i].bitmap,category_filter[i].critical_info);
  }

  printf("\n");
  printf("Skip L1: %d \t Skip L2: %d\n\n",mem_hdr->skip_L1, mem_hdr->skip_L2);

  rwlogd_shm_ctrl_t *rwlogd_shm_ctrl_ = (rwlogd_shm_ctrl_t *)filter;
  for (i = 0; i<RWLOG_FILTER_HASH_SIZE; i++)
  {
    int j;
    if (rwlogd_shm_ctrl_->fieldvalue_entries[i].n_entries)
    {
      printf ("fieldvalue_entries = %d @ index %d\n",
              rwlogd_shm_ctrl_->fieldvalue_entries[i].n_entries,
              i);
      for (j = 0;
           j< rwlogd_shm_ctrl_->fieldvalue_entries[i].n_entries;
           j++)
      {
        char *str = rwlogd_shm_ctrl_->fieldvalue_entries[i].table_offset +
            (char *)rwlogd_shm_ctrl_ +
            j * RWLOG_FILTER_STRING_SIZE;
        printf ("\t%p:%s", (void *)str, str);
      }
      printf("\n");
    }
  }
  printf("\n");
}
