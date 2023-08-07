#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
   #error "Compiler not supported"
#endif

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
// #include <libgen.h>

#if defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION)
    #include <sys/sendfile.h>
#endif

#include "server.h"
#include "argsettings.h"
#include "constants.h"

#define DISABLE_CURSOR "\e[?25l"
#define ENABLE_CURSOR  "\e[?25h"

int handle_request(Socket client);
void send_file(const char* destination, char* filepath, int port);
void receive_file(int port);

int main(int argc, char *argv[]) 
{
    // parse command line arguments
    struct arguments arguments;
    memset(&arguments, 0, sizeof(arguments));
    arguments.port = DEFAULT_PORT;
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    if (strcmp("recv", arguments.args[0]) == 0) 
        receive_file(arguments.port);
    else 
        send_file(arguments.args[1], arguments.args[2], arguments.port);
}

void send_file(const char* destination, char* filepath, int port) {
    struct sockaddr_in servaddr;
    socklen_t addrlen;
    struct stat sb;
    off_t offset;
    int sockfd;
    FILE* fp;
    
    addrlen = sizeof(servaddr);

    printf("sending %s to %s\n", filepath, destination);

    if (stat(filepath, &sb) < 0) {
        perror("stat() failed");
        exit(EXIT_FAILURE);
    }

    if ((sb.st_mode & __S_IFMT) != __S_IFREG) {
        fprintf(stderr, "file not supported\n");
        exit(EXIT_FAILURE);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(destination);
    servaddr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*) &servaddr, addrlen) < 0) {
        perror("connect() failed");
        exit(EXIT_FAILURE);
    }    

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    sprintf(buffer, "%ld %s\n", sb.st_size, basename(filepath));
    
    if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
        perror("send() failed");
        exit(EXIT_FAILURE);
    }

    if (recv(sockfd, buffer, sizeof(buffer), 0) <= 0) {
        perror("receiver rejected");
        exit(EXIT_FAILURE);
    }

    if (buffer[0] != 1) {
        fprintf(stderr, "receiver rejected\n");
        exit(EXIT_FAILURE);
    }

    fp      = fopen(filepath, "r");
    offset  = 0;

    printf(DISABLE_CURSOR);
    
    while (offset < sb.st_size) {

#if __APPLE__
    #if __TARGET_OS_MAC
        off_t len = BUFFER_SIZE;
        if (sendfile(sockfd, fileno(fp), offset, &len, NULL, 0) < 0) {
            perror("sendfile() failed");
            exit(2);
        }

        offset += len;
    #else
        #error "Your platform is not supported"
    #endif
#elif defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION)
    sendfile64(sockfd, fileno(fp), &offset, BUFFER_SIZE);
#else
    #error "Your platform is not supported"
#endif
        printf("\rprogress: %4.1f%%\r",  
                (float) offset * 100.0 / sb.st_size);
    }

    printf(ENABLE_CURSOR);
    printf("\ndone\n");

    fclose(fp);
}

void receive_file(int port) {
    Socket socket;

    if (create_tcp_server(&socket, AF_INET, INADDR_ANY, port, 1) < 0) {
        perror("failed to start server");
        exit(1);
    }

    printf("Server running at %s:%d\n", inet_ntoa(socket.addr.sin_addr), ntohs(socket.addr.sin_port));
    listen_server(&socket, handle_request);
}

int handle_request(Socket client) {
    char buf[BUFFER_SIZE] = {0};

    FILE* fp = NULL;
    size_t nread = 0;
    char *str, *endptr;
    long filesize;

    memset(buf, 0, sizeof(buf));
    while ((nread = recv(client.fd, buf, sizeof(buf), 0)) > 0) {
        // extract file size and file name
        str = strdup(buf);
        endptr = NULL;
        filesize = strtol(str, &endptr, 10);
        endptr++;

        // remove the last newline
        endptr[strlen(endptr) - 1] = '\0';

        printf("receiving %s (%ld bytes) from %s\n", endptr, filesize, inet_ntoa(client.addr.sin_addr));

        // open and allocate disk space for incoming file
        fp = fopen(endptr, "w+");
        if (fallocate(fileno(fp), 0, 0, filesize) < 0) {
            perror("failed to allocate space for file");
            break;
        }

        // send ACK: ready to start receiving byte stream
        memset(buf, 0, sizeof(buf));
        buf[0] = 1;
        send(client.fd, buf, 1, 0);

        struct timeval curr, last, start;
        gettimeofday(&last, NULL);
        start = last;

        size_t total_received = 0;
        size_t samples = 0;
        double cummulative_speed = 0;

        printf(DISABLE_CURSOR);

        while ((nread = recv(client.fd, buf, sizeof(buf), 0)) > 0) {
            // TODO: replace with faster function if available
            gettimeofday(&curr, NULL); 
            cummulative_speed += (nread * 1000000 / (curr.tv_usec - last.tv_usec)) / 1e6;
            samples++;

            total_received += fwrite(buf, 1, nread, fp);

            printf("\rspeed: %10.1lf MBPS completion: %4.1f%%\r", 
                cummulative_speed / samples, 
                (float) total_received * 100.0 / filesize);

            last = curr;
        }

        printf(ENABLE_CURSOR);    

        gettimeofday(&curr, NULL);
        if (curr.tv_sec == start.tv_sec) {
            printf("\ndone in %lfs\n", ((double)curr.tv_usec - start.tv_usec) / 1e6);
        } else {
            printf("\ndone in %lds\n", curr.tv_sec - start.tv_sec);
        }

        free(str);
        fclose(fp);
        fp = NULL;
    }

    return nread <= 0;
}
