 
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
#include <execinfo.h>
#include <sys/ucontext.h>
#include <libunwind-coredump.h>
#include <endian.h>
#include <stdint.h>
#include "Python.h"
#include "frameobject.h"
#include "python2.7/stringobject.h"
#include "unicodeobject.h"
#define ADDR2LINE "/usr/bin/addr2line"
#define MAX_STACK_DEPTH 255

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

static
void signal_handler(int sig, siginfo_t *info, void *ucontext)
{
  long ip = 0;
  ucontext_t *uc ;
  void *array[50];
  int size;

  uc = ucontext;
  ip = uc->uc_mcontext.gregs[REG_RIP];
  fprintf(stderr, "signal:%d address:0x%lx ip:0x%lx\n",
      sig, (long)info->si_addr, ip);
  size = backtrace(array, 50);
  backtrace_symbols_fd(array, size, 2);
  _exit(1);
}

static void install_signal_handler(void)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = signal_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);
}

static void
pybacktrace(unw_cursor_t cursor)
{
  unw_word_t bp;
  PyFrameObject *frame;
  PyObject  *fileobj, *funcobj;
  int depth = 0;

  unw_get_reg(&cursor, UNW_X86_64_RBP, &bp);
  frame = (PyFrameObject*)bp;

  while (frame && (depth < MAX_STACK_DEPTH)) {
    fileobj = PyUnicode_AsUTF8String(frame->f_code->co_filename);
    funcobj = PyUnicode_AsUTF8String(frame->f_code->co_name);
    printf("%s(%d): %s\n",
            fileobj?PyBytes_AsString(fileobj):"???",
            PyFrame_GetLineNumber(frame),
            funcobj?PyBytes_AsString(funcobj):"???");

    if (fileobj) Py_DECREF(fileobj);
    if (funcobj) Py_DECREF(funcobj);
    frame = frame->f_back;
    depth++;
  }
}

static int rw_get_file_and_line(unw_word_t addr,
                                  char *file,
                                  char *program,
                                  size_t flen,
                                  int *line)
{
  static char buf[256];
  char *addr2line = ADDR2LINE;

  // prepare command to be executed
  // our program need to be passed after the -e parameter
  sprintf (buf, "%s -C -e %s -f -i %lx", addr2line,
           program , addr);
  FILE* f = popen (buf, "r");

  if (f == NULL) {
    perror (buf);
    return 0;
  }

  // get function name
  fgets(buf, 256, f);

  // get file and line
  fgets(buf, 256, f);

  if (buf[0] != '?') {
    char *p = buf;

    // file name is until ':'
    while (*p != ':') {
      p++;
    }

    *p++ = 0;
    // after file 2ame follows line number
    strncpy(file, buf, flen);
    sscanf(p, "%d", line);
  } else {
    strncpy(file, "unkown", flen);
    *line = 0;
  }

  pclose(f);

  return 0;
}


int
main(int argc , char **argv)
{
  unw_addr_space_t as;
  unw_cursor_t cursor;
  struct UCD_info *ui;
  unw_word_t ip, sp, off;
  char buf[512], name[256];
  int line;
  int depth = 0;
  int ret;
  bool pybt_done = false;
#define TEST_NAME_LEN 256 

  install_signal_handler();

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <binary> <corefile>", argv[0]);
    exit(1);
  }

  as = unw_create_addr_space(&_UCD_accessors, 0);
  if (!as) {
    fprintf(stderr, "unw_create_addr_space() failed");
    exit(1);
  }

  ui = _UCD_create(argv[2]);
  if (!ui) {
    fprintf(stderr,"_UCD_create('%s') failed", argv[1]);
    exit(1);
  }
  ret = unw_init_remote(&cursor, as, ui);
  if (ret < 0) {
    fprintf(stderr,"unw_init_remote() failed: ret=%d\n", ret);
    exit(1);
  }

  read_elfnotes(argv[2], ui);

  while (unw_step(&cursor) > 0) {
    // Avoid going too deep
    if (depth++ > MAX_STACK_DEPTH) {
      exit(1);
    }
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);


    if (unw_get_proc_name(&cursor, name, sizeof (name), &off) == 0) {
      if (off) {
        snprintf(buf, sizeof (buf), "<%s+0x%lx>", name, (long) off);
      } else {
        snprintf(buf, sizeof (buf), "<%s>", name);
      }
    }

    /* Check for Python backtrace */
    if (!strncmp(name,"PyEval_EvalFrameEx", 18) && !pybt_done) {
      pybacktrace(cursor);
      pybt_done = true;
    }
    rw_get_file_and_line((long)ip, name, argv[1], 256, &line);
    printf("%016lx %s <%s:%d> (sp=%016lx)\n",
            (long) ip, buf, basename(name), line, (long) sp);

  }
  _UCD_destroy(ui);
  unw_destroy_addr_space(as);

  return 0;
}

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
//        fprintf(stderr, "_UCD_add_backing_file_at_vaddr FAILED %" PRId64 " %s\n", start, filenames);
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
