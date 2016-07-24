#ifndef __TCP_EXPORT_H__
#define __TCP_EXPORT_H__
#include <pthread.h>

pthread_key_t log_print_key;
#define MAX_PDU_SIZE 4096

typedef struct {
  char tcpdump_out_buf[MAX_PDU_SIZE];
  int write_count;
  int verbosity;
} rwtcpdump_state_t;

#ifdef TCPDUMP_BUF
#define printf rwtcpdump_printf_override
#define vflag (rwtcpdump_get_vflag())
#endif

extern int rwtcpdump_printf_override(const char *__restrict __format,...);
extern int rwtcpdump_get_vflag();
extern void rwtcpdump_decr_vflag(int dec_verbosity);
extern void rwtcpdump_incr_vflag(int inc_verbosity);
#endif
