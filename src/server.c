#include "server.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/tcp.h>

#define POLL_ARRAY_CHANGED 1

static PolledConnection create_polled_connection(uint32_t timeout);
static int add_to_polled_connection(PolledConnection* conn, Socket sock);
static void remove_polled_connection(PolledConnection* conn, int i);
static void compress_array_polled_connection(PolledConnection* conn);

int create_server(Socket* const server, int domain, int type, int protocol, sa_family_t family, in_addr_t address, in_port_t port) {
    if (server == NULL) 
        return -1;

    // intialize network address struct
    memset(&server->addr, 0, sizeof(server->addr));
    server->addr.sin_family = family;
    server->addr.sin_addr.s_addr = address;
    server->addr.sin_port = htons(port);

    if ((server->fd = socket(domain, type, protocol)) < 0)
        return -1;

    if (setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
       fprintf(stderr, "[WARN] could not set SO_REUSEADDR option\n");
    
    if (bind(server->fd, (struct sockaddr*) &server->addr, sizeof(server->addr)) < 0)
        return -1;

    return 0;
}

int create_tcp_server(Socket* const server, sa_family_t family, in_addr_t address, in_port_t port, int non_blocking) {
    int rc = create_server(server, AF_INET, SOCK_STREAM, IPPROTO_TCP, family, address, port);
    if (rc < 0)
        return rc;

    setsockopt(server->fd, SOL_SOCKET, TCP_NODELAY, &(int){1}, sizeof(int));

    if (non_blocking) {
        rc = fcntl(server->fd, F_SETFL, fcntl(server->fd, F_GETFL, 0) | O_NONBLOCK);
        
        if (rc < 0) {
            perror("fcntl() failed");
            close(server->fd);
            return -1;
        }
    }

    return rc;
}

int listen_server(Socket* const server, request_callback callback) {
    if (server == NULL)
        return -1;

    if (listen(server->fd, BACKLOG) < 0) {
        perror("listen() failed");
        return -1;
    }

    PolledConnection conn = create_polled_connection(POLL_TIMEOUT);
    Socket client_sock;
    socklen_t client_addr_len = sizeof(client_sock.addr);
    int rc, close_connection, current_size;
    int should_stop = 0;

    add_to_polled_connection(&conn, *server);


    do {
        rc = poll(conn.fds, conn.n, conn.timeout);

        if (rc < 0) {
            perror("poll() failed");
            sleep(2);
            break;
        }

        // poll timeout
        if (rc == 0) continue;

        current_size = conn.n;
        for (int i = 0; i < current_size; ++i) {
            if (conn.fds[i].revents == 0)
                continue;

            if (conn.fds[i].revents != POLLIN) {
                remove_polled_connection(&conn, i);
                goto compress_array_section;
            }

            // Listening descriptor is readable | new connection
            if (conn.fds[i].fd == server->fd) {
                // accept all incoming connections
                do {
                    client_sock.fd = accept(server->fd, (struct sockaddr*) &client_sock.addr, &client_addr_len);

                    if (client_sock.fd < 0) {
                        if (errno != EWOULDBLOCK)
                            perror("accept() failed");
                        break;
                    }

                    add_to_polled_connection(&conn, client_sock);
                } while (client_sock.fd != -1);
            } 

            // not listening descriptor, i.e
            // existing connection must be readable
            else {
                close_connection = callback(conn.sockets[i]);

                if (close_connection) 
                    remove_polled_connection(&conn, i);
            }
        }

compress_array_section:
        compress_array_polled_connection(&conn);

    } while (!should_stop);

    return 0;
}

/* Connection polling utilities */

static PolledConnection create_polled_connection(uint32_t timeout) {
    PolledConnection conn = {
        .timeout = timeout,
        .flags = 0,
        .n = 0
    };

    memset(conn.fds, 0, sizeof(conn.fds));
    memset(conn.sockets, 0, sizeof(conn.fds));

    return conn;
}

static int add_to_polled_connection(PolledConnection* conn, Socket sock) {
    if (conn->n == MAX_CONNECTIONS) 
        return -1;
    
    conn->sockets[conn->n] = sock;
    conn->fds[conn->n].fd = sock.fd;
    conn->fds[conn->n].events = POLLIN;
    conn->n++;

    return 0;
}

static void remove_polled_connection(PolledConnection* conn, int i) {
    close(conn->fds[i].fd);
    conn->sockets[i].fd = conn->fds[i].fd = -1;
    conn->n--;
    conn->flags |= POLL_ARRAY_CHANGED;
}

static void compress_array_polled_connection(PolledConnection* conn) {
    if ((conn->flags & POLL_ARRAY_CHANGED) == 0)
        return;

    conn->flags = conn->flags & ~POLL_ARRAY_CHANGED;

    for (int i = 0; i < conn->n; ++i) {
        if (conn->fds[i].fd != -1) continue;

        for (int j = i; j < conn->n - 1; ++j) {
            conn->fds[i] = conn->fds[i + 1];
            conn->sockets[i] = conn->sockets[i + 1];
        }

        conn->n--;
        i--;
    }
}