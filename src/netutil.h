#ifndef __H_NETUTIL__
#define __H_NETUTIL__

#include <stdint.h>
#include <sys/types.h>

uint8_t *build_file_share_request_packet(size_t filesize, const char *filename);
uint8_t *bad_response();
uint8_t *ok_response();

#endif