
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <rwnetns.h>

struct namespace {
  char * ns;
  int fd;
};

void * flip_namespaces(void * id) {
  int r;
  size_t n_ns;
  static struct namespace namespaces[2];

  bzero(namespaces, 2 * sizeof(struct namespace));

  r = asprintf(&namespaces[0].ns, "%s-ns-1", (char *)id);
  if (r == -1)
    goto done;

  r = asprintf(&namespaces[1].ns, "%s-ns-2", (char *)id);
  if (r == -1)
    goto done;


  n_ns = sizeof(namespaces) / sizeof(struct namespace);

  for (size_t i = 0; i < n_ns; ++i) {
    r = rwnetns_create_context(namespaces[i].ns);
    if (r) {
      fprintf(stderr, "rwnetns_create_context(%s) failed %d\n", namespaces[i].ns, r);
      goto done;
    }

    namespaces[i].fd = rwnetns_get_netfd(namespaces[i].ns);
    if (r) {
      fprintf(stderr, "rwnetns_get_netfd(%s) failed %d\n", namespaces[i].ns, r);
      goto done;
    }
  }

  for (size_t i = 0; i < 100000; ++i) {
    r = rwnetns_change_fd(namespaces[i % n_ns].fd);
    if (r) {
      fprintf(stderr, "rwnetns_change_fd(%s) failed %d\n", namespaces[i % n_ns].ns, r);
      goto done;
    }
  }

done:
  if (namespaces[0].ns)
    free(namespaces[0].ns);

  if (namespaces[1].ns)
    free(namespaces[1].ns);

  for (size_t i = 0; i < n_ns; ++i)
    rwnetns_delete_context(namespaces[i].ns);

  pthread_exit(NULL);
}


int main() {
  int r;
  pthread_t threads[2];
  char * names[2] = {"thread1", "thread2"};

  r = pthread_create(&threads[0], NULL, flip_namespaces, (void *)names[0]);
  if (r) {
    fprintf(stderr, "Failed to create thread1\n");
    return 1;
  }

  /*
  r = pthread_create(&threads[1], NULL, flip_namespaces, (void *)names[1]);
  if (r) {
    fprintf(stderr, "Failed to create thread1\n");
    return 1;
  }
*/
  pthread_exit(NULL);
  return 0;
}


