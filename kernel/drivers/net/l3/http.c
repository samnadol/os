#include "http.h"

#include "../../tty.h"
#include "../l3/dns.h"
#include "../../../hw/mem.h"
#include "../../../hw/timer.h"
#include "../../../lib/string.h"
#include "../../../lib/random.h"

typedef struct http_listener_entry
{
    uint16_t dport;
    uint16_t sport;
    bool completed;

    http_listener listener;

    struct http_listener_entry *next;
    struct http_listener_entry *prev;
} http_listener_entry;
http_listener_entry *http_listeners;

void http_free_response(http_response *resp)
{
    http_header *ll = resp->headers;
    while (ll)
    {
        http_header *current = ll;
        ll = ll->next;

        mfree(current->key);
        mfree(current->value);
        mfree(current);
    }

    mfree(resp->response_string);
    mfree(resp->data);
    mfree(resp);
}

http_response *http_parse_response(void *data, size_t data_size)
{
    http_response *ret = (http_response *)calloc(sizeof(http_response));

    char *data_nocr = strrep(data, "\r\n", "\n");
    mfree(data);

    size_t firstnlloc = strfindchar(data_nocr, '\n');
    size_t contentstart = strfindstr(data_nocr, "\n\n");

    ret->response_string = strcut(data_nocr, firstnlloc);

    char *headers = strcut(data_nocr + firstnlloc, contentstart - firstnlloc + 1);
    size_t headers_len = strlen(headers);
    size_t loc = 0;
    while (loc < strlen(headers))
    {
        size_t s = strfindchar(headers + loc, '\n');
        char *header_string = strcut(headers + loc, s);

        size_t seploc = strfindchar(header_string, ':');
        char *key = strcut(header_string, seploc);
        char *val = strcut(header_string + seploc + 1, s - seploc - 1);

        http_header *header = (http_header *)calloc(sizeof(http_header));
        header->key = key;
        header->value = val;
        header->next = 0;

        if (!(ret->headers))
            ret->headers = header;
        else
        {
            http_header *ll = ret->headers;
            while (ll->next)
                ll = ll->next;
            ll->next = header;
        }

        mfree(header_string);
        loc += s;
    }
    mfree(headers);

    if (strlen(data_nocr) == contentstart)
    {
        ret->data = 0;
    }
    else
    {
        size_t resplen = strlen(data_nocr) - contentstart - strlen("\n\n") + 1;
        char *respdata = (char *)calloc(resplen);
        memcpy(respdata, data_nocr + contentstart + strlen("\n\n"), resplen);
        respdata[resplen - 1] = 0;

        ret->data = respdata;
    }

    // // for (size_t i = 0; i < strlen(ret->data) + 1; i++)
    // //     printf("%d %c\n", ret->data[i], ret->data[i]);

    mfree(data_nocr);
    return ret;
}

bool http_response_listener(network_device *netdev, tcp_header *tcp, void *data, size_t data_len)
{
    http_listener_entry *listener_entry = http_listeners;
    while (listener_entry)
    {
        if (listener_entry->sport == tcp->dport)
            break;

        listener_entry = listener_entry->next;
    }

    dprintf(2, "[HTTP] got http response (dport %d)\n", listener_entry->sport);

    listener_entry->listener(netdev, tcp, data, data_len);

    // tcp_connection_close(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, dip, HTTP_SPORT, dport, 5000);
    // dprintf(2, "[HTTP] connection closed\n");

    tcp_uninstall_listener(listener_entry->sport);
    listener_entry->completed = true;

    return true;
}

bool http_send_request(network_device *netdev, uint32_t dip, uint16_t dport, char *host, char *path, http_listener listener)
{
    char *data = (char *)calloc(48 + strlen(host) + strlen(path));
    sprintf(data, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);

    uint32_t HTTP_SPORT = rand(0xFFF) + 40000;

    // add external listener function and necessary data to linked list so it can be looked up when data is returned async
    http_listener_entry *listener_entry = (http_listener_entry *)calloc(sizeof(http_listener_entry));
    listener_entry->dport = dport;
    listener_entry->sport = HTTP_SPORT;
    listener_entry->listener = listener;
    listener_entry->completed = false;
    listener_entry->next = 0;
    listener_entry->prev = 0;

    if (!http_listeners)
        http_listeners = listener_entry;
    else
    {
        http_listener_entry *current = http_listeners;
        while (current->next)
            current = current->next;
        current->next = listener_entry;
        listener_entry->prev = current;
    }

    tcp_install_listener(HTTP_SPORT, http_response_listener);
    dprintf(2, "[HTTP] listener installed\n");

    if (!tcp_connection_open(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, dip, HTTP_SPORT, dport, 5000))
        return false;

    dprintf(2, "[HTTP] connection established\n");

    dprintf(2, "[HTTP] sending HTTP request (sport %d)\n", HTTP_SPORT);
    if (!tcp_connection_transmit(ethernet_first_netdev(), ethernet_first_netdev()->ip_c.ip, dip, HTTP_SPORT, dport, data, strlen(data), 5000))
        return false;

    mfree(data);

    for (size_t time = 0; time < 5000; time += 10)
    {
        timer_wait(10);

        if (listener_entry->completed)
            break;
    }

    // REMOVE FROM LINKED LIST
    http_listener_entry *current = http_listeners;
    while (current)
    {
        if (current->sport == listener_entry->sport)
            break;
        current = current->next;
    }
    if (current == listener_entry)
        http_listeners = listener_entry->next;
    else
    {
        listener_entry->prev->next = listener_entry->next;
        listener_entry->next->prev = listener_entry->prev;
    }
    mfree(listener_entry);

    return true;
}