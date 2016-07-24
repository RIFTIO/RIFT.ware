/*
 * Example program for unwinding core dumps.
 *
 * Compile a-la:
 * gcc -Os -Wall \
 *    -Wl,--start-group \
 *        -lunwind -lunwind-x86 -lunwind-coredump \
 *        example-core-unwind.c \
 *    -Wl,--end-group \
 *    -oexample-core-unwind
 *
 * Run:
 * eu-unstrip -n --core COREDUMP
 *   figure out which virtual addresses in COREDUMP correspond to which mapped executable files
 *   (binary and libraries), then supply them like this:
 * ./example-core-unwind COREDUMP 0x400000:/bin/crashed_program 0x3458600000:/lib/libc.so.6 [...]
 *
 * Note: Program eu-unstrip is part of elfutils, virtual addresses of shared
 * libraries can be determined by ldd (at least on linux).
 */

#include "compiler.h"

#undef _GNU_SOURCE
#define _GNU_SOURCE 1
#undef __USE_GNU
#define __USE_GNU 1

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>

/* For SIGSEGV handler code */
#include <execinfo.h>
#include <sys/ucontext.h>

#include <libunwind-coredump.h>

#define RIFT_CHANGES

#ifdef RIFT_CHANGES

#include <endian.h>
#include <stdint.h>
// Caution - these changes works only for x86_64 architecture now
#define NT_FILE         0x46494c45
typedef struct {
  unsigned char ident[16];
  uint16_t type;
  uint16_t arch;
  uint32_t ver;
  uint64_t va_entry;
  uint64_t phoff;
  uint64_t shoff;
  unsigned char flags[4];
  uint16_t hsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elf64_header;

typedef struct {
  uint32_t type;
  unsigned char flags[4];
  uint64_t offset;
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t filesz;
  uint64_t memsz;
  unsigned char align[8];
} elf64_phdr;

typedef struct {
  uint32_t namesz;
  uint32_t descsz;
  uint32_t type;
  char          name[1];
} elfnote;


#define addr_align(addr)        \
  (((addr) + ((uint64_t) 1 << (2)) - 1) & (-((uint64_t) 1 << (2))))

static int read_elfnotes(char *corefilename, struct UCD_info *ui);

#endif

/* Utility logging functions */

enum {
    LOGMODE_NONE = 0,
    LOGMODE_STDIO = (1 << 0),
    LOGMODE_SYSLOG = (1 << 1),
    LOGMODE_BOTH = LOGMODE_SYSLOG + LOGMODE_STDIO,
};
const char *msg_prefix = "";
const char *msg_eol = "\n";
int logmode = LOGMODE_STDIO;
int xfunc_error_retval = EXIT_FAILURE;

void xfunc_die(void)
{
  exit(xfunc_error_retval);
}

static void verror_msg_helper(const char *s,
                              va_list p,
                              const char* strerr,
                              int flags)
{
  char *msg;
  int prefix_len, strerr_len, msgeol_len, used;

  if (!logmode)
    return;

  used = vasprintf(&msg, s, p);
  if (used < 0)
    return;

  /* This is ugly and costs +60 bytes compared to multiple
   * fprintf's, but is guaranteed to do a single write.
   * This is needed for e.g. when multiple children
   * can produce log messages simultaneously. */

  prefix_len = msg_prefix[0] ? strlen(msg_prefix) + 2 : 0;
  strerr_len = strerr ? strlen(strerr) : 0;
  msgeol_len = strlen(msg_eol);
  /* +3 is for ": " before strerr and for terminating NUL */
  char *msg1 = (char*) realloc(msg, prefix_len + used + strerr_len + msgeol_len + 3);
  if (!msg1)
    {
      free(msg);
      return;
    }
  msg = msg1;
  /* TODO: maybe use writev instead of memmoving? Need full_writev? */
  if (prefix_len)
    {
      char *p;
      memmove(msg + prefix_len, msg, used);
      used += prefix_len;
      p = stpcpy(msg, msg_prefix);
      p[0] = ':';
      p[1] = ' ';
    }
  if (strerr)
    {
      if (s[0])
        {
          msg[used++] = ':';
          msg[used++] = ' ';
        }
      strcpy(&msg[used], strerr);
      used += strerr_len;
    }
  strcpy(&msg[used], msg_eol);

  if (flags & LOGMODE_STDIO)
    {
      fflush(stdout);
      used += write(STDERR_FILENO, msg, used + msgeol_len);
    }
  // following line corrupts memory so disabled
#ifndef RIFT_CHANGES 
  msg[used] = '\0'; /* remove msg_eol (usually "\n") */
#endif
  if (flags & LOGMODE_SYSLOG)
    {
      syslog(LOG_ERR, "%s", msg + prefix_len);
    }
  free(msg);
}

void log_msg(const char *s, ...)
{
  va_list p;
  va_start(p, s);
  verror_msg_helper(s, p, NULL, logmode);
  va_end(p);
}
/* It's a macro, not function, since it collides with log() from math.h */
#undef log
#define log(...) log_msg(__VA_ARGS__)

void error_msg(const char *s, ...)
{
  va_list p;
  va_start(p, s);
  verror_msg_helper(s, p, NULL, logmode);
  va_end(p);
}

void error_msg_and_die(const char *s, ...)
{
  va_list p;
  va_start(p, s);
  verror_msg_helper(s, p, NULL, logmode);
  va_end(p);
  xfunc_die();
}

void perror_msg(const char *s, ...)
{
  va_list p;
  va_start(p, s);
  /* Guard against "<error message>: Success" */
  verror_msg_helper(s, p, errno ? strerror(errno) : NULL, logmode);
  va_end(p);
}

void perror_msg_and_die(const char *s, ...)
{
  va_list p;
  va_start(p, s);
  /* Guard against "<error message>: Success" */
  verror_msg_helper(s, p, errno ? strerror(errno) : NULL, logmode);
  va_end(p);
  xfunc_die();
}

void die_out_of_memory(void)
{
  error_msg_and_die("Out of memory, exiting");
}

/* End of utility logging functions */



static
void handle_sigsegv(int sig, siginfo_t *info, void *ucontext)
{
  long ip = 0;
  ucontext_t *uc UNUSED;

  uc = ucontext;
#if defined(__linux__)
#ifdef UNW_TARGET_X86
	ip = uc->uc_mcontext.gregs[REG_EIP];
#elif defined(UNW_TARGET_X86_64)
	ip = uc->uc_mcontext.gregs[REG_RIP];
#elif defined(UNW_TARGET_ARM)
	ip = uc->uc_mcontext.arm_pc;
#endif
#elif defined(__FreeBSD__)
#ifdef __i386__
	ip = uc->uc_mcontext.mc_eip;
#elif defined(__amd64__)
	ip = uc->uc_mcontext.mc_rip;
#else
#error Port me
#endif
#else
#error Port me
#endif
  dprintf(2, "signal:%d address:0x%lx ip:0x%lx\n",
			sig,
			/* this is void*, but using %p would print "(null)"
			 * even for ptrs which are not exactly 0, but, say, 0x123:
			 */
			(long)info->si_addr,
			ip);

  {
    /* glibc extension */
    void *array[50];
    int size;
    size = backtrace(array, 50);
#ifdef __linux__
    backtrace_symbols_fd(array, size, 2);
#endif
  }

  _exit(1);
}

static void install_sigsegv_handler(void)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = handle_sigsegv;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
}

int
main(int argc UNUSED, char **argv)
{
  unw_addr_space_t as;
  struct UCD_info *ui;
  unw_cursor_t c;
  int ret;
#ifdef RIFT_CHANGES
  char *corefile = NULL;
#define TEST_FRAMES 256
#define TEST_NAME_LEN 256
#else 

#define TEST_FRAMES 4
#define TEST_NAME_LEN 32

#endif

  int testcase = 0;
  int test_cur = 0;
  long test_start_ips[TEST_FRAMES];
  char test_names[TEST_FRAMES][TEST_NAME_LEN];

  install_sigsegv_handler();

  const char *progname = strrchr(argv[0], '/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  if (!argv[1])
    error_msg_and_die("Usage: %s COREDUMP [VADDR:BINARY_FILE]...", progname);

#ifdef RIFT_CHANGES
   corefile = argv[1];
#endif
  msg_prefix = progname;

  as = unw_create_addr_space(&_UCD_accessors, 0);
  if (!as)
    error_msg_and_die("unw_create_addr_space() failed");

  ui = _UCD_create(argv[1]);
  if (!ui)
    error_msg_and_die("_UCD_create('%s') failed", argv[1]);
  ret = unw_init_remote(&c, as, ui);
  if (ret < 0)
    error_msg_and_die("unw_init_remote() failed: ret=%d\n", ret);

  argv += 2;

  /* Enable checks for the crasher test program? */
  if (*argv && !strcmp(*argv, "-testcase"))
  {
    testcase = 1;
    logmode = LOGMODE_NONE;
    argv++;
  }

#ifdef RIFT_CHANGES
  read_elfnotes(corefile, ui);
#else 
  while (*argv)
    {
      char *colon;
      long vaddr = strtol(*argv, &colon, 16);
      if (*colon != ':')
        error_msg_and_die("Bad format: '%s'", *argv);
      if (_UCD_add_backing_file_at_vaddr(ui, vaddr, colon + 1) < 0)
        error_msg_and_die("Can't add backing file '%s'", colon + 1);
      argv++;
    }
#endif

  for (;;)
    {
      unw_word_t ip;
      ret = unw_get_reg(&c, UNW_REG_IP, &ip);
      if (ret < 0)
        error_msg_and_die("unw_get_reg(UNW_REG_IP) failed: ret=%d\n", ret);

      unw_proc_info_t pi;
      ret = unw_get_proc_info(&c, &pi);
      if (ret < 0)
        error_msg_and_die("unw_get_proc_info(ip=0x%lx) failed: ret=%d\n", (long) ip, ret);

      if (!testcase)
        printf("\tip=0x%08lx proc=%08lx-%08lx handler=0x%08lx lsda=0x%08lx\n",
				(long) ip,
				(long) pi.start_ip, (long) pi.end_ip,
				(long) pi.handler, (long) pi.lsda);

      if (testcase && test_cur < TEST_FRAMES)
        {
           unw_word_t off;

           test_start_ips[test_cur] = (long) pi.start_ip;
           if (unw_get_proc_name(&c, test_names[test_cur], sizeof(test_names[0]), &off) != 0)
           {
             test_names[test_cur][0] = '\0';
           }
           test_cur++;
        }

      log("step");
      ret = unw_step(&c);
      log("step done:%d", ret);
      if (ret < 0)
    	error_msg_and_die("FAILURE: unw_step() returned %d", ret);
      if (ret == 0)
        break;
    }
  log("stepping ended");

  /* Check that the second and third frames are equal, but distinct of the
   * others */
  if (testcase &&
       (test_cur != 4
       || test_start_ips[1] != test_start_ips[2]
       || test_start_ips[0] == test_start_ips[1]
       || test_start_ips[2] == test_start_ips[3]
       )
     )
    {
      fprintf(stderr, "FAILURE: start IPs incorrect\n");
      return -1;
    }

  if (testcase &&
       (  strcmp(test_names[0], "a")
       || strcmp(test_names[1], "b")
       || strcmp(test_names[2], "b")
       || strcmp(test_names[3], "main")
       )
     )
    {
      fprintf(stderr, "FAILURE: procedure names are missing/incorrect\n");
      return -1;
    }

  _UCD_destroy(ui);
  unw_destroy_addr_space(as);

  return 0;
}
#ifdef RIFT_CHANGES
static int
read_elfnotes(char *corefilename, struct UCD_info *ui)
{

  FILE *fp = NULL;
  struct stat stbuf;
  elf64_header header;
  elf64_phdr *program_headers = NULL;
  unsigned char *notes = NULL;
  int i;
  size_t ret;
  elfnote *note;
  unsigned char *end=NULL, *curr=NULL;
  unsigned char *filenames = NULL, *descend = NULL , *previous_filenames = NULL;
  uint64_t count;
  int retval = -1;

  if (stat(corefilename, &stbuf) < 0 ) {
    goto func_exit;
  }
  if (!S_ISREG(stbuf.st_mode)) {
    goto func_exit;
  }

  fp = fopen(corefilename, "r");
  if (fp == NULL) {
    goto func_exit;
  }
  memset(&header, 0, sizeof(header));

  if (fread(&header, sizeof(header), 1, fp)!= 1) {
    goto func_exit;
  }

  if (header.ident[0]  !=0x7f ||header.ident[1]!='E'
      ||header.ident[2]!='L'  ||header.ident[3] !='F')
  {
    goto func_exit;
  }
  if ((header.ident[5] == 2)  || (header.ident[4] != 2)
      || (header.phnum == 0 ) || (header.phentsize != sizeof(elf64_phdr))) {
    goto func_exit;
  }

  program_headers = malloc(sizeof(elf64_phdr)*header.phnum);
  if (program_headers == NULL) {
    goto func_exit;
  }

  if (fseek(fp,(long)header.phoff,SEEK_SET) == -1) {
    goto func_exit;
  }

  if (fread(program_headers, sizeof(elf64_phdr), header.phnum, fp) !=  header.phnum) {
    goto func_exit;
  }


  for (i=0; i< header.phnum; i++) {
    if (program_headers[i].type != 4) continue;

    retval = fseek(fp,(long) program_headers[i].offset, SEEK_SET);
    if (retval == -1) {
      goto func_exit;
    }
    notes = malloc(program_headers[i].filesz);
    if (notes == NULL) {
      goto func_exit;
    }

    ret = fread(notes, 1, program_headers[i].filesz, fp);
    if (ret !=  program_headers[i].filesz ) {
      goto func_exit;
    }
    end = notes + program_headers[i].filesz;
    curr = notes;
    while (curr <  end) {
      unsigned char *descdata;
      note =  (elfnote*) curr;
      descdata = (unsigned char *) note->name + addr_align (note->namesz);

      if (note->type != NT_FILE) {
        curr = (unsigned char *) (descdata + addr_align (note->descsz));
        continue;
      }

      if (note->descsz < 16)  goto func_exit;
      descend = descdata + note->descsz;
      if (descdata[note->descsz - 1] != '\0') goto func_exit;

      count =  *((uint64_t *)descdata);
      descdata += 8;

      descdata += 8;

      if (note->descsz < 16 + count * 24 ) goto func_exit;

      filenames = descdata + count * 3 * 8;
      while (count-- > 0)
      {
        uint64_t start;

        if (filenames == descend) goto func_exit;

        start =  *((uint64_t*)descdata);
        descdata += 8;
        descdata += 8;
        descdata += 8;

        if (filenames && previous_filenames && !strcmp((char*)filenames,(char*)previous_filenames)) {
           previous_filenames = filenames;
           filenames += 1 + strlen ((char *) filenames);
           continue;
        }

        if (_UCD_add_backing_file_at_vaddr(ui, (long)start, (char *)filenames) < 0) {
          fprintf(stderr, "uid add backing file failed");
        }
        previous_filenames = filenames;
        filenames += 1 + strlen ((char *) filenames);
      }
      
      curr = (unsigned char *) (descdata + addr_align (note->descsz));
    }
    if (notes) free(notes);
    notes = NULL;
  }
  retval = 0;

func_exit:
  if (fp) fclose(fp);
  if (program_headers) free(program_headers);
  if (notes) free(notes);
  return(retval);
}
#endif
