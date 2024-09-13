#include "http.h"

#include "../../tty.h"
#include "../l3/dns.h"
#include "../../../hw/mem.h"
#include "../../../hw/timer.h"
#include "../../../lib/string.h"
#include "../../../lib/random.h"

void http_free_response(http_response *resp)
{
    mfree(resp->response_string);
    mfree(resp->data);
    mfree(resp);
}

http_response *http_parse_response(void *data, size_t data_size)
{
    http_response *ret = (http_response *)calloc(sizeof(http_response));
    size_t firstnlloc = strfindchar(data, '\n');
    size_t contentstart = strfindstr(data, "\r\n\r\n");

    ret->response_string = strcut(data, firstnlloc);

    size_t resplen = strlen(data) - contentstart - strlen("\r\n\r\n");
    char *respdata = (char *)calloc(resplen);
    memcpy(respdata, data + contentstart + strlen("\r\n\r\n"), resplen);
    respdata[resplen + 1] = 0;

    ret->data = strrep(respdata, "\r\n", "\n");
    
    printf("%s\n", ret->data);
    // for (size_t i = 0; i < strlen(ret->data) + 1; i++)
    //     printf("%d %c\n", ret->data[i], ret->data[i]);

    mfree(respdata);
    mfree(data);

    return ret;
}

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
    // tcp_connection_open(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, 0x2ee26a7f, HTTP_SPORT + 1, 50445, 5000);
    // tcp_connection_transmit(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, 0x2ee26a7f, HTTP_SPORT + 1, 50445, data, strlen(data), 5000);
    // tcp_connection_close(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, 0x2ee26a7f, HTTP_SPORT + 1, 50445, 5000);

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