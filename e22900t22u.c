
// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

/*
 * E22-900T22U Connector
 *
 * This program connects to an E22-900T22U LoRa module via serial port,
 * switches to configuration mode, reads its configuration registers in configuration mode, and updates
 * them as needed to match the desired configuration, then switches back to transmission mode.
 */

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printf_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}
void printf_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

#define PRINTF_DEBUG printf_debug
#define PRINTF_ERROR printf_error

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include "serial.h"
#undef E22900T22_SUPPORT_MODULE_DIP
#define E22900T22_SUPPORT_MODULE_USB
#include "e22900t22.h"

serial_config_t serial_config = {
    .port = "/dev/e22900t22u",
    .rate = 9600,
    .bits = SERIAL_8N1,
};

e22900t22_config_t e22900t22u_config = {
    .address = 0x0008,
    .network = 0x00,
    .channel = 0x17, // Channel 23 (868.125 + 23 = 891.125 MHz)
    .listen_before_transmit = false,
    .rssi_packet = true,
    .rssi_channel = true,
    .debug = false,
};

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

volatile bool is_active = true;

void signal_handler(int sig __attribute__((unused))) {
    if (is_active) {
        PRINTF_DEBUG("e22900t22u: stopping\n");
        is_active = false;
    }
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    // setbuf(stdout, NULL);
    PRINTF_DEBUG("e22900t22u: starting\n");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    if (!device_connect(E22900T22_MODULE_USB, &e22900t22u_config, &serial_config))
        return EXIT_FAILURE;
    if (device_mode_config() && device_info_display() && device_config_read_and_update() && device_mode_transfer()) {
        device_packet_read_and_display(&is_active);
        device_disconnect();
        return EXIT_SUCCESS;
    }
    device_disconnect();
    return EXIT_FAILURE;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
