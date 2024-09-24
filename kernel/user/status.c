#include "status.h"

#include "../drivers/vga/vga.h"
#include "../drivers/vga/modes/text.h"
#include "../drivers/net/l0/ethernet.h"
#include "../lib/string.h"
#include "../hw/cpu/cpuid.h"
#include "../drivers/tty.h"
#include "../hw/mem.h"
#include "../hw/timer.h"

#include "../drivers/net/l0/ethernet.h"
#include "../drivers/net/l1/arp.h"
#include "../drivers/net/l1/ip.h"
#include "../drivers/net/l2/icmp.h"
#include "../drivers/net/l2/udp.h"
#include "../drivers/net/l2/tcp.h"
#include "../drivers/net/l3/dns.h"
#include "../drivers/net/l3/http.h"
#include "../drivers/net/l3/dhcp.h"

char *buffer;

void status_write(uint8_t x, uint8_t y, char *ptr)
{
    int i;
    for (i = x; i < (x + strlen(ptr)); i++)
        vga_setc(ptr[i - x], i, y, VGA_TEXT_WHITE_ON_GREEN);
    for (; i < 80; i++)
        vga_setc(' ', i, y, VGA_TEXT_WHITE_ON_BLACK);
}

// uint32_t wan_ip = 0;
// bool wan_ip_recieve(network_device *driver, tcp_header *tcp, void *data, size_t data_size)
// {
//     wan_ip = 0;
//     http_response *resp = http_parse_response(data, data_size);

//     char *use = resp->data;
//     for (int i = 0; i < 3; i++)
//     {
//         size_t loc = strfindchar(use, '.');
//         char *str = strcut(use, loc);
//         wan_ip += (atoi(str) << (24 - (i*8)));
//         use += loc;
//         mfree(str);
//     }
//     wan_ip += atoi(use);

//     http_free_response(resp);

//     return true;
// }

// void status_update_wan_ip()
// {
//     uint32_t ip = 0;
//     dns_answer *ans = dns_get_ip(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.dns, "ifconfig.me", 5000);
//     if (ans)
//     {
//         if (ans->name_exists)
//         {
//             ip = (ans->data[0] << 24) | (ans->data[1] << 16) | (ans->data[2] << 8) | (ans->data[3] << 0);
//             http_send_request(ethernet_first_netdev(), ip, 80, "ifconfig.me", "/", &wan_ip_recieve);
//         }
//     }
// }

void status_update()
{
    memset(buffer, 0, 79);
    mem_stats mem = mem_get_stats();
    // sprintf(buffer, "os uptime: %ds lan: %i wan: %i %d / %d (%d%%)", (uint32_t)timer_get_epoch(), ethernet_first_netdev() ? ethernet_first_netdev()->ip_c.ip : 0, wan_ip, mem.used, mem.total, (mem.used * 100) / mem.total);
    sprintf(buffer, "os uptime: %ds lan: %i %d / %d (%d%%)", (uint32_t)timer_get_epoch(), ethernet_first_netdev() ? ethernet_first_netdev()->ip_c.ip : 0, mem.used, mem.total, (mem.used * 100) / mem.total);
    // sprintf(buffer, "os %f", mem.used);
    status_write(0, 0, buffer);
}

void status_init()
{
    buffer = (char *)calloc(79 * sizeof(char));
}