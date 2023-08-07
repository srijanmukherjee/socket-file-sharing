#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
   #error "Compiler not supported"
#endif

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <libgen.h>
#include <sys/fcntl.h>

#if defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION)
    #include <sys/sendfile.h>
#endif

#include "../argp-standalone/include/argp-standalone/argp.h"
#include "server.h"

#define BUFFER_SIZE 96 * 1024
#define DEFAULT_PORT 80

int handle_request(Socket client);
void send_file(const char* destination, char* filepath, int port);
void receive_file(int port);

const char* program_version = "share 1.0";
const char* program_bug_address = "<emailofsrijan@gmail.com>";

struct arguments {
    char *args[3];
    int verbose;
    int port;
};

// Order of fields: {NAME, KEY, ARG, FLAGS, DOC}
static struct argp_option options[] = {
    {"verbose", 'v', 0, OPTION_ARG_OPTIONAL, "Enable verbose output"},
    {"port", 'p', "PORTNO", OPTION_ARG_OPTIONAL, "Set socket port"},
    {0}    
};

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    struct arguments* arguments = state->input;

    switch(key) {
        case 'v':
            arguments->verbose = 1;
            break;
        case 'p': {
            if (arg[0] == '=') arg++;
            
            arguments->port = strtol(arg, NULL, 10);
            if (arguments->port <= 0) {
                fprintf(stderr, "[WARN] falling back to default port (%d)\n", DEFAULT_PORT);
                arguments->port = DEFAULT_PORT;
            }
            break;
        }
        case ARGP_KEY_ARG: {
            // first argument must be recv or send
            if (state->arg_num == 0 && ! (strcmp(arg, "recv") == 0 || strcmp(arg, "send") == 0))
                argp_usage(state);
            
            if (state->arg_num > 0) {
                int mode = arguments->args[0][0] == 'r';
                
                // mode=1 recv
                // mode=0 send <ip> <file>
                int num_args = mode == 1 ? 1 : 3;
                if (state->arg_num >= num_args)
                    argp_usage(state);    
            }

            arguments->args[state->arg_num] = arg;
            break;
        }
        case ARGP_KEY_END: {
            if (state->arg_num == 0)
                argp_usage(state);
            
            int num_args = arguments->args[0][0] == 'r' ? 1 : 3;
            if (state->arg_num < num_args)
                argp_usage(state);
            
            break;
        }
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static char arg_doc[] = "ARG1 [ARG2 ARG3]";

static char doc[] = "share -- A program to share file between linux computers.";

static struct argp argp = { options, parse_opt, arg_doc, doc };

int main(int argc, char *argv[]) 
{
    struct arguments arguments;
    memset(&arguments, 0, sizeof(arguments));
    arguments.port = DEFAULT_PORT;
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    if (strcmp("recv", arguments.args[0]) == 0) receive_file(arguments.port);
    else send_file(arguments.args[1], arguments.args[2], arguments.port);
}

void send_file(const char* destination, char* filepath, int port) {
    printf("sending %s to %s\n", filepath, destination);
    struct sockaddr_in servaddr;
    int sockfd;
    socklen_t addrlen = sizeof(servaddr);
    struct stat sb;

    if (stat(filepath, &sb) < 0) {
        perror("stat() failed");
        exit(1);
    }
       int fallocate(int fd, int mode, off_t offset, off_t len);


    if ((sb.st_mode & __S_IFMT) != __S_IFREG) {
        fprintf(stderr, "file not supported\n");
        exit(1);
    }


    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket() failed");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(destination);
    servaddr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*) &servaddr, addrlen) < 0) {
        perror("connect() failed");
        exit(1);
    }    

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    sprintf(buffer, "%ld %s\n", sb.st_size, basename(filepath));
    
    if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
        perror("send() failed");
        exit(1);
    }

    if (recv(sockfd, buffer, sizeof(buffer), 0) <= 0) {
        perror("receiver rejected");
        exit(1);
    }

    if (buffer[0] != 1) {
        fprintf(stderr, "Receiver rejected\n");
        exit(1);
    }

    FILE* fp = fopen(filepath, "r");

#if 0
    size_t read_bytes = 0;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        send(sockfd, buffer, read_bytes, 0);
#else
    off_t offset = 0;
    printf("\e[?25l"); // hide the cursor
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
    printf("\e[?25h"); // enable the cursor
    printf("\ndone\n");
#endif
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

    memset(buf, 0, sizeof(buf));
    while ((nread = recv(client.fd, buf, sizeof(buf), 0)) > 0) {
        char* str = strdup(buf);
        char* endptr = NULL;
        long filesize = strtol(str, &endptr, 10);
        endptr++;
        endptr[strlen(endptr) - 1] = '\0';

        printf("receiving %s (%ld bytes) from %s\n", endptr, filesize, inet_ntoa(client.addr.sin_addr));

        fp = fopen(endptr, "w+");
        if (fallocate(fileno(fp), 0, 0, filesize) < 0) {
            perror("failed to allocate space for file");
            break;
        }
        // send ACK
        memset(buf, 0, sizeof(buf));
        buf[0] = 1;
        send(client.fd, buf, 1, 0);

        struct timeval curr, last, start;
        gettimeofday(&last, NULL);
        start = last;
        size_t total_received = 0;
        double cummulative_speed = 0;
        size_t samples = 0;

        printf("\e[?25l"); // hide the cursor
        while ((nread = recv(client.fd, buf, sizeof(buf), 0)) > 0) {
            gettimeofday(&curr, NULL);
            cummulative_speed += (nread * 1000000 / (curr.tv_usec - last.tv_usec)) / 1e6;
            samples++;
            printf("\rspeed: %10.1lf MBPS completion: %4.1f%%\r", 
                cummulative_speed / samples, 
                (float) total_received * 100.0 / filesize);

            total_received += fwrite(buf, 1, nread, fp);
            
            last = curr;
        }
        printf("\rspeed: %10.1lf MBPS completion: %4.1f%%\r", 
                cummulative_speed / samples, 
                (float) total_received * 100.0 / filesize);
        // re-enable the cursor
        printf("\e[?25h");    

        gettimeofday(&curr, NULL);
        printf("\ndone in %lds\n", curr.tv_sec - start.tv_sec);

        fclose(fp);
        fp = NULL;
    }

    return nread <= 0;
}
