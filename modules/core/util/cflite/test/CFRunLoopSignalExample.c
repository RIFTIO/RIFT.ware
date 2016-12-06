#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

// #include <AssertMacros.h>
#define require(a, b)
#define verify(a)
#define verify_action(a, b)

#include <CoreFoundation/CoreFoundation.h>

CFRunLoopSourceRef work_source;
CFRunLoopRef rl;

void handler(int sig) {
  /* Code for the handler */
  printf("INSIDE sighandler\n");

  CFRunLoopSourceSignal(work_source);
  
  CFRunLoopWakeUp(rl);
}


static void RunWorkSource(void *info) {
  printf("INSIDE RUN\n");
}

int main () {
  //CFRunLoopSourceContext source_context;

  CFRunLoopSourceContext source_context = {                                                                                                                                                                                                                                       
                0,
                NULL,
                CFRetain,
                CFRelease,
                (CFStringRef(*)(const void *))CFCopyDescription,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
            };

  CFTreeRef  tree;
  CFTreeContext *ctx, ctxs[10];
  memset(&ctxs, '\0', sizeof(ctxs));

  // Create a Tree
  ctx = &ctxs[0];
  ctx->version = 0;
  ctx->info = (void *) CFSTR("Tree Root");
  ctx->retain = CFRetain;
  ctx->copyDescription = CFCopyDescription;

  tree = CFTreeCreate(kCFAllocatorDefault,
                       ctx);
  //char *c = malloc(100);
  //CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  //source_context.info = this;
  source_context.info = tree;
  source_context.perform = RunWorkSource;
  work_source = CFRunLoopSourceCreate(
      NULL,  // allocator
      1,     // priority
      &source_context);

  //rl = CFRunLoopGetCurrent();
  rl = CFRunLoopGetMain();
  CFRunLoopAddSource(rl, work_source, kCFRunLoopCommonModes);

  if (signal(SIGINT, handler) == SIG_ERR)
    return -1;

  CFRunLoopRun();

#if 0
  //sleep(100);
  for (;;) /* Loop forever, waiting for signals */
    pause(); /* Block until a signal is caught */
#endif

  return(0);
}
