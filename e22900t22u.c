
// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

/*
 * E22-900T22U Tester
 *
 * This program connects to an E22-900T22U LoRa module via serial port,
 * switches to configuration mode, reads its configuration registers in configuration mode, and updates
 * them as needed to match the desired configuration, then switches back to transmission mode.
 */

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void printf_stdout(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}
void printf_stderr(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

#define PRINTF_DEBUG printf_stdout
#define PRINTF_INFO printf_stdout
#define PRINTF_ERROR printf_stderr

#include "include/serial_linux.h"
#undef E22900T22_SUPPORT_MODULE_DIP
#define E22900T22_SUPPORT_MODULE_USB
#include "include/e22900t22.h"
void __sleep_ms(const unsigned long ms) { usleep(ms * 1000); }

serial_config_t serial_config = {
    .port = "/dev/e22900t22u",
    .rate = 9600,
    .bits = SERIAL_8N1,
};

e22900t22_config_t e22900t22u_config = {
    .address = 0x0008,
    .network = 0x00,
    .channel = 0x17, // Channel 23 (850.125 + 23 = 873.125 MHz)
    .packet_maxsize = CONFIG_PACKET_MAXSIZE_DEFAULT,
    .listen_before_transmit = false,
    .rssi_packet = true,
    .rssi_channel = true,
    .read_timeout_command = CONFIG_READ_TIMEOUT_COMMAND_DEFAULT,
    .read_timeout_packet = CONFIG_READ_TIMEOUT_PACKET_DEFAULT,
    .debug = false,
};

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

volatile bool is_active = true;

void signal_handler(int sig __attribute__((unused))) {
    if (is_active) {
        printf("stopping\n");
        is_active = false;
    }
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {

    printf("starting\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!serial_connect(&serial_config)) {
        fprintf(stderr, "device: failed to connect (port=%s, rate=%d, bits=%s)\n", serial_config.port,
                serial_config.rate, serial_bits_str(serial_config.bits));
        return false;
    }

    if (!device_connect(E22900T22_MODULE_USB, &e22900t22u_config)) {
        serial_disconnect();
        return EXIT_FAILURE;
    }
    printf("device: connected (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate,
           serial_bits_str(serial_config.bits));
    if (!(device_mode_config() && device_info_display() && device_config_read_and_update() && device_mode_transfer())) {
        device_disconnect();
        serial_disconnect();
        return EXIT_FAILURE;
    }

    device_packet_read_and_display(&is_active);

    device_disconnect();
    serial_disconnect();

    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
