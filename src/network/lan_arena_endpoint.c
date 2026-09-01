#include "network/lan_arena_endpoint.h"

#include <stdio.h>
#include <string.h>

int SudekiMpLanArenaParseEndpoint(
    const char *text,
    uint16_t default_port,
    char *ipv4,
    size_t ipv4_capacity,
    uint16_t *port
) {
    unsigned int octets[4];
    unsigned int parsed_port = default_port;
    int consumed = 0;
    int count;
    int has_explicit_port;
    char canonical[16];
    if (text == NULL || ipv4 == NULL || ipv4_capacity == 0u || port == NULL ||
        default_port < 1024u) return 0;
    count = sscanf(text, "%u.%u.%u.%u:%u%n",
        &octets[0], &octets[1], &octets[2], &octets[3],
        &parsed_port, &consumed);
    has_explicit_port = count == 5;
    if (!has_explicit_port) {
        consumed = 0;
        count = sscanf(text, "%u.%u.%u.%u%n",
            &octets[0], &octets[1], &octets[2], &octets[3], &consumed);
        parsed_port = default_port;
    }
    if ((!has_explicit_port && count != 4) || consumed < 0 ||
        text[consumed] != '\0' ||
        parsed_port < 1024u || parsed_port > 65535u ||
        octets[0] > 255u || octets[1] > 255u ||
        octets[2] > 255u || octets[3] > 255u) return 0;
    snprintf(canonical, sizeof(canonical), "%u.%u.%u.%u",
        octets[0], octets[1], octets[2], octets[3]);
    if (strlen(canonical) + 1u > ipv4_capacity) return 0;
    strcpy(ipv4, canonical);
    *port = (uint16_t)parsed_port;
    return 1;
}
