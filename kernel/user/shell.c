#include "shell.h"

#include "../hw/mem.h"
#include "../hw/pci.h"
#include "../drivers/vga/vga.h"
#include "../lib/string.h"
#include "../hw/cpu/irq.h"
#include "../hw/cpu/cpuid.h"
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
#include "scheduler.h"
#include "gui/gui.h"

bool typing_enabled = false;

typedef struct arg
{
    char *val;
    struct arg *next;
} arg;

/*
 * HELPER FUNCTIONS
 */

const unsigned long hash(const char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/*
 * NETWORK LISTENERS
 */

bool echo_semaphore;
void echo_listener(network_device *netdev, ip_header *ip, udp_header *udp, void *data, size_t data_size)
{
    printf("[ECHO] GOT ECHO: ");
    for (int i = 0; i < data_size; i++)
        printf("%d ", ((char *)data)[i]);
    printf("\n");
    udp_uninstall_listener(10000);
    echo_semaphore = true;
}

bool mock_http_recieve(network_device *driver, tcp_header *tcp, void *data, size_t data_size)
{
    http_response *resp = http_parse_response(data, data_size);

    printf("%s\n", resp->data);

    http_free_response(resp);

    return true;
}

/*
 * COMMAND PROCESSING
 */

size_t num_args(arg *base)
{
    size_t num = 1;
    arg *current = base;
    while (current->next)
    {
        current = current->next;
        num++;
    }
    return num;
}
void process_command(tty_interface *tty)
{
    // mem_print();
    typing_enabled = false;
    dprintf("[SHL] Command processing started\n");

    arg *args = 0;
    uint8_t last_space = 1;
    size_t *space_locations = (size_t *)calloc(sizeof(size_t) * 32);
    space_locations[0] = -1;

    for (size_t i = 0; i < strlen(tty->keybuffer) + 1; i++)
    {
        if (tty->keybuffer[i] == ' ' || tty->keybuffer[i] == '\0')
        {
            space_locations[last_space] = i;
            last_space++;

            if (space_locations[last_space - 1] - space_locations[last_space - 2] + 1 > 1)
            {
                // printf("arg from %d to %d, len %d\n", space_locations[last_space - 2] + 1, space_locations[last_space - 1], space_locations[last_space - 1] - space_locations[last_space - 2] - 1);

                char *str = (char *)calloc(space_locations[last_space - 1] - space_locations[last_space - 2] + 1);
                memcpy(str, tty->keybuffer + space_locations[last_space - 2] + 1, space_locations[last_space - 1] - space_locations[last_space - 2] - 1);

                arg *new = (arg *)calloc(sizeof(arg));
                new->val = str;
                new->next = 0;

                if (!args)
                    args = new;
                else
                {
                    arg *current = args;
                    while (current->next)
                        current = current->next;
                    current->next = new;
                }
            }

            if (tty->keybuffer[i] == '\0')
                break;
        }
    }
    mfree(space_locations);

    switch (hash(args[0].val))
    {
    case COMMAND_CPU:
        tprintf(tty, "%s %s\n", cpuinfo.vendor, cpuinfo.model);
        break;
    case COMMAND_MEM:
        mem_print(tty);
        break;
    case COMMAND_PCI:
        pci_print(tty);
        break;
    case COMMAND_IRQ:
        irq_print(tty);
        break;
    case COMMAND_ARP:
        arp_print(tty);
        break;
    case COMMAND_ICMP:
        for (int i = 0; i < 4; i++)
        {
            if (i > 0)
                timer_wait(1000);
            icmp_send_packet(ethernet_first_netdev(), ip_to_uint(1, 1, 1, 1), i);
        }
        break;
    case COMMAND_IP:
        network_device *netdev = ethernet_first_netdev();
        tprintf(tty, "IP: %i\nNetmask: %i\nGateway: %i\nDNS: %i\nDHCP: %i\n", netdev->ip_c.ip, netdev->ip_c.netmask, netdev->ip_c.gateway, netdev->ip_c.dns, netdev->ip_c.dhcp);
        break;
    case COMMAND_DNS:
        dns_print(tty);
        break;
    case COMMAND_ECHO:
        echo_semaphore = false;
        char *data = (char *)calloc(10);
        for (int i = 0; i < 10; i++)
            data[i] = i;
        udp_send_packet(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, 10000, ip_to_uint(52, 43, 121, 77), 10001, data, 10);
        mfree(data);
        udp_install_listener(10000, echo_listener);

        size_t timer = 0;
        while (!echo_semaphore && timer < 10000)
        {
            timer_wait(10);
            timer += 10;
        }
        break;
    case COMMAND_DNSREQ:
        if (num_args(args) != 2)
        {
            tprintf(tty, "Invalid number of arguments\n");
            break;
        }

        dns_answer *res = dns_get_ip(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.dns, args->next->val, 10000);
        if (res)
        {
            if (res->name_exists)
                tprintf(tty, "%d.%d.%d.%d\n", res->data[0], res->data[1], res->data[2], res->data[3]);
            else
                tprintf(tty, "name does not exist\n");
        }
        else
            tprintf(tty, "no response\n");
        break;
    case COMMAND_HTTP:
        if (num_args(args) != 2)
        {
            tprintf(tty, "Invalid number of arguments\n");
            break;
        }

        uint32_t ip = 0;
        dns_answer *ans = dns_get_ip(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.dns, args->next->val, 5000);
        if (ans)
        {
            if (ans->name_exists)
            {
                ip = (ans->data[0] << 24) | (ans->data[1] << 16) | (ans->data[2] << 8) | (ans->data[3] << 0);
                http_send_request(ethernet_first_netdev(), ip, 80, args->next->val, "/", &mock_http_recieve); // blocks until data is recieved or times out
            }
            else
            {
                tprintf(tty, "[HTTP] domain does not exist\n");
            }
        }
        else
        {
            tprintf(tty, "[HTTP] dns request timed out\n");
        }
        break;
    case COMMAND_GUI:
        if (tty->type == TTYType_VGA)
        {
            vga_switch_mode(VGA_GUI);
            gui_init();
        }
        else
        {
            tprintf(tty, "This terminal is not on a VGA display, cannot switch to GUI.\n");
        }
        break;
    case COMMAND_DHCP:
        network_device *dev = ethernet_first_netdev();
        if (strcmp(args->next->val, "release") == 0)
            if (dev->ip_c.ip)
                dhcp_configuration_release(dev);
            else
                tprintf(tty, "Don't have an IP to release!\n");
        else if (strcmp(args->next->val, "request") == 0)
            if (!dev->ip_c.ip)
                dhcp_configuration_request(dev, 3000);
            else
                tprintf(tty, "Already have an IP!\n");
        else
            printf("Unknown subcommand!\n");
        break;
    case COMMAND_HELP:
        tprintf(tty, "COMMAND_CPU = 0xB8866ED,\nCOMMAND_MEM = 0xB889004,\nCOMMAND_PCI = 0xB889C81,\nCOMMAND_IRQ = 0xB8880B1,\nCOMMAND_ARP = 0xB885EA8,\nCOMMAND_ICMP = 0x7C9856EE,\nCOMMAND_IP = 0x59783E,\nCOMMAND_DNS = 0xB886AEA,\nCOMMAND_ECHO = 0x7C9624C4,\nCOMMAND_DNSREQ = 0xF92A6D12,\nCOMMAND_HTTP = 0x7C9813C5,\nCOMMAND_GUI = 0xB88788A,\nCOMMAND_DHCP = 0x7C9E690A,\nCOMMAND_HELP = 0x7C97D2EE");
        tprintf(tty, "\n");
        break;
    case COMMAND_MEMLEAK:
        mem_print_blocks(tty);
        break;
    default:
        tprintf(tty, "UNKNOWN COMMAND %s (hash 0x%x)\n", args[0].val, hash(args[0].val));
    }

    arg *current = args;
    arg *next = 0;
    while (current)
    {
        next = current->next;
        mfree(current->val);
        mfree(current);
        current = next;
    }
    // mem_print();

    tprintf(tty, "> ");
    tty->keybuffer[0] = '\0';

    dprintf("[SHL] Command processing done\n");
    typing_enabled = true;
}

/*
 * EXTERNAL CONTROL
 */

void shell_init()
{
    // printf("0x%x\n", hash("dhcp"));
    printf("> ");
    typing_enabled = true;
}

void shell_add_key(tty_interface *tty, char letter)
{
    if (!typing_enabled)
        return;

    strappend(tty->keybuffer, letter);

    char s[2];
    s[0] = letter;
    s[1] = 0;
    tprintf(tty, s);
}

bool shell_backspace(tty_interface *tty)
{
    if (!typing_enabled)
        return false;

    // printf("backspace\n");

    int len = strlen(tty->keybuffer);
    if (len > 0)
    {
        tty->keybuffer[len - 1] = '\0';
        tty->tty_del_char();
        return true;
    }
    else
    {
        return false;
    }
}

bool ctrlc = false;
void shell_control(tty_interface *tty, char c, bool shift)
{
    tprintf(tty, "CTRL+%s%c\n", shift ? "SHIFT+" : "", c);
    if (!shift && c == 'c')
        ctrlc = true;
}

bool is_ctrlc()
{
    if (ctrlc)
    {
        ctrlc = false;
        return true;
    }
    return false;
}

void shell_enter(tty_interface *tty)
{
    if (!typing_enabled)
        return;

    tprintf(tty, "\n");
    scheduler_add_task(&process_command, tty);
}