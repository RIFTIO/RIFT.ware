
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <poll.h> 
#include <unistd.h> 
#include <string.h>

#include <confd_lib.h>
#include "confd_cdb.h"
#include "confd_dp.h"

static struct confd_daemon_ctx *dctx;
static int ctlsock;
static int workersock;

typedef struct _foodata {
  char *name;
  struct _foodata *next;
} foodata_t;

typedef struct _opdata {
  foodata_t *foo;
} opdata_t;

opdata_t *g_opdata = NULL;

int process_confd_subscription(int subsock)
{
  int confd_result, flags, length, *subscription_points, i, j, nvalues;
  enum cdb_sub_notification type;
  confd_tag_value_t *values;

  confd_result = cdb_read_subscription_socket2(subsock,
                                               &type,
                                               &flags,
                                               &subscription_points,
                                               &length);

  if (confd_result != CONFD_OK) {
    confd_fatal("Failed to read subscription data \n");
  }

  switch (type) {
    case CDB_SUB_PREPARE:
      for (i = 0; i < length; i++) {
        printf("i = %d, point = %d\n", i, subscription_points[i]);
        if (cdb_get_modifications(subsock, subscription_points[i], flags, &values, &nvalues,
                              "/") == CONFD_OK) {
          for (j = 0; j < nvalues; j++) {
            printf("j = %d\n", j);
            confd_free_value(CONFD_GET_TAG_VALUE(&values[j]));
          }
        }
      }
      cdb_sync_subscription_socket(subsock, CDB_DONE_PRIORITY);
      fprintf(stdout, "CBD_SUB_PREPARE\n");
      break;

    case CDB_SUB_COMMIT:
      cdb_sync_subscription_socket(subsock, CDB_DONE_PRIORITY);
      fprintf(stdout, "CDB_SUB_COMMIT\n");
      break;

    case CDB_SUB_ABORT:
      fprintf(stdout, "CDB_SUB_ABORT\n");
      break;

    default:
      confd_fatal("Invalid type %d in cdb_read_subscription_socket2\n", type);
  }

  return 0;
}

static int do_init_action(struct confd_user_info *uinfo)
{
  int ret = CONFD_OK;
  // fprintf(stdout, "init_action called\n");
  confd_action_set_fd(uinfo, workersock);
  return ret;
}

static int do_rw_action(struct confd_user_info *uinfo,
                        struct xml_tag *name,
                        confd_hkeypath_t *kp,
                        confd_tag_value_t *params,
                        int nparams)
{
  // confd_tag_value_t reply[2];
  // int status;
  // char *ret_status;
  int i;
  char buf[BUFSIZ];

  /* Just print the parameters and return */

  //
  for (i = 0; i < nparams; i++) {
    confd_pp_value(buf, sizeof(buf), CONFD_GET_TAG_VALUE(&params[i]));
    printf("param %2d: %9u:%-9u, %s\n", i, CONFD_GET_TAG_NS(&params[i]),
           CONFD_GET_TAG_TAG(&params[i]), buf);
  }

  i = 0;
  // CONFD_SET_TAG_INT32(&reply[i], NULL, 0); i++;
  // CONFD_SET_TAG_STR(&reply[i], NULL, "success"); i++;
  confd_action_reply_values(uinfo, NULL, i);

  return CONFD_OK;

}

static int get_next(struct confd_trans_ctx *tctx,
                    confd_hkeypath_t *keypath,
                    long next) 
{
  opdata_t *opdata = tctx->t_opaque;
  foodata_t *curr;
  confd_value_t v[2];

  if (next == -1) { /* first call */
    curr = opdata->foo;
  } else {
    curr = (foodata_t *)next;
  }

  if (curr == NULL) {
    confd_data_reply_next_key(tctx, NULL, -1, -1);
    return CONFD_OK;
  }

  CONFD_SET_STR(&v[0], curr->name);
  confd_data_reply_next_key(tctx, &v[0], 1, (long)curr->next);
  return CONFD_OK;
}

static foodata_t *find_foo(confd_hkeypath_t *keypath, opdata_t *dp)
{
  char *name = (char*)CONFD_GET_BUFPTR(&keypath->v[1][0]);
  foodata_t *foo = dp->foo;
  while (foo != NULL) {
    if (strcmp(foo->name, name) == 0) {
      return foo;
    }
    foo = foo->next;
  }
  return NULL;
}

/* Keypath example */
/* /arpentries/arpe{192.168.1.1 eth0}/hwaddr */
/* 3 2 1 0 */
static int get_elem(struct confd_trans_ctx *tctx,
                    confd_hkeypath_t *keypath)
{
  confd_value_t v;
  foodata_t *foo = find_foo(keypath, tctx->t_opaque);
  if (foo == NULL) {
    confd_data_reply_not_found(tctx);
    return CONFD_OK;
  }
  
  CONFD_SET_STR(&v, foo->name);
  confd_data_reply_value(tctx, &v);
  
  return CONFD_OK;
}

static foodata_t *create_dummy_foodata_list(int count)
{
  foodata_t *head, *curr, *prev;
  int i;
  char buf[64];

  head = prev = curr = NULL;
  for (i = 0; i < count; ++i) {
    curr = malloc(sizeof(foodata_t));
    memset(curr, 0, sizeof(foodata_t));
    snprintf(buf, 64, "foo%d", i);
    curr->name = strdup(buf);
    if (prev) {
      prev->next = curr;
    } else {
      head = curr;
    }
    prev = curr;
  }

  return head;
}

static void free_foodata_list(foodata_t *foo)
{
  foodata_t *curr, *next;
  curr = foo;
  while (curr) {
    next = curr->next;
    if (curr->name) {
      free(curr->name);
    }
    free(curr);
    curr = next;
  }
}

static void print_foodata_list(foodata_t *foo) 
{
  foodata_t *curr = foo;
  while (curr) {
    // fprintf(stdout, "%s\n", curr->name);
    curr = curr->next;
  }
}

static int s_init(struct confd_trans_ctx *tctx)
{
  opdata_t *opdata;
  if ((opdata = malloc(sizeof(opdata_t))) == NULL) {
    return CONFD_ERR;
  }

  memset(opdata, 0, sizeof(opdata_t));
  opdata->foo = create_dummy_foodata_list(10);
  print_foodata_list(opdata->foo);
  tctx->t_opaque = opdata;
  confd_trans_set_fd(tctx, workersock);
  return CONFD_OK;
}

static int s_finish(struct confd_trans_ctx *tctx)
{
  opdata_t *opdata = tctx->t_opaque;
  if (opdata != NULL) {
    free_foodata_list(opdata->foo);
    free(opdata);
  }

  return CONFD_OK;
}

int main(int argc, char **argv)
{
  struct sockaddr_in addr;
  int debuglevel = CONFD_TRACE;
  struct confd_trans_cbs trans;
  struct confd_data_cbs data;
  struct confd_action_cbs action;
  int i;

  int subsock, datasock;
  int status;
  int spoint;

  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(CONFD_PORT);

   /**
   * Setup CDB subscription socket 
   */
  confd_init(argv[0], stderr, CONFD_DEBUG);
  if ((subsock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    confd_fatal("Failed to open subscription socket\n");
  }

  printf("Subscription socket: %d\n", subsock);

  for (i = 1; i < 10; ++i) {
    if (cdb_connect(subsock, CDB_SUBSCRIPTION_SOCKET,
                    (struct sockaddr*)&addr,
                    sizeof (struct sockaddr_in)) < 0) {
      sleep(2);
      fprintf(stdout, "Failed in confd_connect() {attempt: %d}\n", i);
    } else {
      fprintf(stdout, "confd_connect succeeded\n");
      break;
    }
  }

  if ((status = cdb_subscribe2(subsock, CDB_SUB_RUNNING_TWOPHASE, 0, 0, &spoint, 0, "/"))
      != CONFD_OK) {
    fprintf(stderr, "Terminate: subscribe %d\n", status);
    exit(1);
  }

  if (cdb_subscribe_done(subsock) != CONFD_OK) {
    confd_fatal("cdb_subscribe_done() failed");
  }

  /**
   * Setup CBD data socket
   */

  if ((datasock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    confd_fatal("Failed to open data socket\n");
  }

  if (cdb_connect(datasock, CDB_DATA_SOCKET,
                  (struct sockaddr*)&addr,
                  sizeof (struct sockaddr_in)) < 0) {
    confd_fatal("Failed to confd_connect() to confd \n");
  }

  memset(&trans, 0, sizeof (struct confd_trans_cbs));
  trans.init = s_init;
  trans.finish = s_finish;

  memset(&data, 0, sizeof (struct confd_data_cbs));
  data.get_elem = get_elem;
  data.get_next = get_next;
  strcpy(data.callpoint, "base_show");

  memset(&action, 0, sizeof (action));
  strcpy(action.actionpoint, "rw_action");
  action.init = do_init_action;
  action.action = do_rw_action;


  /* initialize confd library */
  confd_init("confd_client_op_data_daemon", stderr, debuglevel);


  for (i = 1; i < 10; ++i) {
    if (confd_load_schemas((struct sockaddr*)&addr,
                           sizeof(struct sockaddr_in)) != CONFD_OK) {
      fprintf(stdout, "Failed to load schemas from confd {attempt: %d}\n", i);
      sleep(2);
    } else {
      fprintf(stdout, "confd_load_schemas succeeded\n");
      break;
    }
  }

  if ((dctx = confd_init_daemon("confd_client_op_data_daemon")) == NULL) {
    confd_fatal("Failed to initialize confdlib\n");
  }

  /* Create the first control socket, all requests to */
  /* create new transactions arrive here */
  if ((ctlsock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    confd_fatal("Failed to open ctlsocket\n");
  }

  if (confd_connect(dctx, ctlsock, CONTROL_SOCKET, (struct sockaddr*)&addr,
                    sizeof (struct sockaddr_in)) < 0) {
    confd_fatal("Failed to confd_connect() to confd \n");
  }

  /* Also establish a workersocket, this is the most simple */
  /* case where we have just one ctlsock and one workersock */
  if ((workersock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    confd_fatal("Failed to open workersocket\n");
  }

  if (confd_connect(dctx, workersock, WORKER_SOCKET,(struct sockaddr*)&addr,
                    sizeof (struct sockaddr_in)) < 0) {
    confd_fatal("Failed to confd_connect() to confd \n");
  }

  if (confd_register_trans_cb(dctx, &trans) == CONFD_ERR) {
    confd_fatal("Failed to register trans cb \n");
  }

  if (confd_register_data_cb(dctx, &data) == CONFD_ERR) {
    confd_fatal("Failed to register data cb \n"); 
  }

  if (confd_register_action_cbs(dctx, &action) == CONFD_ERR) {
    confd_fatal("Failed to register action cb \n"); 
  }

  if (confd_register_done(dctx) != CONFD_OK) {
    confd_fatal("Failed to complete registration \n");
  }

  while(1) {
    struct pollfd set[3];
    int ret;
    set[0].fd = ctlsock;
    set[0].events = POLLIN;
    set[0].revents = 0;
    set[1].fd = workersock;
    set[1].events = POLLIN;
    set[1].revents = 0;
    set[2].fd = subsock;
    set[2].events = POLLIN;
    set[2].revents = 0;
    if (poll(set, sizeof(set)/sizeof(*set), -1) < 0) {
      perror("Poll failed:");
      continue;
    }
    /* Check for I/O */
    if (set[0].revents & POLLIN) {
      if ((ret = confd_fd_ready(dctx, ctlsock)) == CONFD_EOF) {
        confd_fatal("Control socket closed\n");
      } else if (ret == CONFD_ERR && confd_errno != CONFD_ERR_EXTERNAL) {
        confd_fatal("Error on control socket request: %s (%d): %s\n",
                    confd_strerror(confd_errno), confd_errno, confd_lasterr());
      }
    }
    if (set[1].revents & POLLIN) {
      if ((ret = confd_fd_ready(dctx, workersock)) == CONFD_EOF) {
        confd_fatal("Worker socket closed\n");
      } else if (ret == CONFD_ERR && confd_errno != CONFD_ERR_EXTERNAL) {
        confd_fatal("Error on worker socket request: %s (%d): %s\n",
                    confd_strerror(confd_errno), confd_errno, confd_lasterr());
      }
    }
    if (set[2].revents & POLLIN) {
      process_confd_subscription(set[2].fd);
    }
  }

  return 0;
}
