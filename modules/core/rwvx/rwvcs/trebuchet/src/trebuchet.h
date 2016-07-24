
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __TREBUCHET_H__
#define __TREBUCHET_H__

#include <sys/queue.h>

#include <curl/curl.h>

__BEGIN_DECLS

/*
 * Client library for the Trebuchet task launcher.  Trebuchet is responsible
 * for running and monitoring rwmain instances on physical virtual machines.
 */

struct trebuchet_client;

struct trebuchet {
  SLIST_HEAD(_client_list, trebuchet_client) clients;
};

struct trebuchet_launch_resp {
    int ret;
    char * errmsg;
    char * log;
};

typedef enum {
  TREBUCHET_VM_IDLE = 0,
  TREBUCHET_VM_RUNNING,
  TREBUCHET_VM_CRASHED,
  TREBUCHET_VM_NOTFOUND,
} trebuchet_vm_status;

/*
 * Create a request to launch a Rift.VM
 *
 * @param manifest_path   - local path to the bootstrap manifest
 * @param component_name  - vm component name
 * @param instance_id     - vm component instance
 * @param parent_id       - parent instance
 * @param ip_address      - vm ip-address
 * @param uid             - if non-zero, the uid that will launch the Rift.VM
 *                          if enabled on the remote trebuchet server
 * @return                - allocated launch request or NULL on error
 */
void * trebuchet_launch_req_alloc(
    const char * manifest_path,
    const char * component_name,
    uint32_t instance_id,
    const char * parent_id,
    const char * ip_address,
    int64_t uid);

/*
 * Free a launch request
 *
 * @param req - launch request to free
 */
void trebuchet_launch_req_free(void * req);

/*
 * Free a launch response
 *
 * @param resp  - launch response to free
 */
void trebuchet_launch_resp_free(struct trebuchet_launch_resp * resp);

/*
 * Initialize the trebuchet library.  This must be called before any other
 * trebuchet methods.  It is ok if this is called multiple times as successive
 * calls will be null operations.  However, in a multi-threaded environment,
 * multiple trebuchet_init calls must not be executed at the same time.
 */
void trebuchet_init();

/*
 * Create a trebuchet client instance
 *
 * @return  - trebuchet client instance
 */
struct trebuchet * trebuchet_alloc();

/*
 * Free a trebuchet client instance.  This will close any connections made by
 * the client.
 *
 * @param trebuchet - trebuchet client instance
 */
void trebuchet_free(struct trebuchet * trebuchet);

/*
 * Connect to a remote trebuchet server.
 *
 * @param trebuchet - trebuchet client instance
 * @param host      - remote server hostname
 * @param port      - port trebuchet server is listening on
 * @return          - 0 on success, errno otherwise.
 */
int trebuchet_connect(struct trebuchet * trebuchet, const char * host, int port);

/*
 * Ping a remote trebuchet server.  If not already connected, a connection will
 * be created.
 *
 * @param trebuchet - trebuchet client instance
 * @param host      - remote server hostname
 * @param port      - port trebuchet server is listening on
 * @param ret       - on success, response to ping from remote server
 * @return          - 0 on success, errno otherwise.
 */
int trebuchet_ping(struct trebuchet * trebuchet, const char * host, int port, double * ret);

/*
 * Launch a Rift.VM on a remote trebuchet server.  If not already connected a
 * connection will be created.
 *
 * @param trebuchet     - trebuchet client instance
 * @param host          - remote server hostname
 * @param port          - port trebuchet server is listening on
 * @param req           - trebuchet launch request
 * @param ret           - pointer to an struct trebuchet_launch_ret which is filled on success.
 * @return              - 0 on success, errno otherwise.
 */
int trebuchet_launch(
    struct trebuchet * trebuchet,
    const char * host,
    int port,
    void * req,
    struct trebuchet_launch_resp * ret);

/*
 * Terminate a Rift.VM on a remote trebuchet server.  If not already connected
 * a connection will be created.
 *
 * @param trebuchet       - trebuchet client instance
 * @param host            - remote server hostname
 * @param port            - port trebuchet server is listening on
 * @param component_name  - vm component name
 * @param instance_id     - vm component instance
 * @param ret             - 0 or errno filled on success.
 * @return                - 0 on success, errno otherwise.
 */
int trebuchet_terminate(
    struct trebuchet * trebuchet,
    const char * host,
    int port,
    const char * component_name,
    uint32_t instance_id,
    int * ret);

/*
 * Query the status of a Rift.VM
 *
 * @param trebuchet       - trebuchet client instance
 * @param host            - remote server hostname
 * @param port            - port trebuchet server is listening on
 * @param component_name  - vm component name
 * @param instance_id     - vm component instance
 * @param status          - on success, Rift.VM status
 * @return                - 0 on success, errno otherwise
 */
int trebuchet_get_status(
    struct trebuchet * trebuchet,
    const char * host,
    int port,
    const char * component_name,
    uint32_t instance_id,
    trebuchet_vm_status * status);

__END_DECLS
#endif

// vim: sw=2
