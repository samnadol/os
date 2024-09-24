#pragma once

#include "../l0/ethernet.h"

bool time_request(network_device *netdev);
uint32_t time_unix_epoch();