
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rwcli_zsh.h
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 07/15/2015 
 * @brief 
 *  The Rift plugin for zsh uses these exports to invoke RW.CLI methods.
 *  Serves as the C wrappers for RW.CLI cpp methods.
 */

#ifndef __RWCLI_AGENT_H__
#define __RWCLI_AGENT_H__

#define RW_CLI_MAX_PIPES 4
#define RW_CLI_NO_MORE RW_CLI_MAX_PIPES
#define RW_CLI_NOT_FREE (-1)
#define RW_CLI_MAX_BUF (256 * 1024)

typedef union rwcli_pipe_s {
  int fds[2];
  struct {
    int read;
    int write;
  }fd;
}rwcli_pipe_t;

typedef struct rwcli_msg_channel_s {
  rwcli_pipe_t in;
  rwcli_pipe_t out;
}rwcli_msg_channel_t;

extern rwcli_msg_channel_t rwcli_agent_ch;

extern rwtrace_ctx_t* rwcli_trace;

#endif /* __RWCLI_AGENT_H__ */
