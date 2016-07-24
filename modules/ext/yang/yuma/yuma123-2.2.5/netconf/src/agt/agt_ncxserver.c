/*
 * Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: agt_ncxserver.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
11-jan-07    abb      begun; gathered from glibc documentation

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "procdefs.h"
#include "agt.h"
#include "agt_ncxserver.h"
#include "agt_cli.h"
#include "agt_not.h"
#include "agt_rpc.h"
#include "agt_ses.h"
#include "agt_timer.h"
#include "def_reg.h"
#include "log.h"
#include "ncx.h"
#include "ncxmod.h"
#include "ncxconst.h"
#include "ses.h"
#include "ses_msg.h"
#include "status.h"
#include "xmlns.h"


/********************************************************************
 *                                                                   *
 *                       C O N S T A N T S                           *
 *                                                                   *
 *********************************************************************/

/* how often to check for agent shutown (in seconds) */
#define AGT_NCXSERVER_TIMEOUT  1

/* number of notifications to send out in 1 timeout interval */
#define MAX_NOTIFICATION_BURST  10


static fd_set active_fd_set;
static fd_set read_fd_set;
static fd_set write_fd_set;


/********************************************************************
 * FUNCTION make_named_socket
 * 
 * Create an AF_LOCAL socket for the ncxserver
 * 
 * INPUTS:
 *    filename == full filespec of the socket filename
 *    sock == ptr to return value
 *
 * OUTPUTS:
 *    *sock == the FD for the socket if return ok
 *
 * RETURNS:
 *    status   
 *********************************************************************/
static status_t
    make_named_socket (const char *filename, int *sock)
{
    int ret;
    size_t  size;
    struct sockaddr_un name;

    *sock = 0;

    /* Create the socket. */
    *sock = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (*sock < 0) {
        perror ("socket");
        return ERR_NCX_OPERATION_FAILED;
    }

    /* Give the socket a name. */
    name.sun_family = AF_LOCAL;
    strncpy(name.sun_path, filename, sizeof(name.sun_path)-1);
    name.sun_path[sizeof(name.sun_path)-1] = 0;
    size = SUN_LEN(&name);
    ret = bind(*sock, (struct sockaddr *)&name, size);
    if (ret != 0) {
        perror ("bind");
        return ERR_NCX_OPERATION_FAILED;
    }

    /* change the permissions */
    ret = chmod(filename, (S_IRUSR | S_IWUSR |
                           S_IRGRP | S_IWGRP |
                           S_IROTH | S_IWOTH));
    if (ret != 0) {
        perror ("chmod");
        return ERR_NCX_OPERATION_FAILED;
    }

    return NO_ERR;
} /* make_named_socket */


/********************************************************************
 * FUNCTION make_tcp_socket
 *
 * Create an AF_INET socket for the ncxserver
 *
 * INPUTS:
 *    port == port
 *    sock == ptr to return value
 *
 * OUTPUTS:
 *    *sock == the FD for the socket if return ok
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    make_tcp_socket (const char *server_address, int port, int *sock)
{
    int ret;
    size_t  size;
    struct sockaddr_in name;
    int so_reuse_address_option=1;
    struct hostent* hp;

    *sock = 0;

    /* Create the socket. */
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock < 0) {
        perror ("socket");
        return ERR_NCX_OPERATION_FAILED;
    }

    if (setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, (char*) &so_reuse_address_option, sizeof(so_reuse_address_option)) < 0) {
        perror ("setsockopt");
        return ERR_NCX_OPERATION_FAILED;
    }

    hp = gethostbyname(server_address);
    if (hp == NULL) {
        perror ("gethostbyname");
        return ERR_NCX_OPERATION_FAILED;
    }
    size = sizeof(name);
    memset((char *) &name, 0, size);
    name.sin_family = AF_INET;
    name.sin_port = htons((unsigned short)port);
    memcpy((char *) &name.sin_addr, hp->h_addr, hp->h_length);

    ret = bind(*sock, (struct sockaddr *)&name, size);
    if (ret != 0) {
        perror ("bind");
        return ERR_NCX_OPERATION_FAILED;
    }

    return NO_ERR;
} /* make_tcp_socket */


/********************************************************************
 * FUNCTION send_some_notifications
 * 
 * Send some notifications as needed
 * 
 * INPUTS:
 *    filename == full filespec of the socket filename
 *    sock == ptr to return value
 *
 * OUTPUTS:
 *    *sock == the FD for the socket if return ok
 *
 * RETURNS:
 *    status   
 *********************************************************************/
static void
    send_some_notifications (ncx_instance_t *instance)
{
    const agt_profile_t  *agt_profile;
    uint32                sendcount, sendtotal, sendmax;
    boolean               done;

    sendcount = 0;
    sendtotal = 0;

    /* get --maxburst CLI param value */
    agt_profile = agt_get_profile();
    sendmax = agt_profile->agt_maxburst;

    done = FALSE;
    while (!done) {
        sendcount = agt_not_send_notifications(instance);
        if (sendcount) {
            sendtotal += sendcount;
            if (sendmax && (sendtotal >= sendmax)) {
                done = TRUE;
            }
        } else {
            done = TRUE;
        }
    }

    if (agt_profile->agt_eventlog_size == 0) {
        agt_not_clean_eventlog(instance);
    }

} /* send_some_notifications */



/***********     E X P O R T E D   F U N C T I O N S   *************/


/********************************************************************
 * FUNCTION agt_ncxserver_run
 * 
 * IO server loop for the ncxserver socket
 * 
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    agt_ncxserver_run (ncx_instance_t *instance)
{
    ses_cb_t              *scb;
    agt_profile_t         *profile;
    int                    ncxsock, maxwrnum, maxrdnum;
    int                    i, new, ret;
    struct sockaddr_un     clientname;
    struct timeval         timeout;
    socklen_t              size;
    status_t               res;
    boolean                done, done2, stream_output;
    char*                  tcp_direct_address = NULL;
    int                    tcp_direct_port = -1;
    char*                  ncxserver_sockname;
    val_value_t            *clivalset;
    val_value_t            *val;

    /* Create the socket and set it up to accept connections. */
    clivalset = agt_cli_get_valset();
    if (clivalset) {

        val = val_find_child(instance,
                             clivalset,
                             NCXMOD_NETCONFD,
                             NCX_EL_TCP_DIRECT_PORT);
        if(val != NULL) {
            tcp_direct_port = VAL_INT(val);
        }

        val = val_find_child(instance,
                             clivalset,
                             NCXMOD_NETCONFD,
                             NCX_EL_TCP_DIRECT_ADDRESS);
        if(val != NULL) {
            tcp_direct_address = (char *)VAL_STR(val);
            if(tcp_direct_port==-1) tcp_direct_port = 2023;
        }

        val = val_find_child(instance,
                             clivalset,
                             NCXMOD_NETCONFD,
                             NCX_EL_NCXSERVER_SOCKNAME);
        if(val != NULL) {
            ncxserver_sockname = (char *)VAL_STR(val);
        } else {
            ncxserver_sockname = (char *)(NCXSERVER_SOCKNAME);
        }

    } else {
            log_error(instance, "\n*** agt_ncxserver_run:agt_cli_get_valset failed.\n");
            return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }
    if(tcp_direct_port!=-1) {
        res = make_tcp_socket(tcp_direct_address, tcp_direct_port, &ncxsock);
        if (res != NO_ERR) {
            log_error(instance,"\n*** Cannot connect to ncxserver socket listen tcp port: %d\n",tcp_direct_port);
            return res;
        }
    } else {
        res = make_named_socket(ncxserver_sockname, &ncxsock);
        if (res != NO_ERR) {
            log_error(instance, "\n*** Cannot connect to ncxserver socket"
                      "\n*** If no other instances of netconfd are running,"
                      "\n*** try deleting %s\n",ncxserver_sockname);
            return res;
        }
    }
    profile = agt_get_profile();
    if (profile == NULL) {
        return SET_ERROR(instance, ERR_INTERNAL_VAL);
    }

    stream_output = profile->agt_stream_output;

    if (listen(ncxsock, 1) < 0) {
        log_error(instance, "\nError: listen failed");
        return ERR_NCX_OPERATION_FAILED;
    }
     
    /* Initialize the set of active sockets. */
    FD_ZERO(&read_fd_set);
    FD_ZERO(&write_fd_set);
    FD_ZERO(&active_fd_set);
    FD_SET(ncxsock, &active_fd_set);
    maxwrnum = maxrdnum = ncxsock;

    done = FALSE;
    while (!done) {

        /* check exit program */
        if (agt_shutdown_requested()) {
            done = TRUE;
            continue;
        }

        ret = 0;
        done2 = FALSE;
        while (!done2) {
            read_fd_set = active_fd_set;
            agt_ses_fill_writeset(instance, &write_fd_set, &maxwrnum);
            timeout.tv_sec = AGT_NCXSERVER_TIMEOUT;
            timeout.tv_usec = 0;

            /* Block until input arrives on one or more active sockets. 
             * or the timer expires
             */
            ret = select(max(maxrdnum+1, maxwrnum+1), 
                         &read_fd_set, 
                         &write_fd_set, 
                         NULL, 
                         &timeout);
            if (ret > 0) {
                done2 = TRUE;
            } else if (ret < 0) {
                if (!(errno == EINTR || errno==EAGAIN)) {
                    done2 = TRUE;
                }
            } else if (ret == 0) {
                /* should only happen if a timeout occurred */
                if (agt_shutdown_requested()) {
                    done2 = TRUE; 
                } else {
                    /* !! put all polling callbacks here for now !! */
                    agt_ses_check_timeouts(instance);
                    agt_timer_handler(instance);
                    send_some_notifications(instance);
                }
            } else {
                /* normal return with some bytes */
                done2 = TRUE;
            }
        }

        /* check exit program */
        if (agt_shutdown_requested()) {
            done = TRUE;
            continue;
        }

        /* check select return status for non-recoverable error */
        if (ret < 0) {
            res = ERR_NCX_OPERATION_FAILED;
            log_error(instance, 
                      "\nncxserver select failed (%s)", 
                      strerror(errno));
            agt_request_shutdown(instance, NCX_SHUT_EXIT);
            done = TRUE;
            continue;
        }
     
        /* Service all the sockets with input and/or output pending */
        done2 = FALSE;
        for (i = 0; i < max(maxrdnum+1, maxwrnum+1) && !done2; i++) {

            /* check write output to client sessions */
            if (!stream_output && FD_ISSET(i, &write_fd_set)) {
                /* try to send 1 packet worth of buffers for a session */
                scb = def_reg_find_scb(instance, i);
                if (scb) {
                    /* check if anything to write */
                    if (!dlq_empty(instance, &scb->outQ)) {
                        res = ses_msg_send_buffs(instance, scb);
                        if (res != NO_ERR) {
                            if (LOGINFO) {
                                log_info(instance, 
                                         "\nagt_ncxserver write failed; "
                                         "closing session %d ", 
                                         scb->sid);
                            }
                        }
                        if (res != NO_ERR) {
                            agt_ses_kill_session(instance, 
                                                 scb, 
                                                 scb->sid,
                                                 SES_TR_OTHER);
                            scb = NULL;
                        } else if (scb->state == SES_ST_SHUTDOWN_REQ) {
                            /* close-session reply sent, now kill ses */
                            agt_ses_kill_session(instance, 
                                                 scb, 
                                                 scb->killedbysid,
                                                 scb->termreason);
                            scb = NULL;
                        }
                    }

                    /* check if any buffers left over for next loop */
                    if (scb && !dlq_empty(instance, &scb->outQ)) {
                        ses_msg_make_outready(instance, scb);
                    }
                }
            }

            /* check read input from client sessions */
            if (FD_ISSET(i, &read_fd_set)) {
                if (i == ncxsock) {
                    /* Connection request on original socket. */
                    size = (socklen_t)sizeof(clientname);
                    new = accept(ncxsock,
                                 (struct sockaddr *)&clientname,
                                 &size);
                    if (new < 0) {
                        if (LOGINFO) {
                            log_info(instance,
                                     "\nagt_ncxserver accept "
                                     "connection failed (%d)",
                                     new);
                        }
                        continue;
                    }

                    /* get a new session control block */
                    if (!agt_ses_new_session(instance, SES_TRANSPORT_SSH, new)) {
                        close(new);
                        if (LOGINFO) {
                            log_info(instance, 
                                     "\nagt_ncxserver new "
                                     "session failed (%d)", 
                                     new);
                        }
                    } else {
                        /* set non-blocking IO */
                        if (fcntl(new, F_SETFD, O_NONBLOCK)) {
                            if (LOGINFO) {
                                log_info(instance, "\nfnctl failed");
                            }
                        }
                        FD_SET(new, &active_fd_set);
                        if (new > maxrdnum) {
                            maxrdnum = new;
                        }
                    }
                } else {
                    /* Data arriving on an already-connected socket.
                     * Need to have the xmlreader for this session
                     */
                    scb = def_reg_find_scb(instance, i);
                    if (scb != NULL) {
                        res = ses_accept_input(instance, scb);
                        if (res != NO_ERR) {
                            if (i >= maxrdnum) {
                                maxrdnum = i-1;
                            }
                            if (res != ERR_NCX_SESSION_CLOSED) {
                                if (LOGINFO) {
                                    log_info(instance,
                                             "\nagt_ncxserver: input failed"
                                             " for session %d (%s)",
                                             scb->sid, 
                                             get_error_string(res));
                                }
                                /* send an error reply instead of
                                 * killing the session right now
                                 */
                                agt_rpc_send_error_reply(instance, scb, res);
                                agt_ses_request_close(instance, 
                                                      scb, 
                                                      0, 
                                                      SES_TR_OTHER);
                            } else {
                                /* connection already closed
                                 * so kill session right now
                                 */
                                agt_ses_kill_session(instance,
                                                     scb,
                                                     scb->sid,
                                                     SES_TR_DROPPED);
                            }
                        }
                    }
                }
            }
        }

        /* drain the ready queue before accepting new input */
        if (!done) {
            done2 = FALSE;
            while (!done2) {
                if (!agt_ses_process_first_ready(instance)) {
                    done2 = TRUE;
                } else if (agt_shutdown_requested()) {
                    done = done2 = TRUE;
                } else {
                    send_some_notifications(instance);
                }
            }
        }
    }  /* end select loop */

    /* all open client sockets will be closed as the sessions are
     * torn down, but the original ncxserver socket needs to be closed now
     */
    close(ncxsock);
    unlink(NCXSERVER_SOCKNAME);
    return NO_ERR;

}  /* agt_ncxserver_run */


/********************************************************************
 * FUNCTION agt_ncxserver_clear_fd
 * 
 * Clear a dead session from the select loop
 * 
 * INPUTS:
 *   fd == file descriptor number for the socket to clear
 *********************************************************************/
void
    agt_ncxserver_clear_fd (int fd)
{
    FD_CLR(fd, &active_fd_set);

} /* agt_ncxserver_clear_fd */


/* END agt_ncxserver.c */



