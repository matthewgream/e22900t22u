
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include "serial.h"
#define MODULE_USB
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
    printf("e22900t22u: stopping\n");
    is_active = false;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    printf("e22900t22u: starting\n");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    if (!device_connect(&e22900t22u_config, &serial_config))
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
