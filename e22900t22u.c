// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

/*
 * E22-900T22D Tester (Raspberry Pi Zero - DIP module via GPIO)
 *
 * This program connects to an E22-900T22D LoRa module by either USB or DIP (UART and GPIO),
 * switches to configuration mode, reads its configuration registers in configuration mode,
 * and updates them as needed to match the desired configuration, then switches back to
 * transmission mode.
 *
 * DIP wiring (Pi → E22 DIP):
 *   VCC  → 3.3V (Pin 1)
 *   GND  → GND  (Pin 6)
 *   TXD  → GPIO 15 / RXD (Pin 10)
 *   RXD  → GPIO 14 / TXD (Pin 8)
 *   M0   → GPIO 17 (Pin 11)
 *   M1   → GPIO 27 (Pin 13)
 *   AUX  → GPIO 22 (Pin 15)
 */

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(E22900T22_SUPPORT_MODULE_DIP)
#include <gpiod.h>
#define SERIAL_PORT "/dev/ttyAMA0"
#elif defined(E22900T22_SUPPORT_MODULE_USB)
#define SERIAL_PORT "/dev/e22900t22u"
#endif

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
#define PRINTF_INFO  printf_stdout
#define PRINTF_ERROR printf_stderr

#include "include/serial_linux.h"

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#if defined(E22900T22_SUPPORT_MODULE_DIP)
#undef E22900T22_SUPPORT_MODULE_USB
#elif defined(E22900T22_SUPPORT_MODULE_USB)
#undef E22900T22_SUPPORT_MODULE_DIP
#endif
#include "include/e22xxxtxx.h"

void __sleep_ms(const uint32_t ms) {
    usleep((useconds_t)ms * 1000);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// DIP 
// -----------------------------------------------------------------------------------------------------------------------------------------

#ifdef E22900T22_SUPPORT_MODULE_DIP

#define GPIO_CHIP "/dev/gpiochip0"
#define GPIO_M0   17
#define GPIO_M1   27
#define GPIO_AUX  22

static struct gpiod_chip *gpio_chip = NULL;
static struct gpiod_line_request *gpio_req_outputs = NULL;
static struct gpiod_line_request *gpio_req_input = NULL;

bool gpio_begin(void) {
    gpio_chip = gpiod_chip_open(GPIO_CHIP);
    if (!gpio_chip) {
        PRINTF_ERROR("gpio: failed to open %s\n", GPIO_CHIP);
        return false;
    }

    // request M0 and M1 as outputs
    struct gpiod_line_settings *out_settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(out_settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(out_settings, GPIOD_LINE_VALUE_INACTIVE);
    struct gpiod_line_config *out_config = gpiod_line_config_new();
    static const unsigned int out_offsets[] = { GPIO_M0, GPIO_M1 };
    gpiod_line_config_add_line_settings(out_config, out_offsets, 2, out_settings);
    struct gpiod_request_config *out_req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(out_req_cfg, "e22-mx");
    gpio_req_outputs = gpiod_chip_request_lines(gpio_chip, out_req_cfg, out_config);
    gpiod_request_config_free(out_req_cfg);
    gpiod_line_config_free(out_config);
    gpiod_line_settings_free(out_settings);
    if (!gpio_req_outputs) {
        PRINTF_ERROR("gpio: failed to request output lines (m0=%d, m1=%d)\n", GPIO_M0, GPIO_M1);
        return false;
    }

    // request AUX as input
    struct gpiod_line_settings *in_settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(in_settings, GPIOD_LINE_DIRECTION_INPUT);
    struct gpiod_line_config *in_config = gpiod_line_config_new();
    static const unsigned int in_offsets[] = { GPIO_AUX };
    gpiod_line_config_add_line_settings(in_config, in_offsets, 1, in_settings);
    struct gpiod_request_config *in_req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(in_req_cfg, "e22-aux");
    gpio_req_input = gpiod_chip_request_lines(gpio_chip, in_req_cfg, in_config);
    gpiod_request_config_free(in_req_cfg);
    gpiod_line_config_free(in_config);
    gpiod_line_settings_free(in_settings);
    if (!gpio_req_input) {
        PRINTF_ERROR("gpio: failed to request input line (aux=%d)\n", GPIO_AUX);
        return false;
    }

    PRINTF_INFO("gpio: ready (m0=%d, m1=%d, aux=%d)\n", GPIO_M0, GPIO_M1, GPIO_AUX);
    return true;
}

void gpio_end(void) {
    if (gpio_req_outputs)
        gpiod_line_request_release(gpio_req_outputs);
    if (gpio_req_input)
        gpiod_line_request_release(gpio_req_input);
    if (gpio_chip)
        gpiod_chip_close(gpio_chip);
    gpio_req_outputs = gpio_req_input = NULL;
    gpio_chip = NULL;
}

void gpio_set_pin_mx(const bool pin_m0, const bool pin_m1) {
    gpiod_line_request_set_value(gpio_req_outputs, GPIO_M0, pin_m0 ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_set_value(gpio_req_outputs, GPIO_M1, pin_m1 ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

bool gpio_get_pin_aux(void) {
    return gpiod_line_request_get_value(gpio_req_input, GPIO_AUX) == GPIOD_LINE_VALUE_ACTIVE;
}

#endif

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

serial_config_t serial_config = {
    .port = "/dev/ttyAMA0",
    .rate = 9600,
    .bits = SERIAL_8N1,
};

e22900t22_config_t e22900t22_config = {
    .address = 0x0008,
    .network = 0x00,
    .channel = 0x17, // Channel 23 (850.125 + 23 = 873.125 MHz)
    .packet_maxsize = E22900T22_CONFIG_PACKET_MAXSIZE_DEFAULT,
    .packet_maxrate = E22900T22_CONFIG_PACKET_MAXRATE_DEFAULT,
    .crypt = E22900T22_CONFIG_CRYPT_DEFAULT,
#if defined(E22900T22_SUPPORT_MODULE_DIP)
    .wor_enabled = E22900T22_CONFIG_WOR_ENABLED_DEFAULT,
    .wor_cycle = E22900T22_CONFIG_WOR_CYCLE_DEFAULT,
#endif
    .transmit_power = E22900T22_CONFIG_TRANSMIT_POWER_DEFAULT,
    .transmission_method = E22900T22_CONFIG_TRANSMISSION_METHOD_DEFAULT,
    .relay_enabled = E22900T22_CONFIG_RELAY_ENABLED_DEFAULT,
    .listen_before_transmit = false,
    .rssi_packet = true,
    .rssi_channel = true,
    .read_timeout_command = E22900T22_CONFIG_READ_TIMEOUT_COMMAND_DEFAULT,
    .read_timeout_packet = E22900T22_CONFIG_READ_TIMEOUT_PACKET_DEFAULT,
#if defined(E22900T22_SUPPORT_MODULE_DIP)
    .set_pin_mx = gpio_set_pin_mx,
    .get_pin_aux = gpio_get_pin_aux,
#endif
    .debug = false,
};

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

volatile bool running = true;

void signal_handler(int sig __attribute__((unused))) {
    if (running) {
        printf("stopping\n");
        running = false;
    }
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {

    setbuf(stdout, NULL);
    printf("starting\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

#if defined(E22900T22_SUPPORT_MODULE_DIP)
    if (!gpio_begin()) {
        fprintf(stderr, "gpio: failed to initialise\n");
        goto exit_fail;
    }
#endif

    if (!serial_begin(&serial_config) || !serial_connect()) {
        fprintf(stderr, "device: failed to connect (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate, serial_bits_str(serial_config.bits));
	goto exit_fail_gpio;
    }

    if (!device_connect(E22900T22_MODULE_DIP, &e22900t22_config))
	goto exit_fail_serial;
    printf("device: connected (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate, serial_bits_str(serial_config.bits));
    if (!(device_mode_config() && device_info_read() && device_config_read_and_update() && device_mode_transfer()))
	goto exit_fail_device;

    device_packet_read_and_display(&running);

exit_fail_device:
    device_disconnect();
exit_fail_serial:
    serial_end();
exit_fail_gpio:
#if defined(E22900T22_SUPPORT_MODULE_DIP)
    gpio_end();
exit_fail:
#endif
    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
