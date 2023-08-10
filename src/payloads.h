#ifndef __H_PAYLOADS__
#define __H_PAYLOADS__

#include <stdint.h>

typedef struct __attribute__ ((packed)) FileRequestPayload {
    size_t    filesize;
    char      filename[255];
} FileRequestPayload;

#endif