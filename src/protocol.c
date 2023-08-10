#include "protocol.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "util.h"

static uint8_t* pack_uint8(uint8_t* dest, uint8_t src) {
    *(dest++) = src;
    return dest;
}

static uint8_t* pack_uint32(uint8_t* dest, uint32_t src) {
    *((uint32_t*)dest) = src;
    return dest + 4;
}

uint8_t* build_packet(
    uint8_t major_version,
    uint8_t minor_version,
    uint8_t type,
    uint8_t flags,
    uint32_t body_length,
    const uint8_t* body
) {
    uint8_t* packet = malloc(7 + body_length);
    uint8_t* p = packet;

    p = pack_uint8(p, (major_version << 4) | minor_version);
    p = pack_uint8(p, type);
    p = pack_uint8(p, flags);
    p = pack_uint32(p, body_length);
    
    if (body) memcpy(p, body, body_length);

    return packet;
}

int read_packet(Packet* packet, PacketInfo* packet_info, uint8_t buf[], ssize_t* buf_len) {
    if (packet == NULL || buf_len == NULL || packet_info == NULL || buf == NULL)
        return PACKET_ERROR;

    if (!packet_info->header_done) {
        
        // first 7 bytes define the header. it can't be partial
        if (*buf_len < 7)
            return PACKET_ERROR;

        uint8_t version = buf[0];
        packet->minor_version = version & 0xF;
        packet->major_version = version >> 4;
        packet->type = buf[1];
        packet->flags = buf[2];
        buf += 3;

        // read next 4 bytes as uint32_t
        uint32_t* p = *(uint32_t**) &buf;
        packet->body_length = p[0];
        buf += 4;

        packet_info->header_done = 1;

        buf = memmove(buf - 7, buf, *buf_len - 7);
        *buf_len -= 7;

        if (packet->body_length == 0)
            return PACKET_COMPLETE;

        // allocate memory for body
        packet->body = malloc(sizeof(uint8_t) * packet->body_length);

        uint32_t len = min(*buf_len, packet->body_length);
        memcpy(packet->body, buf, len);
        packet_info->body_length += len;
        
        if (*buf_len > len)
            memmove(buf, buf + len, *buf_len - len);

        *buf_len -= len;

        return packet_info->body_length == packet->body_length ? PACKET_COMPLETE : PACKET_PARTIAL;
    }

    // read the remaining body
    uint32_t len = min(*buf_len, packet->body_length - packet_info->body_length);
    memcpy(packet->body + packet_info->body_length, buf, len);
    packet_info->body_length += len;

    if (*buf_len > len)
        memmove(buf, buf + len, *buf_len - len);
    
    *buf_len -= len;

    return packet_info->body_length == packet->body_length ? PACKET_COMPLETE : PACKET_PARTIAL;
}

int send_packet(int sockfd, const uint8_t *packet) {
    uint32_t body_length = ((uint32_t*) (packet + 3))[0];
    uint32_t packet_size = 7 + body_length;
    return send(sockfd, packet, packet_size, 0);
}

int recv_packet(int sockfd, uint8_t * restrict buf, uint32_t buf_capacity, ssize_t *buf_len, Packet *packet) {
    if (buf == NULL || packet == NULL)
        return -1;
    
    ssize_t nread;
    int pstat = PACKET_ERROR;

    PacketInfo packet_info;
    memset(&packet_info, 0, sizeof(packet_info));

    while ((nread = recv(sockfd, buf, buf_capacity, 0)) > 0) {
        if ((pstat = read_packet(packet, &packet_info, buf, &nread)) == PACKET_ERROR)
            return -1;

        if (pstat == PACKET_COMPLETE)
            break;
    }

    if (pstat != PACKET_COMPLETE || (nread == 0 && pstat != PACKET_COMPLETE) || nread < 0)
        return -1;

    if (buf_len) *buf_len = nread;
    return 0;
}