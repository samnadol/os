#include "http.h"

#include "../../tty.h"
#include "../l3/dns.h"
#include "../../../hw/mem.h"
#include "../../../hw/timer.h"
#include "../../../lib/string.h"
#include "../../../lib/random.h"

bool http_send_request(network_device *netdev, uint32_t dip, uint16_t dport, char *host, char *path, tcp_listener listener)
{
    char *data = (char *)calloc(48 + strlen(host) + strlen(path));
    sprintf(data, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n", path, host);

    uint32_t HTTP_SPORT = rand(0xFFF) + 40000;

    tcp_install_listener(HTTP_SPORT, listener);
    dprintf("[HTTP] listener installed\n");

    if (!tcp_connection_open(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, dip, HTTP_SPORT, dport, 5000))
        return false;

    // send http data to tcpbin.net
    // tcp_connection_open(get_first_netdev(), get_first_netdev()->ip_c.ip, 0x2ee26a7f, HTTP_SPORT + 1, 44667, 5000); 
    // tcp_connection_transmit(get_first_netdev(), get_first_netdev()->ip_c.ip, 0x2ee26a7f, HTTP_SPORT + 1, 44667, data, strlen(data), 5000);
    // tcp_connection_close(get_first_netdev(), get_first_netdev()->ip_c.ip, 0x2ee26a7f, HTTP_SPORT + 1, 44667, 5000);

    dprintf("[HTTP] connection established\n");

    dprintf("[HTTP] sending HTTP request\n");
    if (!tcp_connection_transmit(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, dip, HTTP_SPORT, dport, data, strlen(data), 5000))
        return false;
    mfree(data);

    for (int i = 0; i < 50; i++)
        timer_wait(10);

    tcp_uninstall_listener(HTTP_SPORT);
    // tcp_connection_close(get_first_netdev(), get_first_netdev()->ip_c.ip, dip, HTTP_SPORT, dport, 5000);
    dprintf("[HTTP] connection closed\n");

    return true;
}