/*
    Copyright (c) 2012-2013 250bpm s.r.o.  All rights reserved.
    Copyright (c) 2013 GoPivotal, Inc.  All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef NN_H_INCLUDED
#define NN_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stddef.h>

/*  Handle DSO symbol visibility                                             */
#if defined _WIN32
#   if defined NN_EXPORTS
#       define NN_EXPORT __declspec(dllexport)
#   else
#       define NN_EXPORT __declspec(dllimport)
#   endif
#else
#   if defined __SUNPRO_C
#       define NN_EXPORT __global
#   elif (defined __GNUC__ && __GNUC__ >= 4) || \
          defined __INTEL_COMPILER || defined __clang__
#       define NN_EXPORT __attribute__ ((visibility("default")))
#   else
#       define NN_EXPORT
#   endif
#endif

/*  Inline functions are everywhere, but MSVC requires underscores           */
#if defined _WIN32
#  define NN_INLINE static __inline
#else
#  define NN_INLINE static inline
#endif


/******************************************************************************/
/*  ABI versioning support.                                                   */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define NN_VERSION_CURRENT 0

/*  The latest revision of the current interface. */
#define NN_VERSION_REVISION 1

/*  How many past interface versions are still supported. */
#define NN_VERSION_AGE 0

/******************************************************************************/
/*  Errors.                                                                   */
/******************************************************************************/

/*  A number random enough not to collide with different errno ranges on      */
/*  different OSes. The assumption is that error_t is at least 32-bit type.   */
#define NN_HAUSNUMERO 156384712

/*  On some platforms some standard POSIX errnos are not defined.    */
#ifndef ENOTSUP
#define ENOTSUP (NN_HAUSNUMERO + 1)
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT (NN_HAUSNUMERO + 2)
#endif
#ifndef ENOBUFS
#define ENOBUFS (NN_HAUSNUMERO + 3)
#endif
#ifndef ENETDOWN
#define ENETDOWN (NN_HAUSNUMERO + 4)
#endif
#ifndef EADDRINUSE
#define EADDRINUSE (NN_HAUSNUMERO + 5)
#endif
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL (NN_HAUSNUMERO + 6)
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED (NN_HAUSNUMERO + 7)
#endif
#ifndef EINPROGRESS
#define EINPROGRESS (NN_HAUSNUMERO + 8)
#endif
#ifndef ENOTSOCK
#define ENOTSOCK (NN_HAUSNUMERO + 9)
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT (NN_HAUSNUMERO + 10)
#endif
#ifndef EPROTO
#define EPROTO (NN_HAUSNUMERO + 11)
#endif
#ifndef EAGAIN
#define EAGAIN (NN_HAUSNUMERO + 12)
#endif
#ifndef EBADF
#define EBADF (NN_HAUSNUMERO + 13)
#endif
#ifndef EINVAL
#define EINVAL (NN_HAUSNUMERO + 14)
#endif
#ifndef EMFILE
#define EMFILE (NN_HAUSNUMERO + 15)
#endif
#ifndef EFAULT
#define EFAULT (NN_HAUSNUMERO + 16)
#endif
#ifndef EACCESS
#define EACCESS (NN_HAUSNUMERO + 17)
#endif
#ifndef ENETRESET
#define ENETRESET (NN_HAUSNUMERO + 18)
#endif
#ifndef ENETUNREACH
#define ENETUNREACH (NN_HAUSNUMERO + 19)
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH (NN_HAUSNUMERO + 20)
#endif
#ifndef ENOTCONN
#define ENOTCONN (NN_HAUSNUMERO + 21)
#endif
#ifndef EMSGSIZE
#define EMSGSIZE (NN_HAUSNUMERO + 22)
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT (NN_HAUSNUMERO + 23)
#endif
#ifndef ECONNABORTED
#define ECONNABORTED (NN_HAUSNUMERO + 24)
#endif
#ifndef ECONNRESET
#define ECONNRESET (NN_HAUSNUMERO + 25)
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT (NN_HAUSNUMERO + 26)
#endif
#ifndef EISCONN
#define EISCONN (NN_HAUSNUMERO + 27)
#define NN_EISCONN_DEFINED
#endif

/*  Native nanomsg error codes.                                               */
#ifndef ETERM
#define ETERM (NN_HAUSNUMERO + 53)
#endif
#ifndef EFSM
#define EFSM (NN_HAUSNUMERO + 54)
#endif

/*  This function retrieves the errno as it is known to the library.          */
/*  The goal of this function is to make the code 100% portable, including    */
/*  where the library is compiled with certain CRT library (on Windows) and   */
/*  linked to an application that uses different CRT library.                 */
NN_EXPORT int nn_errno (void);

/*  Resolves system errors and native errors to human-readable string.        */
NN_EXPORT const char *nn_strerror (int errnum);

/*  Returns the symbol name (e.g. "NN_REQ") and value at a specified index.   */
/*  If the index is out-of-range, returns NULL and sets errno to EINVAL       */
/*  General usage is to start at i=0 and iterate until NULL is returned.      */
NN_EXPORT const char *nn_symbol (int i, int *value);

/******************************************************************************/
/*  Helper function for shutting down multi-threaded applications.            */
/******************************************************************************/

NN_EXPORT void nn_term (void);

/******************************************************************************/
/*  Zero-copy support.                                                        */
/******************************************************************************/

#define NN_MSG ((size_t) -1)

NN_EXPORT void *nn_allocmsg (size_t size, int type);
NN_EXPORT int nn_freemsg (void *msg);

/******************************************************************************/
/*  Socket definition.                                                        */
/******************************************************************************/

struct nn_iovec {
    void *iov_base;
    size_t iov_len;
};

struct nn_msghdr {
    struct nn_iovec *msg_iov;
    int msg_iovlen;
    void *msg_control;
    size_t msg_controllen;
};

struct nn_cmsghdr {
    size_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
};

/*  Internal function. Not to be used directly.                               */
/*  Use NN_CMSG_NEXTHDR macro instead.                                        */
NN_INLINE struct nn_cmsghdr *nn_cmsg_nexthdr_ (const struct nn_msghdr *mhdr,
    const struct nn_cmsghdr *cmsg)
{
    size_t sz;

    sz = sizeof (struct nn_cmsghdr) + cmsg->cmsg_len;
    if (((char*) cmsg) - ((char*) mhdr->msg_control) + sz >=
           mhdr->msg_controllen)
        return NULL;
    return (struct nn_cmsghdr*) (((char*) cmsg) + sz);
}

#define NN_CMSG_FIRSTHDR(mhdr) \
    ((mhdr)->msg_controllen >= sizeof (struct nn_cmsghdr) \
    ? (struct nn_cmsghdr*) (mhdr)->msg_control : (struct nn_cmsghdr*) NULL)

#define NN_CMSG_NXTHDR(mhdr,cmsg) \
    nn_cmsg_nexthdr_ ((struct nn_msghdr*) (mhdr), (struct nn_cmsghdr*) (cmsg))

#define NN_CMSG_DATA(cmsg) \
    ((unsigned char*) (((struct nn_cmsghdr*) (cmsg)) + 1))

/*  Helper macro. Not to be used directly.                                    */
#define NN_CMSG_ALIGN(len) \
    (((len) + sizeof (size_t) - 1) & (size_t) ~(sizeof (size_t) - 1))

/* Extensions to POSIX defined by RFC3542.                                    */

#define NN_CMSG_SPACE(len) \
    (NN_CMSG_ALIGN (len) + NN_CMSG_ALIGN (sizeof (struct nn_cmsghdr)))

#define NN_CMSG_LEN(len) \
    (NN_CMSG_ALIGN (sizeof (struct nn_cmsghdr)) + (len))

/*  SP address families.                                                      */
#define AF_SP 1
#define AF_SP_RAW 2

/*  Max size of an SP address.                                                */
#define NN_SOCKADDR_MAX 128

/*  Socket option levels: Negative numbers are reserved for transports,
    positive for socket types. */
#define NN_SOL_SOCKET 0

/*  Generic socket options (NN_SOL_SOCKET level).                             */
#define NN_LINGER 1
#define NN_SNDBUF 2
#define NN_RCVBUF 3
#define NN_SNDTIMEO 4
#define NN_RCVTIMEO 5
#define NN_RECONNECT_IVL 6
#define NN_RECONNECT_IVL_MAX 7
#define NN_SNDPRIO 8
#define NN_SNDFD 10
#define NN_RCVFD 11
#define NN_DOMAIN 12
#define NN_PROTOCOL 13
#define NN_IPV4ONLY 14
#define NN_SOCKET_NAME 15
  /*...*/
#define NN_RWHANDSHAKE 31
#define NN_RWGET_STATS 32
#define NN_RWSET_CONN_IND 33

/*  Send/recv options.                                                        */
#define NN_DONTWAIT 1

#if defined NN_HAVE_WINDOWS
#ifndef uint64_t
typedef unsigned __int64 uint64_t;
#endif
#else
#if __WORDSIZE == 64
typedef unsigned long int uint64_t;
#else
__extension__
typedef unsigned long long int  uint64_t;
#endif
#endif


struct nn_sock_stats {

    /*****  The ever-incrementing counters  *****/

    /*  Successfully established nn_connect() connections  */
    uint64_t established_connections;
    /*  Successfully accepted connections  */
    uint64_t accepted_connections;
    /*  Forcedly closed connections  */
    uint64_t dropped_connections;
    /*  Connections closed by peer  */
    uint64_t broken_connections;
    /*  Errors trying to establish active connection  */
    uint64_t connect_errors;
    /*  Errors binding to specified port  */
    uint64_t bind_errors;
    /*  Errors accepting connections at nn_bind()'ed endpoint  */
    uint64_t accept_errors;

    /*  Messages sent  */
    uint64_t messages_sent;
    /*  Messages received  */
    uint64_t messages_received;
    /*  Bytes sent (sum length of data in messages sent)  */
    uint64_t bytes_sent;
    /*  Bytes recevied (sum length of data in messages received)  */
    uint64_t bytes_received;

    /*****  Level-style values *****/

    /*  Number of currently established connections  */
    int current_connections;
    /*  Number of connections currently in progress  */
    int inprogress_connections;
    /*  The currently set priority for sending data  */
    int current_snd_priority;
    /*  Number of endpoints having last_errno set to non-zero value  */
    int current_ep_errors;

};

NN_EXPORT int nn_socket (int domain, int protocol);
NN_EXPORT int nn_close (int s);
NN_EXPORT int nn_setsockopt (int s, int level, int option, const void *optval,
    size_t optvallen);
NN_EXPORT int nn_getsockopt (int s, int level, int option, void *optval,
    size_t *optvallen);
NN_EXPORT int nn_bind (int s, const char *addr);
NN_EXPORT int nn_connect (int s, const char *addr);
NN_EXPORT int nn_shutdown (int s, int how);
NN_EXPORT int nn_send (int s, const void *buf, size_t len, int flags);
NN_EXPORT int nn_recv (int s, void *buf, size_t len, int flags);
NN_EXPORT int nn_sendmsg (int s, const struct nn_msghdr *msghdr, int flags);
NN_EXPORT int nn_recvmsg (int s, struct nn_msghdr *msghdr, int flags);

/******************************************************************************/
/*  Socket mutliplexing support.                                              */
/******************************************************************************/

#define NN_POLLIN 1
#define NN_POLLOUT 2

struct nn_pollfd {
    int fd;
    short events;
    short revents;
};

NN_EXPORT int nn_poll (struct nn_pollfd *fds, int nfds, int timeout);

/******************************************************************************/
/*  Built-in support for devices.                                             */
/******************************************************************************/

NN_EXPORT int nn_device (int s1, int s2);

/*  Max number of concurrent SP sockets.  Note that each takes three
    real descriptors: two eventfds plus a real socket. */
#define NN_MAX_SOCKETS 32768     /* Silly big for collapsed model */

/*  Seems to be just for eventfd? */
#define NN_EVENTFD_DIRECT_OFFSET   (NN_MAX_SOCKETS * 4)    /* Totally broken, eventfd fd numbers are from the process fd namespace, unrelated to nn socket count etc */

struct nn_riftclosure {
  void (*fn)(void *ud, int conns);
  void *ud;
};

struct nn_riftconfig {
  int singlethread;
  int (*waitfunc)(int fd, int bits, int timeo);
  int (*direct_cb)(int fd);
  //int (*conn_down_cb)(void *handle);
};
NN_EXPORT void nn_global_init (struct nn_riftconfig *cfg);
NN_EXPORT void nn_run_worker(void);
NN_EXPORT void nn_worker_getfdto(int *fd, int *events, int *timeo);
NN_EXPORT void nn_global_get_riftconfig(struct nn_riftconfig *cfg_out);
NN_EXPORT int nn_global_singlethread();

#undef NN_EXPORT

#ifdef __cplusplus
}
#endif

#endif

