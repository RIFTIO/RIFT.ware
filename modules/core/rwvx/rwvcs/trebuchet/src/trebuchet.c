
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <curl/curl.h>
#include <json.h>
#include <json_tokener.h>

#include "trebuchet.h"

struct trebuchet_client {
  CURL * curl;
  char host[HOST_NAME_MAX];
  int port;

  SLIST_ENTRY(trebuchet_client) slist;
};

/*
 * Create a client connection to the specified trebuchet server.
 *
 * @param trebuchet - trebuchet client instance
 * @param host      - remote server hostname
 * @param port      - port trebuchet server is listening on
 * @return          - 0 on success, errno otherwise
 */
static struct trebuchet_client * trebuchet_client_alloc(struct trebuchet * trebuchet, const char * host, int port) {
  int r;
  struct trebuchet_client * cli;

  cli = (struct trebuchet_client *)malloc(sizeof(struct trebuchet_client));
  if (!cli)
    return NULL;

  r = snprintf(cli->host, HOST_NAME_MAX, host);
  if (r >= HOST_NAME_MAX) {
    free(cli);
    return NULL;
  }

  cli->port = port;
  cli->curl = curl_easy_init();

  return cli;
}

/*
 * Free a trebuchet client connection.
 *
 * @param trebuchet - trebuchet client instance
 * @param cli       - trebuchet client connection
 */
static void trebuchet_client_free(struct trebuchet * trebuchet, struct trebuchet_client * cli) {
  SLIST_REMOVE(&trebuchet->clients, cli, trebuchet_client, slist);
  curl_easy_cleanup(cli->curl);
  free(cli);
}


/*
 * Search for a client connection by host and port
 *
 * @param trebuchet - trebuchet client instance
 * @param host      - remote server hostname
 * @param port      - port trebuchet server listens on
 * @return          - on success, pointer to a trebuchet client connection, NULL otherwise
 */
static struct trebuchet_client * trebuchet_find_client(struct trebuchet * trebuchet, const char * host, int port) {
  struct trebuchet_client * cli;

  SLIST_FOREACH(cli, &trebuchet->clients, slist) {
    if (!strcmp(cli->host, host) && cli->port == port)
      return cli;
  }

  return NULL;
}

/*
 * Handle response to a PING request.  Expected response is a json object
 *  {'pong': <current time as a double>}
 *
 * @param ud  - pointer to a double which will be filled with the pong timestamp
 *              on success.
 */
static size_t on_ping_response(char * data, size_t size, size_t n_memb, void * ud) {
  size_t ret = 0;
  double * timestamp;
  json_object * jobj = NULL;
  json_object * tmp;

  jobj = json_tokener_parse(data);
  if (!jobj)
    goto done;

  tmp = json_object_object_get(jobj, "pong");
  if (!tmp)
    goto done;

  timestamp = (double *)ud;
  *timestamp = json_object_get_double(tmp);
  ret = size * n_memb;

done:
  if (jobj)
    json_object_put(jobj);

  return ret;
}

/*
 * Handle response to a LAUNCH request.  Expected response is a json object
 * {
 *  'ret':    0 on success or errno
 *  'errmsg': on error, more verbose error message
 *  'log':    on error, stdout/stderr from launch attempt
 * }
 *
 * @param ud  - pointer to a struct trebuchet_launch_response which will be
 *              filled on success.
 */
static size_t on_launch_response(char * data, size_t size, size_t n_memb, void * ud) {
  size_t ret = 0;
  struct trebuchet_launch_resp * resp;
  json_object * jobj = NULL;
  json_object * tmp;
  char * errmsg = NULL;
  char * log = NULL;

  resp = (struct trebuchet_launch_resp *)ud;
  bzero(resp, sizeof(struct trebuchet_launch_resp));

  jobj = json_tokener_parse(data);
  if (!jobj)
    goto err;

  /* Parse 'ret' */
  tmp = json_object_object_get(jobj, "ret");
  if (!tmp)
    goto err;

  if (json_object_get_type(tmp) != json_type_int)
    goto err;

  errno = 0;
  resp->ret = json_object_get_int(tmp);
  if (resp->ret == 0) {
    if (errno != EINVAL)
      ret = size * n_memb;
    goto done;
  }

  /* Parse 'errmsg' */
  tmp = json_object_object_get(jobj, "errmsg");
  if (!tmp)
    goto err;

  if (json_object_get_type(tmp) != json_type_string)
    goto err;

  errmsg = strdup(json_object_get_string(tmp));
  if (!errmsg)
    goto err;

  /* Parse 'log' */
  tmp = json_object_object_get(jobj, "log");
  if (!tmp)
    goto err;

  if (json_object_get_type(tmp) != json_type_string)
    goto err;

  log = strdup(json_object_get_string(tmp));
  if (!log)
    goto err;

  resp->errmsg = errmsg;
  resp->log = log;
  ret = size * n_memb;
  goto done;

err:
  if (errmsg)
    free(errmsg);

  if (log)
    free(log);

done:
  if (jobj)
    json_object_put(jobj);

  return ret;
}

/*
 * Handle response to a TERMINATE request.  Expected response is a json object
 *  {'status': 0 or errno}
 *
 * @param ud  - pointer to an int which will be filled with the status response
 *              on success.
 */
static size_t on_terminate_response(char * data, size_t size, size_t n_memb, void * ud) {
  size_t ret = 0;
  int * status;
  json_object * jobj = NULL;
  json_object * tmp;

  jobj = json_tokener_parse(data);
  if (!jobj)
    goto done;

  tmp = json_object_object_get(jobj, "status");
  if (!tmp)
    goto done;

  status = (int *)ud;
  *status = json_object_get_int(tmp);
  ret = size * n_memb;

done:
  if (jobj)
    json_object_put(jobj);

  return ret;
}


/*
 * Handle response to a STATUS request.  Expected response is a json object
 *  {'status': <status as a string>}
 *
 * @param ud  - pointer to a trebuchet_vm_status which will be filled with the
 *              status on success.
 */
static size_t on_status_response(char * data, size_t size, size_t n_memb, void * ud) {
  size_t ret = 0;
  trebuchet_vm_status * status;
  json_object * jobj = NULL;
  json_object * tmp;
  const char * cstatus = NULL;

  jobj = json_tokener_parse(data);
  if (!jobj)
    goto done;

  tmp = json_object_object_get(jobj, "status");
  if (!tmp)
    goto done;

  status = (trebuchet_vm_status *)ud;

  if (json_object_get_type(tmp) != json_type_string)
    goto done;

  ret = size * n_memb;
  cstatus = json_object_get_string(tmp);
  if (!strcmp(cstatus, "IDLE"))
    *status = TREBUCHET_VM_IDLE;
  else if (!strcmp(cstatus, "RUNNING"))
    *status = TREBUCHET_VM_RUNNING;
  else if (!strcmp(cstatus, "CRASHED"))
    *status = TREBUCHET_VM_CRASHED;
  else if (!strcmp(cstatus, "NOTFOUND"))
    *status = TREBUCHET_VM_NOTFOUND;
  else
    ret = 0;

done:
  if (jobj)
    json_object_put(jobj);

  return ret;
}


void * trebuchet_launch_req_alloc(
    const char * manifest_path,
    const char * component_name,
    uint32_t instance_id,
    const char * parent_id,
    const char * ip_address,
    int64_t uid) {
  int r;
  json_object * req = NULL;
  json_object * tmp;
  FILE * fp = NULL;
  long fsz;
  char * fbuf;

  req = json_object_new_object();
  if (!req)
    goto err;

  /* Read the bootstrap manifest file and add the data to the request */
  fp = fopen(manifest_path, "r");
  if (!fp) {
    goto err;
  }

  r = fseek(fp, 0L, SEEK_END);
  if (r != 0) {
    assert(0);
    goto err;
  }

  fsz = ftell(fp);
  rewind(fp);

  fbuf = (char *)malloc(fsz * sizeof(char));
  if (!fbuf) {
    assert(0);
    goto err;
  }

  r = fread(fbuf, sizeof(char), fsz, fp);
  if (r != fsz) {
    assert(0);
    goto err;
  }
  fclose(fp);
  fp = NULL;

  tmp = json_object_new_string_len(fbuf, fsz);
  if (!tmp) {
    assert(0);
    goto err;
  }

  json_object_object_add(req, "manifest_data", tmp);

  /* Add the rest of the request values, these are just simple strings and ints */
  tmp = json_object_new_string(component_name);
  if (!tmp) {
    assert(0);
    goto err;
  }
  json_object_object_add(req, "component_name", tmp);

  tmp = json_object_new_int64(instance_id);
  if (!tmp) {
    assert(0);
    goto err;
  }
  json_object_object_add(req, "instance_id", tmp);

  tmp = json_object_new_string(parent_id);
  if (!tmp) {
    assert(0);
    goto err;
  }
  json_object_object_add(req, "parent_id", tmp);

  tmp = json_object_new_string(ip_address);
  if (!tmp) {
    assert(0);
    goto err;
  }
  json_object_object_add(req, "ip_address", tmp);

  tmp = json_object_new_int64(uid);
  if (!tmp) {
    assert(0);
    goto err;
  }
  json_object_object_add(req, "uid", tmp);

  return (void *)req;

err:
  if (req)
    json_object_put(req);

  if (fp)
    fclose(fp);

  return NULL;
}

void trebuchet_launch_req_free(void * req) {
  if (req)
    json_object_put((json_object *)req);
}

void trebuchet_launch_resp_free(struct trebuchet_launch_resp * resp) {
  if (resp) {
    if (resp->errmsg)
      free(resp->errmsg);

    if (resp->log)
      free(resp->log);

    free(resp);
  }
}

void trebuchet_init() {
  curl_global_init(CURL_GLOBAL_ALL);
}

struct trebuchet * trebuchet_alloc() {
  struct trebuchet * trebuchet;

  trebuchet = (struct trebuchet *)malloc(sizeof(struct trebuchet));
  if (!trebuchet)
    return NULL;

  SLIST_INIT(&trebuchet->clients);

  return trebuchet;
}

void trebuchet_free(struct trebuchet * trebuchet) {
  struct trebuchet_client * cli;
  struct trebuchet_client * cli_safe;

  SLIST_FOREACH_SAFE(cli, &trebuchet->clients, slist, cli_safe)
    trebuchet_client_free(trebuchet, cli);

  free(trebuchet);
}

int trebuchet_connect(struct trebuchet * trebuchet, const char * host, int port) {
  struct trebuchet_client * cli;

  cli = trebuchet_find_client(trebuchet, host, port);
  if (!cli) {
    cli = trebuchet_client_alloc(trebuchet, host, port);
    if (!cli)
      return ENOMEM;

    SLIST_INSERT_HEAD(&trebuchet->clients, cli, slist);
  }

  return 0;
}

int trebuchet_ping(struct trebuchet * trebuchet, const char * host, int port, double * ret) {
  int r;
  CURLcode res;
  struct trebuchet_client * cli;
  char * path = NULL;
  static char error[CURL_ERROR_SIZE];


  cli = trebuchet_find_client(trebuchet, host, port);
  if (!cli)
    return ENOENT;

  r = asprintf(&path, "%s/vcs/1.0/ping", host);
  if (r == -1)
    return ENOMEM;

  curl_easy_setopt(cli->curl, CURLOPT_URL, path);
  curl_easy_setopt(cli->curl, CURLOPT_PORT, cli->port);
  curl_easy_setopt(cli->curl, CURLOPT_WRITEFUNCTION, on_ping_response);
  curl_easy_setopt(cli->curl, CURLOPT_WRITEDATA, ret);
  curl_easy_setopt(cli->curl, CURLOPT_ERRORBUFFER, error);

  res = curl_easy_perform(cli->curl);
  free(path);
  curl_easy_reset(cli->curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "%s\n", error);
    return EIO;
  }

  return 0;
}

int trebuchet_launch(
    struct trebuchet * trebuchet,
    const char * host,
    int port,
    void * req,
    struct trebuchet_launch_resp * ret) {
  int r;
  CURLcode res;
  struct trebuchet_client * cli;
  char * path = NULL;
  struct curl_slist * headers = NULL;
  static char error[CURL_ERROR_SIZE];
  struct json_object * jreq;

  cli = trebuchet_find_client(trebuchet, host, port);
  if (!cli)
    return ENOENT;

  r = asprintf(&path, "%s/vcs/1.0/vms/launch", host);
  if (r == -1)
    return ENOMEM;

  jreq = (json_object *)req;

  headers = curl_slist_append(headers, "Content-type: application/json");

  curl_easy_setopt(cli->curl, CURLOPT_POST, 1);
  curl_easy_setopt(cli->curl, CURLOPT_POSTFIELDS, json_object_to_json_string(jreq));
  curl_easy_setopt(cli->curl, CURLOPT_HTTPHEADER, headers);

  curl_easy_setopt(cli->curl, CURLOPT_URL, path);
  curl_easy_setopt(cli->curl, CURLOPT_PORT, cli->port);
  curl_easy_setopt(cli->curl, CURLOPT_WRITEFUNCTION, on_launch_response);
  curl_easy_setopt(cli->curl, CURLOPT_WRITEDATA, ret);
  curl_easy_setopt(cli->curl, CURLOPT_ERRORBUFFER, error);

  res = curl_easy_perform(cli->curl);
  free(path);
  curl_easy_reset(cli->curl);
  curl_slist_free_all(headers);

  if (res != CURLE_OK) {
    fprintf(stderr, "%s\n", error);
    return EIO;
  }

  return ret->ret == 0 ? 0 : EIO;
}

int trebuchet_terminate(
    struct trebuchet * trebuchet,
    const char * host,
    int port,
    const char * component_name,
    uint32_t instance_id,
    int * ret) {
  int r;
  CURLcode res;
  struct trebuchet_client * cli;
  char * path = NULL;
  static char error[CURL_ERROR_SIZE];


  cli = trebuchet_find_client(trebuchet, host, port);
  if (!cli)
    return ENOENT;

  r = asprintf(&path, "%s/vcs/1.0/vms/%s-%u/terminate", host, component_name, instance_id);
  if (r == -1)
    return ENOMEM;

  curl_easy_setopt(cli->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  curl_easy_setopt(cli->curl, CURLOPT_URL, path);
  curl_easy_setopt(cli->curl, CURLOPT_PORT, cli->port);
  curl_easy_setopt(cli->curl, CURLOPT_WRITEFUNCTION, on_terminate_response);
  curl_easy_setopt(cli->curl, CURLOPT_WRITEDATA, ret);
  curl_easy_setopt(cli->curl, CURLOPT_ERRORBUFFER, error);

  res = curl_easy_perform(cli->curl);
  free(path);
  curl_easy_reset(cli->curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "%s\n", error);
    return EIO;
  }

  return 0;
}

int trebuchet_get_status(
    struct trebuchet * trebuchet,
    const char * host,
    int port,
    const char * component_name,
    uint32_t instance_id,
    trebuchet_vm_status * status) {
  int r;
  CURLcode res;
  struct trebuchet_client * cli;
  char * path = NULL;
  static char error[CURL_ERROR_SIZE];


  cli = trebuchet_find_client(trebuchet, host, port);
  if (!cli)
    return ENOENT;

  r = asprintf(&path, "%s/vcs/1.0/vms/%s-%u/status", host, component_name, instance_id);
  if (r == -1)
    return ENOMEM;

  curl_easy_setopt(cli->curl, CURLOPT_URL, path);
  curl_easy_setopt(cli->curl, CURLOPT_PORT, cli->port);
  curl_easy_setopt(cli->curl, CURLOPT_WRITEFUNCTION, on_status_response);
  curl_easy_setopt(cli->curl, CURLOPT_WRITEDATA, status);
  curl_easy_setopt(cli->curl, CURLOPT_ERRORBUFFER, error);

  res = curl_easy_perform(cli->curl);
  free(path);
  curl_easy_reset(cli->curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "%s\n", error);
    return EIO;
  }

  return 0;
}


// vim: sw=2
