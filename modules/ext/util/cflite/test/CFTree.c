#include <CoreFoundation/CoreFoundation.h>

void my_CFRelease(const void *info) {
  CFRelease(info);
}

void my_CFTreeTraverse(CFTreeRef tree) {
  if (tree == NULL) return;
  CFShow((CFTypeRef) tree);
  printf("\n");
  my_CFTreeTraverse(CFTreeGetFirstChild(tree));
  my_CFTreeTraverse(CFTreeGetNextSibling(tree));
}

void my_CFTreeRelease(CFTreeRef tree) {
  if (tree == NULL) return;
  my_CFTreeRelease(CFTreeGetFirstChild(tree));
  my_CFTreeRelease(CFTreeGetNextSibling(tree));

  //CFTreeRemoveAllChildren( tree );
  CFTreeRemove( tree );

  CFRelease(tree);
}

void main () {
    CFTreeRef  tree;
    CFTreeRef  sib, sibout;
    CFTreeContext *ctx, ctxs[10];
    char name[100];
    const void *ret_val = NULL;

    // Create a Tree
    ctx = &ctxs[0];
    ctx->version = 0;
    ctx->info = (void *) CFSTR("Tree Root");
    ctx->retain = CFRetain;
    ctx->release = my_CFRelease;
    ctx->copyDescription = CFCopyDescription;

    tree = CFTreeCreate( kCFAllocatorDefault,
                         ctx);
    // Create a Tree
    ctx = &ctxs[1];
    ctx->version = 0;
    ctx->info = (void *) CFSTR("Marge");
    ctx->retain = CFRetain;
    ctx->release = my_CFRelease;
    ctx->copyDescription = CFCopyDescription;

    sib = CFTreeCreate( kCFAllocatorDefault,
                         ctx);
    CFTreePrependChild(tree, sib);

    // Create a Tree
    ctx = &ctxs[2];
    ctx->version = 0;
    ctx->info = (void *) CFSTR("Homer");
    ctx->retain = CFRetain;
    ctx->release = my_CFRelease;
    ctx->copyDescription = CFCopyDescription;

    sib = CFTreeCreate( kCFAllocatorDefault,
                         ctx);
    CFTreePrependChild(tree, sib);

    sibout = CFTreeGetFirstChild(tree);

    // Create a Tree
    ctx = &ctxs[3];
    ctx->version = 0;
    ctx->info = (void *) CFSTR("Bart");
    ctx->retain = CFRetain;
    ctx->release = my_CFRelease;
    ctx->copyDescription = CFCopyDescription;

    sib = CFTreeCreate(kCFAllocatorDefault,
                       ctx);
    CFTreePrependChild(sibout, sib);



    my_CFTreeTraverse(tree);

    my_CFTreeRelease(tree);

    /*
    printf("Should Fail\n");
    my_CFTreeTraverse(sibout);

    printf("Should Fail\n");
    my_CFTreeTraverse(sib);

    printf("Should Fail\n");
    my_CFTreeTraverse(tree);
    */
}
