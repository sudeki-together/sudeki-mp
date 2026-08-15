#define _POSIX_C_SOURCE 200809L

#include "input/bridge_protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/joystick.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_DEVICE "/dev/input/js0"
#define DEFAULT_PORT 26760u
#define SEND_INTERVAL_MS 16u
#define MAX_AXES 64u
#define MAX_BUTTONS 512u

typedef struct ControllerMap {
    int left_x;
    int left_y;
    int right_x;
    int right_y;
    int left_trigger;
    int right_trigger;
    int dpad_x;
    int dpad_y;
    uint32_t button_bits[MAX_BUTTONS];
} ControllerMap;

static volatile sig_atomic_t stop_requested;

static void handle_signal(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

static uint64_t monotonic_ms(void) {
    struct timespec time_value;
    if (clock_gettime(CLOCK_MONOTONIC, &time_value) != 0) {
        return 0u;
    }
    return (uint64_t)time_value.tv_sec * 1000u +
        (uint64_t)time_value.tv_nsec / 1000000u;
}

static int parse_port(const char *text, unsigned int *port) {
    char *end = NULL;
    unsigned long value;
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || text[0] == '\0' || end == NULL || *end != '\0' ||
        value == 0u || value > 65535u) {
        return 0;
    }
    *port = (unsigned int)value;
    return 1;
}

static int find_axis(
    const uint8_t axis_codes[MAX_AXES],
    unsigned int axis_count,
    uint8_t wanted
) {
    unsigned int index;
    for (index = 0u; index < axis_count; ++index) {
        if (axis_codes[index] == wanted) {
            return (int)index;
        }
    }
    return -1;
}

static uint32_t button_code_to_bridge_bit(uint16_t code) {
    switch (code) {
        case BTN_SOUTH: return SUDEKIMP_BRIDGE_BUTTON_A;
        case BTN_EAST: return SUDEKIMP_BRIDGE_BUTTON_B;
        case BTN_WEST: return SUDEKIMP_BRIDGE_BUTTON_X;
        case BTN_NORTH: return SUDEKIMP_BRIDGE_BUTTON_Y;
        case BTN_TL: return SUDEKIMP_BRIDGE_BUTTON_LEFT_SHOULDER;
        case BTN_TR: return SUDEKIMP_BRIDGE_BUTTON_RIGHT_SHOULDER;
        case BTN_SELECT: return SUDEKIMP_BRIDGE_BUTTON_BACK;
        case BTN_START: return SUDEKIMP_BRIDGE_BUTTON_START;
        case BTN_THUMBL: return SUDEKIMP_BRIDGE_BUTTON_LEFT_STICK;
        case BTN_THUMBR: return SUDEKIMP_BRIDGE_BUTTON_RIGHT_STICK;
        default: return 0u;
    }
}

static void initialize_map(ControllerMap *map) {
    memset(map, 0, sizeof(*map));
    map->left_x = -1;
    map->left_y = -1;
    map->right_x = -1;
    map->right_y = -1;
    map->left_trigger = -1;
    map->right_trigger = -1;
    map->dpad_x = -1;
    map->dpad_y = -1;
}

static int discover_map(
    int device,
    ControllerMap *map,
    unsigned int *axis_count_out,
    unsigned int *button_count_out
) {
    uint8_t axis_count = 0u;
    uint8_t button_count = 0u;
    uint8_t axis_codes[MAX_AXES];
    uint16_t button_codes[MAX_BUTTONS];
    unsigned int index;
    int rx;
    int ry;
    int z;
    int rz;

    initialize_map(map);
    memset(axis_codes, 0xff, sizeof(axis_codes));
    memset(button_codes, 0, sizeof(button_codes));
    if (ioctl(device, JSIOCGAXES, &axis_count) < 0 ||
        ioctl(device, JSIOCGBUTTONS, &button_count) < 0 ||
        ioctl(device, JSIOCGAXMAP, axis_codes) < 0 ||
        ioctl(device, JSIOCGBTNMAP, button_codes) < 0) {
        return 0;
    }
    if ((unsigned int)axis_count > MAX_AXES) {
        axis_count = MAX_AXES;
    }
    map->left_x = find_axis(axis_codes, axis_count, ABS_X);
    map->left_y = find_axis(axis_codes, axis_count, ABS_Y);
    rx = find_axis(axis_codes, axis_count, ABS_RX);
    ry = find_axis(axis_codes, axis_count, ABS_RY);
    z = find_axis(axis_codes, axis_count, ABS_Z);
    rz = find_axis(axis_codes, axis_count, ABS_RZ);
    if (rx >= 0 && ry >= 0) {
        map->right_x = rx;
        map->right_y = ry;
        map->left_trigger = z;
        map->right_trigger = rz;
    } else {
        map->right_x = z;
        map->right_y = rz;
        map->left_trigger = find_axis(axis_codes, axis_count, ABS_BRAKE);
        map->right_trigger = find_axis(axis_codes, axis_count, ABS_GAS);
    }
    map->dpad_x = find_axis(axis_codes, axis_count, ABS_HAT0X);
    map->dpad_y = find_axis(axis_codes, axis_count, ABS_HAT0Y);
    for (index = 0u; index < (unsigned int)button_count; ++index) {
        map->button_bits[index] =
            button_code_to_bridge_bit(button_codes[index]);
    }
    *axis_count_out = axis_count;
    *button_count_out = button_count;
    return map->left_x >= 0 && map->left_y >= 0;
}

static int16_t read_axis(
    const int16_t values[MAX_AXES],
    int index
) {
    return index < 0 ? 0 : values[index];
}

static uint16_t read_trigger(
    const int16_t values[MAX_AXES],
    int index
) {
    int value;
    if (index < 0) {
        return 0u;
    }
    value = (int)values[index] + 32768;
    if (value < 0) {
        value = 0;
    } else if (value > 65535) {
        value = 65535;
    }
    return (uint16_t)value;
}

static void update_dpad(
    SudekiMpInputBridgeState *state,
    const int16_t values[MAX_AXES],
    const ControllerMap *map
) {
    int16_t x = read_axis(values, map->dpad_x);
    int16_t y = read_axis(values, map->dpad_y);
    state->buttons &= ~(SUDEKIMP_BRIDGE_BUTTON_DPAD_UP |
        SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN |
        SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT |
        SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT);
    if (x < -16384) {
        state->buttons |= SUDEKIMP_BRIDGE_BUTTON_DPAD_LEFT;
    } else if (x > 16384) {
        state->buttons |= SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT;
    }
    if (y < -16384) {
        state->buttons |= SUDEKIMP_BRIDGE_BUTTON_DPAD_UP;
    } else if (y > 16384) {
        state->buttons |= SUDEKIMP_BRIDGE_BUTTON_DPAD_DOWN;
    }
}

static int run_self_test(void) {
    SudekiMpInputBridgeState source;
    SudekiMpInputBridgeState decoded;
    uint8_t packet[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE];
    memset(&source, 0, sizeof(source));
    source.sequence = 7u;
    source.left_x = -1234;
    source.left_y = 2345;
    source.right_x = 32767;
    source.right_y = -32768;
    source.left_trigger = 123u;
    source.right_trigger = 65000u;
    source.buttons = SUDEKIMP_BRIDGE_BUTTON_A |
        SUDEKIMP_BRIDGE_BUTTON_DPAD_RIGHT;
    if (!SudekiMpEncodeInputBridgePacket(packet, &source) ||
        !SudekiMpDecodeInputBridgePacket(packet, sizeof(packet), &decoded) ||
        memcmp(&source, &decoded, sizeof(source)) != 0) {
        fputs("sudekimp-input-bridge: self-test failed\n", stderr);
        return 1;
    }
    puts("sudekimp-input-bridge: self-test passed");
    return 0;
}

static void usage(const char *program) {
    fprintf(stderr,
        "usage: %s [--device /dev/input/js0] [--port 26760] [--verbose]\n"
        "       %s --self-test\n",
        program,
        program);
}

int main(int argc, char **argv) {
    const char *device_path = DEFAULT_DEVICE;
    unsigned int port = DEFAULT_PORT;
    int verbose = 0;
    int device;
    int sender;
    struct sockaddr_in destination;
    ControllerMap map;
    unsigned int axis_count;
    unsigned int button_count;
    int16_t axis_values[MAX_AXES];
    SudekiMpInputBridgeState state;
    uint64_t next_send;
    uint32_t last_verbose_buttons = 0u;
    int16_t last_verbose_axes[4] = {0, 0, 0, 0};
    int index;
    char controller_name[128];

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--device") == 0 && index + 1 < argc) {
            device_path = argv[++index];
        } else if (strcmp(argv[index], "--port") == 0 && index + 1 < argc) {
            if (!parse_port(argv[++index], &port)) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[index], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[index], "--self-test") == 0) {
            return run_self_test();
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    device = open(device_path, O_RDONLY | O_NONBLOCK);
    if (device < 0) {
        fprintf(stderr, "sudekimp-input-bridge: cannot open %s: %s\n",
                device_path, strerror(errno));
        return 1;
    }
    if (!discover_map(device, &map, &axis_count, &button_count)) {
        fprintf(stderr,
            "sudekimp-input-bridge: %s has no usable left stick or its joydev map could not be read: %s\n",
            device_path,
            strerror(errno));
        close(device);
        return 1;
    }
    memset(controller_name, 0, sizeof(controller_name));
    if (ioctl(device, JSIOCGNAME(sizeof(controller_name)), controller_name) < 0) {
        strcpy(controller_name, "unknown controller");
    }
    sender = socket(AF_INET, SOCK_DGRAM, 0);
    if (sender < 0) {
        fprintf(stderr, "sudekimp-input-bridge: socket failed: %s\n",
                strerror(errno));
        close(device);
        return 1;
    }
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons((uint16_t)port);
    memset(axis_values, 0, sizeof(axis_values));
    memset(&state, 0, sizeof(state));
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    fprintf(stderr,
        "sudekimp-input-bridge: controller=\"%s\" device=%s axes=%u buttons=%u destination=127.0.0.1:%u\n",
        controller_name,
        device_path,
        axis_count,
        button_count,
        port);
    fprintf(stderr,
        "sudekimp-input-bridge: map lx=%d ly=%d rx=%d ry=%d lt=%d rt=%d dpad_x=%d dpad_y=%d\n",
        map.left_x, map.left_y, map.right_x, map.right_y,
        map.left_trigger, map.right_trigger, map.dpad_x, map.dpad_y);
    next_send = monotonic_ms();

    while (!stop_requested) {
        struct pollfd descriptor;
        struct js_event event;
        uint64_t now;
        int poll_result;
        descriptor.fd = device;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        poll_result = poll(&descriptor, 1, (int)SEND_INTERVAL_MS);
        if (poll_result < 0 && errno != EINTR) {
            fprintf(stderr, "sudekimp-input-bridge: poll failed: %s\n",
                    strerror(errno));
            break;
        }
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            fputs("sudekimp-input-bridge: controller disconnected\n", stderr);
            break;
        }
        while (read(device, &event, sizeof(event)) == (ssize_t)sizeof(event)) {
            uint8_t type = event.type & (uint8_t)~JS_EVENT_INIT;
            if (type == JS_EVENT_AXIS && event.number < MAX_AXES) {
                axis_values[event.number] = event.value;
            } else if (type == JS_EVENT_BUTTON) {
                uint32_t bit = map.button_bits[event.number];
                if (event.value != 0) {
                    state.buttons |= bit;
                } else {
                    state.buttons &= ~bit;
                }
            }
        }
        now = monotonic_ms();
        if (now < next_send) {
            continue;
        }
        state.sequence += 1u;
        state.sender_timestamp_ms = (uint32_t)now;
        state.left_x = read_axis(axis_values, map.left_x);
        state.left_y = read_axis(axis_values, map.left_y);
        state.right_x = read_axis(axis_values, map.right_x);
        state.right_y = read_axis(axis_values, map.right_y);
        state.left_trigger = read_trigger(axis_values, map.left_trigger);
        state.right_trigger = read_trigger(axis_values, map.right_trigger);
        update_dpad(&state, axis_values, &map);
        {
            uint8_t packet[SUDEKIMP_INPUT_BRIDGE_PACKET_SIZE];
            if (!SudekiMpEncodeInputBridgePacket(packet, &state) ||
                sendto(sender, packet, sizeof(packet), 0,
                       (const struct sockaddr *)&destination,
                       sizeof(destination)) != (ssize_t)sizeof(packet)) {
                fprintf(stderr, "sudekimp-input-bridge: send failed: %s\n",
                        strerror(errno));
                break;
            }
        }
        if (verbose &&
            (state.buttons != last_verbose_buttons ||
             state.left_x != last_verbose_axes[0] ||
             state.left_y != last_verbose_axes[1] ||
             state.right_x != last_verbose_axes[2] ||
             state.right_y != last_verbose_axes[3])) {
            fprintf(stderr,
                "state seq=%u left=%d,%d right=%d,%d triggers=%u,%u buttons=0x%08x\n",
                state.sequence,
                state.left_x, state.left_y,
                state.right_x, state.right_y,
                state.left_trigger, state.right_trigger,
                state.buttons);
            last_verbose_buttons = state.buttons;
            last_verbose_axes[0] = state.left_x;
            last_verbose_axes[1] = state.left_y;
            last_verbose_axes[2] = state.right_x;
            last_verbose_axes[3] = state.right_y;
        }
        next_send = now + SEND_INTERVAL_MS;
    }

    close(sender);
    close(device);
    return stop_requested ? 0 : 1;
}
