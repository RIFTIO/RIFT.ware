/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_file.c
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 04/26/2013
 * @brief RIFT utilities
 * 
 * @details 
 *
 */

#include "rwlib.h"

rw_status_t rw_mkpath(const char *path, mode_t mode)
{
  char *tmp;
  char *p = NULL;
  size_t len;
 
  tmp = strdup(path);
  RW_ASSERT(tmp);

  len = strlen(tmp);
  if (tmp[len-1] == '/') {
    tmp[len-1] = 0;
  }
  
  for(p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
       mkdir(tmp, mode);
       *p = '/';
    }
  }
  mkdir(tmp, mode);

  RW_FREE(tmp);

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_delfiles_in_dir(const char *dir_path)
{
  char file_name[612];
  int  dir_path_len = 0;
  DIR *dir;
  struct dirent *dent;                                                                                                                                                                                                  
  RW_ASSERT(dir_path);

  dir_path_len = strlen(dir_path);
  dir = opendir(dir_path);

  if (NULL != dir) {
    while(NULL != (dent = readdir(dir))) {
      if ((dir_path_len + strlen(dent->d_name)) >= sizeof(file_name)) {
        RW_ASSERT_NOT_REACHED();
      }
      sprintf(file_name,"%s/%s",dir_path,dent->d_name);
      unlink(file_name);
    }
  }

  closedir(dir);

  return RW_STATUS_SUCCESS;
}

