#ifndef __H_PROTOCOL__
#define __H_PROTOCOL__

#include <sys/types.h>
#include <stdint.h>

#define PACKET_COMPLETE         0
#define PACKET_PARTIAL          1
#define PACKET_ERROR           -1

enum RequestType {
    REQUEST_SEND_FILE       = 0b10000000,
    REQUEST_DISCOVER        = 0b10000001
};
 
enum ResponseType {
    RESPONSE_OK             = 0b00000000,
    RESPONSE_BAD            = 0b00000001,
    RESPONSE_DISCOVERED     = 0b00000010
};

typedef struct Packet {
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t type;
    uint8_t flags;
    uint32_t body_length;
    uint8_t* body;
} Packet;

typedef struct PacketInfo {
    uint8_t header_done;
    uint32_t body_length;
} PacketInfo;

/** 
 @brief creates a custom packet
*/
uint8_t* build_packet(
    uint8_t major_version,
    uint8_t minor_version,
    uint8_t type,
    uint8_t flags,
    uint32_t body_length,
    const uint8_t* body
);

int read_packet(Packet* packet, PacketInfo* packet_info, uint8_t buf[], ssize_t* buf_len);

int send_packet(int sockfd, const uint8_t *packet);
int recv_packet(int sockfd, uint8_t *buf, uint32_t buf_capacity, ssize_t *buf_len, Packet *packet);

#endif