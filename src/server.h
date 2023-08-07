#ifndef __H_SERVER__
#define __H_SERVER__

#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>

#define MAX_CONNECTIONS 1024
#define BACKLOG         2
#define POLL_TIMEOUT    1 * 5 * 1000

typedef struct Socket {
    struct sockaddr_in addr;
    int fd;
} Socket;

typedef struct PolledConnection {
    struct pollfd fds[MAX_CONNECTIONS];
    Socket sockets[MAX_CONNECTIONS];
    uint32_t timeout;
    uint32_t flags;
    size_t n;
} PolledConnection;

typedef int (*request_callback)(Socket);

int create_server(Socket* restrict  server, int domain, int type, int protocol, sa_family_t family, in_addr_t address, in_port_t port);
int create_tcp_server(Socket* const server, sa_family_t family, in_addr_t address, in_port_t port, int non_blocking);
int create_tcp_client(Socket* const server, sa_family_t family, in_addr_t address, in_port_t port, int non_blocking);
int listen_server(Socket* const server, request_callback callback);

#endif