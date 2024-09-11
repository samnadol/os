#pragma once

#include <stdint.h>

#include "../l0/ethernet.h"
#include "../l2/tcp.h"

typedef struct http_header {
    char *key;
    char *value;

    struct http_header *next;
} http_header;

typedef struct http_response {
    char *response_string;
    http_header *headers;
    char *data;
} http_response;

bool http_send_request(network_device *netdev, uint32_t dip, uint16_t dport, char *host, char *path, tcp_listener listener);
http_response *http_parse_response(void *data, size_t data_size);
void http_free_response(http_response *resp);