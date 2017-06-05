/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_btrace.c
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 07/20/2013
 * @brief RIFT utilities for backtrace. 
 * 
 * @details These utilities are written on top of libunwind.
 *
 */

#define _GNU_SOURCE 1        /* See feature_test_macros(7) */
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <execinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include "rwlib.h"

#include "Python.h"
#include "frameobject.h"
#include "python2.7/stringobject.h"
#include "unicodeobject.h"


extern char *program_invocation_name;

#define ADDR2LINE "/usr/bin/addr2line"

#define MAX_STACK_DEPTH 128  

/**
 * This function gets the file name and line number of an instruction
 * using add2line utility.
 *
 * @param[in] addr - address of the instruction
 * @param[out] file - name of the file
 * @param[out] flen - length of file name
 * @param[out] line - line number
 *
 * @returns 0  if success
 * @returns -1 if there is an error
 */
static int rw_get_file_and_line(unw_word_t addr,
                                  char *file,
                                  size_t flen,
                                  int *line)
{
  static char buf[256];
  char *addr2line = ADDR2LINE;
  
  // prepare command to be executed
  // our program need to be passed after the -e parameter
  sprintf (buf, "%s -C -e %s -f -i %lx", addr2line,
           program_invocation_name, addr);
  FILE* f = popen (buf, "r");
  
  if (f == NULL) {
    perror (buf);
    return 0;
  }
  
  // get function name
  if (fgets(buf, 256, f) == NULL )  { 
      pclose(f);
      return 0;
  }
      

  // get file and line
  if (fgets(buf, 256, f) == NULL )  { 
      pclose(f);
      return 0;
  }
  
  if (buf[0] != '?') {
    char *p = buf;
    
    // file name is until ':'
    while (*p != ':') {
      p++;
    }
    
    *p++ = 0;
    // after file name follows line number
    strncpy(file, buf, flen);
    sscanf(p, "%d", line);
  } else {
    strncpy(file, "unkown", flen);
    *line = 0;
  }
  
  pclose(f);

  return 0;
}

/**
 * This function fork a child process and invokes gdb to get the stack trace
 *
 * @returns void
 */
 void rw_show_gdb_trace(void) {
  char pid_buf[30];
  sprintf(pid_buf, "%d", getpid());
  char name_buf[512];
  name_buf[readlink("/proc/self/exe", name_buf, 511)]=0;/* readlink wont return -1 ?? */
  int child_pid = fork();
  if (!child_pid) {
    dup2(2,1); // redirect output to stderr
    fprintf(stdout, "stack trace for %s pid=%s\n", name_buf, pid_buf);
    execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", 
           "-ex", "bt", name_buf, pid_buf, NULL);
    abort(); /* If gdb failed to start */
  } else {
    waitpid(child_pid,NULL,0);
  }
}

/**
 * This function prints the python backtrace 
 *
 * @param[in] cursor - libunwind cursor  
 * @param[in] out    - FILEP where to send output 
 * @returns void
 *
 */
void
rw_unw_show_pybacktrace(unw_cursor_t cursor, FILE* out)
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
    fprintf(out, "%s(%d): %s\n",
            fileobj?PyBytes_AsString(fileobj):"???",
            PyFrame_GetLineNumber(frame),
            funcobj?PyBytes_AsString(funcobj):"???");

    if (fileobj) Py_DECREF(fileobj);
    if (funcobj) Py_DECREF(funcobj);
    frame = frame->f_back;
    depth++;
  }
}

/**
 * This function uses libuunwind to get the backtrace
 *
 * @param[in] out - FILEP where to send output
 *
 * @returns void
 */
void rw_show_unw_backtrace (FILE *out) {
  unw_cursor_t cursor; unw_context_t uc;
  unw_word_t ip, sp, off;
  char buf[512], name[256];
  // unw_proc_info_t pi;
  int line;
  int depth = 0;
  bool pybt_done = FALSE;

  UNUSED(out);

  fprintf(out, "libunwind backtrace:\n");

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    // Avoid going too deep
    if (depth++ > MAX_STACK_DEPTH) {
      return;
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
      rw_unw_show_pybacktrace(cursor, out);
      pybt_done = TRUE;
    }
    rw_get_file_and_line((long)ip, name, 256, &line);
    fprintf(out, "%016lx %s <%s:%d> (sp=%016lx)\n", 
            (long) ip, buf, basename(name), line, (long) sp);

#if 0
    if(unw_get_proc_info (&cursor, &pi) == 0) {
      fprintf(out,"\tproc=0x%lx-0x%lx\n\thandler=0x%lx lsda=0x%lx gp=0x%lx",
	      (long) pi.start_ip, (long) pi.end_ip,
	      (long) pi.handler, (long) pi.lsda, (long) pi.gp);
    }
    fprintf(out,"\n");
#endif /* 0 */
  }
}

/**
 * This function gets the backtrace using GNU backtrace
 *
 * @param[in] out - FILEP where to send output
 *
 * @returns void
 */
void rw_show_backtrace(FILE *out)
{
  void *array[MAX_STACK_DEPTH];
  size_t size;
  char **strings;
  size_t i;

  fprintf(out, "backtrace: \n");

  size = backtrace(array, MAX_STACK_DEPTH);
  strings = backtrace_symbols(array, size);

  if (strings == NULL) {
    return;
  }

  for (i = 0; i < size; i++) {
    fprintf(out, "[%p]%s\n", array[i], strings[i]);
  }

  free(strings);
}

/**
 * This function gets the address of 'depth' levels of caller stack
 *
 * @param[out] array - addresses of the caller's addresses in the stack
 * @param[in] depth -  how deep the stack is requested
 *
 * @returns how many addresses in the array
 */
int rw_btrace_backtrace(void **array, int depth)
{
  return backtrace(array, depth);
}

/**
 * This function gets the function-name for an address
 *
 * @param[in] addr - address to be decoded
 *
 * @returns char* - this memory need to be freed by the caller
 */
char* rw_btrace_get_proc_name(void *addr)
{
  void *array[MAX_STACK_DEPTH];
  char **strings;
  char *ret = NULL;

  array[0] = addr;
  strings = backtrace_symbols(array, 1);

  if (strings == NULL) {
    return NULL;
  }

  ret = strdup(strings[0]);
  free(strings);

  return ret;
}

typedef struct my_unw_addr_space {
  struct unw_accessors acc;
} *my_unw_addr_space_t;

/**
 * This function gets the function-name for an address using libunwind APIs
 *
 * @param[in] addr - address to be decoded
 *
 * @returns char* - this memory need to be freed by the caller
 */
char* rw_unw_get_proc_name(void *addr)
{
  unw_cursor_t cursor; unw_context_t uc;
  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  unw_word_t offset;
  char fname[999];
  char *ret = NULL;

  my_unw_addr_space_t as = (my_unw_addr_space_t)unw_local_addr_space;
  (as->acc.get_proc_name)(
      unw_local_addr_space,
      (unw_word_t)addr,
      fname, sizeof(fname), &offset, NULL);
  int r = asprintf(&ret, "%s+0x%lx", fname, offset);
  RW_ASSERT(r != -1);
  //ret = strdup(fname);

  return ret;
}
