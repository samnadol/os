#include "time.h"

#include "./dns.h"
#include "../l2/udp.h"
#include "../../../hw/timer.h"
#include "../../../lib/arpa/inet.h"

uint32_t epoch = 0;

uint32_t time_unix_epoch()
{
    return epoch + timer_get_epoch();
}

void time_receive(network_device *netdev, ip_header *ip, udp_header *udp, void *data, size_t data_size)
{
    epoch = ntohl(*((uint32_t *)data)) - 2208985200;
    udp_uninstall_listener(5000);
}

bool time_request(network_device *netdev)
{
    dns_answer *ans = dns_get_ip(netdev, netdev->ip_c.dns, "time.nist.gov", 1000);
    if (ans)
    {
        uint32_t ip = (ans->data[0] << 24) | (ans->data[1] << 16) | (ans->data[2] << 8) | (ans->data[3] << 0);
        udp_install_listener(5000, time_receive);
        uint32_t old_epoch = epoch;
        udp_send_packet(netdev, netdev->ip_c.ip, 5000, ip, 37, "TIME", 5);

        uint32_t timeout = 0;
        while (old_epoch == epoch)
        {
            if (timeout > 1000)
                return false;
            timeout += 10;
            timer_wait(10);
        }
        return true;
    }
    return false;
}