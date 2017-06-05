/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_resource_track.h
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 02/13/2014
 * @brief Memory-Resource Annotation macros
 * @details RIFT Memory-Resource Annotation macros
 */
#ifndef __RW_RESOURCE_ANNOTATE_H__ 
#define __RW_RESOURCE_ANNOTATE_H__

#include <stdio.h>
#include <assert.h>
#include <pthread.h>

__BEGIN_DECLS

#include <uthash.h>
#include <utlist.h>

# pragma pack (1)
typedef struct _addr_nd_annot_t_ {
    void                      *_addr;
    void                      *ph;
    const char                *_loc;
    const char                *_annotation;
    struct _addr_nd_annot_t_  *next;
    UT_hash_handle            hh;
} addr_nd_annot_t;
# pragma pack ()

#define NUM_BLOCKS (4*1024*16/sizeof(addr_nd_annot_t))

extern addr_nd_annot_t *a_a_a_h;
extern addr_nd_annot_t *a_a_a_l;
extern pthread_mutex_t res_annot_mutex;

#ifndef __rw_location__
#define __RW_STRING_LINE1__(s)    #s
#define __RW_STRING_LINE2__(s)   __RW_STRING_LINE1__(s)
#define __RW_STRING_LINE3__  __RW_STRING_LINE2__(__LINE__)
#define __rw_location__ __FILE__ ":" __RW_STRING_LINE3__
#endif


#define RW_RESOURCE_ANNOTATION_ADD(addr, annotation, multi_threaded)    \
/*void RW_RESOURCE_ANNOTATION_ADD(void *addr, const char *annotation)*/ \
{                                                                       \
  unsigned int __i;                                                     \
  void *__key = (addr);                                                 \
  void *__ph;                                                           \
  addr_nd_annot_t *__p;                                                 \
  if (multi_threaded) pthread_mutex_lock(&res_annot_mutex);             \
  assert(__key != NULL);                                                \
  HASH_FIND_PTR(a_a_a_h, &__key, __p);                                  \
  /*printf("p=%p _addr=%p annot=%s\n", __p, __key, annotation);*/       \
  if (__p == NULL) {                                                    \
    if (a_a_a_l == NULL) {                                              \
      __ph = malloc(sizeof(*__p)*NUM_BLOCKS + sizeof(int));             \
      assert(__ph != NULL);                                             \
      __p = __ph + sizeof(int);                                         \
      *((int*)__ph) = NUM_BLOCKS-1;                                     \
      for (__i = 0; __i<NUM_BLOCKS-1; __i++) {                          \
        LL_APPEND(a_a_a_l, __p);                                        \
        __p->ph = __ph;                                                 \
        __p++;                                                          \
      }                                                                 \
      __p->ph = __ph;                                                   \
    }                                                                   \
    else {                                                              \
      __p = a_a_a_l;                                                    \
      *((int*)(__p->ph)) -= 1;                                          \
      LL_DELETE(a_a_a_l, __p);                                          \
    }                                                                   \
    __p->_addr = __key;                                                 \
    HASH_ADD_PTR(a_a_a_h, _addr, __p);                                  \
  }                                                                     \
  __p->_annotation = (annotation);                                      \
  __p->_loc = __rw_location__;                                          \
  if (multi_threaded) pthread_mutex_unlock(&res_annot_mutex);           \
}

#define RW_RESOURCE_ANNOTATION_DEL(addr, multi_threaded)                \
/*void RW_RESOURCE_ANNOTATION_DEL(void *addr)*/                         \
{                                                                       \
  unsigned int __i;                                                     \
  void *__key;                                                          \
  void *__ph;                                                           \
  addr_nd_annot_t *__p;                                                 \
  if (multi_threaded) pthread_mutex_lock(&res_annot_mutex);             \
  __key = (addr);                                                       \
  HASH_FIND(hh, a_a_a_h, &__key, sizeof(void *), __p);                  \
  if (__p != NULL) {                                                    \
    __ph = __p->ph;                                                     \
    assert(__ph != NULL);                                               \
    HASH_DEL(a_a_a_h, __p);                                             \
    LL_APPEND(a_a_a_l, __p);                                            \
    *((int*)__ph) += 1;                                                 \
    if (*((int*)__ph) == NUM_BLOCKS) {                                  \
      __p = __ph + sizeof(int);                                         \
      for (__i = 0; __i<NUM_BLOCKS; __i++) {                            \
        assert(__p->ph == __ph);                                        \
        LL_DELETE(a_a_a_l, __p);                                        \
        __p++;                                                          \
      }                                                                 \
      free(__ph);                                                       \
    }                                                                   \
  }                                                                     \
  if (multi_threaded) pthread_mutex_unlock(&res_annot_mutex);           \
}

#define RW_QUOTE(name) #name

#define RW_RESOURCE_ANNOTATION_ADD_TYPE(addr, type, multi_threaded)     \
      (RW_RESOURCE_ANNOTATION_ADD(addr, RW_QUOTE(type), multi_threaded))

#define RW_RESOURCE_ANNOTATION_DUMP(multi_threaded)                     \
{                                                                       \
  addr_nd_annot_t *p=NULL, *tmp;                                        \
  int count;                                                            \
  if (multi_threaded) pthread_mutex_lock(&res_annot_mutex);             \
  HASH_ITER(hh, a_a_a_h, p, tmp) {                                      \
    printf("0x%-9p ANNOT:%s FROM %s\n",                                 \
            p->_addr,                                                   \
            (p->_annotation ?                                           \
             p->_annotation :                                           \
             "null"),                                                   \
            p->_loc                                                     \
           );                                                           \
  }                                                                     \
  printf("hC=%u\n", HASH_COUNT(a_a_a_h));                               \
  LL_COUNT(a_a_a_l, p, count);                                          \
  printf("lC=%u\n\n", count);                                           \
  if (multi_threaded) pthread_mutex_unlock(&res_annot_mutex);           \
}

__END_DECLS

#endif /* __RW_RESOURCE_ANNOTATE_H__ */
