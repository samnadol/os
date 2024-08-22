#pragma once

#include <stdint.h>

#include "../l0/ethernet.h"
#include "../l2/tcp.h"

bool http_send_request(network_device *netdev, uint32_t dip, uint16_t dport, char *host, char *path, tcp_listener listener);