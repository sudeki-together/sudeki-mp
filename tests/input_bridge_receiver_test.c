#include <winsock2.h>
#include <ws2tcpip.h>

#include "input/bridge_protocol.h"
#include "input/bridge_receiver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "input_bridge_receiver_test: %s\n", message);
        return 0;
    }
    return 1;
}

static int send_packet(
    SOCKET sender,
    unsigned int port,
    const uint8_t *packet,
    int size
) {
    struct sockaddr_in destination;
    ZeroMemory(&destination, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons((u_short)port);
    return sendto(sender, (const char *)packet, size, 0,
                  (const struct sockaddr *)&destination,
                  sizeof(destination));
}

static int run_external_sender_test(unsigned int port) {
    SudekiMpInputBridgeState received;
    DWORD deadline;

    if (!require(SudekiMpInputBridgeStart(port, 500u),
                 "external receiver start failed")) {
        return 1;
    }
    deadline = GetTickCount() + 3000u;
    while (!SudekiMpInputBridgePoll(&received) &&
           (LONG)(GetTickCount() - deadline) < 0) {
        Sleep(1u);
    }
    if (!require((LONG)(GetTickCount() - deadline) < 0,
                 "no Linux bridge packet received")) {
        SudekiMpInputBridgeStop();
        return 1;
    }
    printf(
        "external input bridge packet received: sequence=%lu left=%d,%d right=%d,%d buttons=0x%08lx\n",
        (unsigned long)received.sequence,
        (int)received.left_x,
        (int)received.left_y,
        (int)received.right_x,
        (int)received.right_y,
        (unsigned long)received.buttons
    );
    SudekiMpInputBridgeStop();
    return 0;
}

int main(int argc, char **argv) {
    WSADATA data;
    SOCKET sender;
    SudekiMpInputBridgeState source;
    SudekiMpInputBridgeState received;
    uint8_t packet[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE];
    unsigned int port;
    DWORD deadline;

    if (argc == 3 && strcmp(argv[1], "--external") == 0) {
        char *end = NULL;
        unsigned long external_port = strtoul(argv[2], &end, 10);
        if (argv[2][0] == '\0' || end == NULL || *end != '\0' ||
            external_port < 1024u || external_port > 65535u) {
            fputs("input_bridge_receiver_test: invalid external port\n",
                  stderr);
            return 2;
        }
        return run_external_sender_test((unsigned int)external_port);
    }
    if (argc != 1) {
        fputs("usage: SudekiMP.InputBridgeReceiverTest.exe [--external port]\n",
              stderr);
        return 2;
    }

    if (!require(SudekiMpInputBridgeStart(0u, 60u),
                 "receiver start failed")) {
        return 1;
    }
    port = SudekiMpInputBridgeBoundPort();
    if (!require(port != 0u, "ephemeral port unavailable") ||
        !require(WSAStartup(MAKEWORD(2, 2), &data) == 0,
                 "sender WSAStartup failed")) {
        SudekiMpInputBridgeStop();
        return 1;
    }
    sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!require(sender != INVALID_SOCKET, "sender socket failed")) {
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    ZeroMemory(&source, sizeof(source));
    source.sequence = 42u;
    source.sender_timestamp_ms = 500u;
    source.left_x = 12000;
    source.left_y = -22000;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_A;
    SudekiMpEncodeInputBridgePacket(packet, &source);
    if (!require(send_packet(sender, port, packet, sizeof(packet)) ==
                     (int)sizeof(packet),
                 "valid send failed")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    deadline = GetTickCount() + 250u;
    while (!SudekiMpInputBridgePoll(&received) &&
           (LONG)(GetTickCount() - deadline) < 0) {
        Sleep(1u);
    }
    if (!require(received.sequence == source.sequence,
                 "valid packet not received") ||
        !require(received.left_x == source.left_x &&
                     received.left_y == source.left_y,
                 "axis state mismatch") ||
        !require(received.buttons == source.buttons,
                 "button state mismatch")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    Sleep(80u);
    if (!require(!SudekiMpInputBridgePoll(&received),
                 "stale packet remained connected") ||
        !require(received.buttons == 0u && received.left_x == 0,
                 "stale state was not neutralized")) {
        closesocket(sender);
        WSACleanup();
        SudekiMpInputBridgeStop();
        return 1;
    }

    closesocket(sender);
    WSACleanup();
    SudekiMpInputBridgeStop();
    puts("input bridge receiver tests passed");
    return 0;
}
