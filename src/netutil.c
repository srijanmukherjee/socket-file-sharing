#include "netutil.h"

#include <string.h>

#include <arpa/inet.h>
#include "constants.h"
#include "protocol.h"
#include "payloads.h"

uint8_t *build_file_share_request_packet(size_t filesize, const char *filename) {
    FileRequestPayload payload;
    payload.filesize = filesize;
    memcpy(payload.filename, filename, strlen(filename) + 1);
    return build_packet(
        MAJOR_VERSION,
        MINOR_VERSION,
        REQUEST_SEND_FILE,
        0,
        sizeof(FileRequestPayload),
        (uint8_t*) &payload
    );
}

uint8_t *bad_response() {
    return build_packet(
        MAJOR_VERSION,
        MINOR_VERSION,
        RESPONSE_BAD,
        0,
        0,
        NULL
    );
}

uint8_t *ok_response() {
    return build_packet(
        MAJOR_VERSION,
        MINOR_VERSION,
        RESPONSE_OK,
        0,
        0,
        NULL
    );
}