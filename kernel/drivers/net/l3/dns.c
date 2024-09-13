#include "dns.h"

#include "../l2/udp.h"
#include "../../../lib/string.h"
#include "../../../lib/arpa/inet.h"
#include "../../../hw/mem.h"
#include "../../../hw/timer.h"
#include "../../../user/shell.h"

size_t dns_id;

#define DNS_PORT 33242
#define DNS_CACHE_SIZE 32

dns_answer *dns_cache;

void dns_cache_remove(dns_answer *ans)
{
    if (ans == dns_cache) // if first entry, then set first entry to next entry
        dns_cache = ans->next;
    else // if not first entry, then set previous entry's next to next
        ans->prev->next = ans->next;
    ans->next->prev = ans->prev;

    mfree(ans->domain);
    mfree(ans);
}

dns_answer *dns_cache_get(network_device *netdev, uint32_t dns_server_ip, char *domain, size_t timeout)
{
    dns_answer *current = dns_cache;
    while (current)
    {
        if (strcmp(current->domain, domain) == 0)
        {
            // domain matches, check ttl expiry
            if ((uint32_t)(timer_get_epoch() - current->created) > current->ttl)
            {
                // expired
                dprintf(3, "[DNS] %s in cache but expired, removing\n", domain);
                dns_cache_remove(current);
            }
            else
            {
                dprintf(3, "[DNS] in cache\n");
                if (!(current->name_exists))
                    return current;
                else
                    switch (current->type)
                    {
                    case DNS_TYPE_A:
                        return current;
                        break;
                    case DNS_TYPE_CNAME:
                        return dns_get_ip(netdev, netdev->ip_c.dns, (char *)current->data, timeout);
                        break;
                    }
            }
        }
        current = current->next;
    }

    return 0;
}

void dns_cache_add(dns_answer *ans)
{
    if (!dns_cache)
    {
        dns_cache = ans;
    }
    else
    {
        dns_answer *current = dns_cache;
        while (current->next)
            current = current->next;
        ans->prev = current;
        current->next = ans;
    }
}

bool dns_send_query(network_device *netdev, uint32_t dns_server_ip, const char *domain_name, int type)
{
    strlower((char *)domain_name);

    dns_header *packet = (dns_header *)calloc(sizeof(dns_header) + 1 + strlen(domain_name) + 1 + sizeof(dns_query));
    packet->transaction_id = htons(dns_id++);

    packet->flags.qr = DNS_MESSAGE_QUERY;
    packet->flags.opcode = DNS_OPCODE_QUERY;
    packet->flags.aa = 0;
    packet->flags.tc = 0;
    packet->flags.rd = 1;
    packet->flags.ra = 0;
    packet->flags.z = 0;
    packet->flags.rcode = 0;
    packet->flags.raw = htons(packet->flags.raw);

    packet->num_questions = htons(1);
    packet->num_answers = htons(0);
    packet->num_auth_rrs = htons(0);
    packet->num_add_rrs = htons(0);

    for (int i = 0; i < strlen(domain_name); i++)
    {
        ((uint8_t *)packet + sizeof(dns_header) + 1)[i] = domain_name[i];
    }

    uint8_t *domain_ptr = (uint8_t *)packet + sizeof(dns_header);
    for (int i = 0, len = 0; i <= strlen(domain_name); i++, len++)
    {
        if (i == strlen(domain_name) || domain_name[i] == '.')
        {
            *domain_ptr = len;
            domain_ptr += len + 1;
            len = -1;
        }
    }

    dns_query *query = (dns_query *)((uint8_t *)packet + 1 + sizeof(dns_header) + strlen(domain_name) + 1);
    query->type = htons(type);
    query->class = htons(DNS_CLASS_IN);

    dprintf(3, "[DNS] sent query for domain %s\n", domain_name);

    bool res = udp_send_packet(netdev, netdev->ip_c.ip, DNS_PORT, dns_server_ip, 53, packet, sizeof(dns_header) + 1 + strlen(domain_name) + 1 + 4);
    mfree(packet);
    return res;
}

size_t dns_response_read_domain(uint8_t *data, size_t start_offset, uint8_t **buffer)
{
    bool physical_len_done = false;
    size_t physical_len = 0; // length of data in response (num char + physical size of pointer (2 byte))
    size_t string_len = 0;   // length of data, calculated (num char total, following all pointers)
    size_t i = start_offset;

    // calculate lengths, must do this before running string assembly or we won't know the length of the required string
    // inefficient to run this same loop & pointer jumping twice, but unless we want the possibility of buffer overflow if domain name is longer than buffer, required.
    // per spec max length of domain is 253 char, but this keeps the malloc to the least size possible at the cost of a very small performance hit.

    while (data[i] != '\0')
    {
        if ((((data[i] << 8) | data[i + i]) & 0xC000) != 0)
        {
            // pointer in response (string shortening)
            if (!physical_len_done)
                physical_len += 1;
            physical_len_done = true;
            i = (((data[i] << 8) | data[i + 1]) & 0x3FFF) - sizeof(dns_header); // set i to pointed address, continue from that address
        }
        else
        {
            // normal string
            if (!physical_len_done)
                physical_len += data[i] + 1;
            string_len += data[i] + 1;
            i += data[i] + 1; // increment i; next character
        }
    }

    *buffer = (uint8_t *)calloc(string_len);
    i = start_offset;

    // assemble string
    size_t str_pos = 0;
    while (data[i] != '\0')
    {
        if ((((data[i] << 8) | data[i + i]) & 0xC000) != 0)                     // per spec, if top two bits are 1, is pointer
            i = (((data[i] << 8) | data[i + 1]) & 0x3FFF) - sizeof(dns_header); // jump to pointed address
        else
        {
            for (int j = 1; j < data[i] + 1; j++)
                (*buffer)[str_pos++] = data[i + j]; // add chars from string to buffer
            (*buffer)[str_pos++] = '.';             // add dot domain separator after all chars from this portion of domain
            i += data[i] + 1;
        }
    }
    (*buffer)[string_len - 1] = 0; // null terminate domain name
    strlower((char *)*buffer);

    return physical_len; // return length of characters traversed from start_offset, not inluding pointer jumps, but including the actual pointer lengths
}

void dns_handle_response(network_device *dev, dns_header *header, size_t data_len)
{
    dprintf(3, "[DNS] got response, num q: %d, num a: %d\n", header->num_questions, header->num_answers);

    if (header->flags.rcode != DNS_RCODE_NO_ERROR && header->flags.rcode != DNS_RCODE_NO_SUCH_NAME)
        return; // return if error other than name not existing

    uint8_t *qna = (uint8_t *)calloc(data_len + 1);
    memcpy(qna, (uint8_t *)header + sizeof(dns_header), data_len); // copy useful dns data to new buffer

    size_t i = 0;
    for (size_t current_question = 0; current_question < header->num_questions; current_question++)
    {
        uint8_t *domain = 0;
        i += dns_response_read_domain(qna, i, &domain) + 1;

        if (header->flags.rcode == DNS_RCODE_NO_SUCH_NAME)
        {
            dns_answer *ans = (dns_answer *)calloc(sizeof(dns_answer));
            ans->domain = (char *)domain;
            dprintf(3, "[DNS] got NSN response for %s\n", ans->domain);
            ans->created = timer_get_epoch();
            ans->ttl = 86400;
            ans->next = 0;
            ans->name_exists = false;
            dns_cache_add(ans);
        }

        // unused, uncomment to use and command i+=4;
        // uint16_t type = (qna[i] << 8) | (qna[i + 1]);
        // i += 2;
        // uint16_t class = (qna[i] << 8) | (qna[i + 1]);
        // i += 2;
        i += 4;
    }
    for (size_t current_answer = 0; current_answer < header->num_answers; current_answer++)
    {
        uint8_t *domain = 0;
        i += dns_response_read_domain(qna, i, &domain) + 1;

        dns_answer *ans = (dns_answer *)calloc(sizeof(dns_answer));
        ans->domain = (char *)domain;
        ans->created = timer_get_epoch();
        ans->next = 0;

        memcpy(ans, qna + i, 10); // copy type, class, ttl, and len into dns_answer struct
        i += 10;

        ans->type = ntohs(ans->type);
        ans->class = ntohs(ans->class);
        ans->ttl = ntohl(ans->ttl) + 1;
        ans->len = ntohs(ans->len);
        ans->name_exists = true;

        ans->data = (uint8_t *)calloc(ans->len);
        switch (ans->type)
        {
        case DNS_TYPE_A:
            memcpy(ans->data, qna + i, ans->len);
            break;
        case DNS_TYPE_CNAME:
            dns_response_read_domain(qna, i, &(ans->data));
            break;
        }
        i += ans->len;

        switch (ans->type)
        {
        case DNS_TYPE_A:
            dprintf(3, "[DNS] got     A response for %s: %d.%d.%d.%d type: %x class: %x ttl: %d\n", ans->domain, ans->data[0], ans->data[1], ans->data[2], ans->data[3], ans->type, ans->class, ans->ttl);
            break;
        case DNS_TYPE_CNAME:
            dprintf(3, "[DNS] got CNAME response for %s: %s type: %x class: %x ttl: %d\n", ans->domain, ans->data, ans->type, ans->class, ans->ttl);
            break;
        }
        dns_cache_add(ans);
    }
    mfree(qna);
}

void dns_udp_listener(network_device *netdev, ip_header *ip, udp_header *udp, void *data, size_t data_size)
{
    dns_header *header = (dns_header *)data;
    header->transaction_id = ntohs(header->transaction_id);
    header->flags.raw = ntohs(header->flags.raw);
    header->num_questions = ntohs(header->num_questions);
    header->num_answers = ntohs(header->num_answers);
    header->num_auth_rrs = ntohs(header->num_auth_rrs);
    header->num_add_rrs = ntohs(header->num_add_rrs);

    if (header->flags.qr != DNS_MESSAGE_RESPONSE)
        return;

    dns_handle_response(netdev, header, data_size);
}

void dns_print(tty_interface *tty)
{
    tprintf(tty, "Current DNS Cache:\n");

    dns_answer *current = dns_cache;
    while (current)
    {
        if (!(current->name_exists))
            tprintf(tty, "   NSN %s: ttl: %d\n", current->domain, current->ttl);
        else
            switch (current->type)
            {
            case DNS_TYPE_A:
                tprintf(tty, "     A %s: %d.%d.%d.%d type: %x class: %x ttl: %d\n", current->domain, current->data[0], current->data[1], current->data[2], current->data[3], current->type, current->class, current->ttl);
                break;
            case DNS_TYPE_CNAME:
                tprintf(tty, " CNAME %s: %s type: %x class: %x ttl: %d\n", current->domain, current->data, current->type, current->class, current->ttl);
                break;
            }
        tprintf(tty, "\tcreated %d, expires %d, now %d\n", (uint32_t)current->created, (uint32_t)(current->created + current->ttl), (uint32_t)timer_get_epoch());
        current = current->next;
    }
}

dns_answer *dns_get_ip(network_device *netdev, uint32_t dns_server_ip, char *domain, size_t timeout)
{
    dns_answer *cached = dns_cache_get(netdev, dns_server_ip, domain, timeout);

    if (cached)
        return cached;

    dprintf(3, "[DNS] %s not in cache\n", domain);
    if (!dns_send_query(netdev, dns_server_ip, domain, DNS_TYPE_A))
        return 0;

    for (size_t time = 0; !is_ctrlc() && time < timeout; time += 10)
    {
        timer_wait(10);

        dns_answer *check = dns_cache_get(netdev, dns_server_ip, domain, timeout);
        if (check)
            return check;
    }

    return 0;
}

void dns_init()
{
    dns_id = 0x1;
    udp_install_listener(DNS_PORT, dns_udp_listener);
}