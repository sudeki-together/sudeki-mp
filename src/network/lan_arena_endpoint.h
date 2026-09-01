#ifndef SUDEKIMP_LAN_ARENA_ENDPOINT_H
#define SUDEKIMP_LAN_ARENA_ENDPOINT_H

#include <stddef.h>
#include <stdint.h>

/* Strict direct-LAN endpoint parser. DNS, discovery, whitespace, URI syntax,
 * and implicit privileged ports are deliberately excluded from version 1. */
int SudekiMpLanArenaParseEndpoint(
    const char *text,
    uint16_t default_port,
    char *ipv4,
    size_t ipv4_capacity,
    uint16_t *port
);

#endif
