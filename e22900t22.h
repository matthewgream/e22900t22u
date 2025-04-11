
// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#if (!defined(MODULE_USB) && !defined(MODULE_DIP)) || (defined(MODULE_USB) && defined(MODULE_DIP))
#error "must define one of MODULE_USB or MODULE_DIP"
#endif

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define CONFIG_ADDRESS_DEFAULT 0x0000
#define CONFIG_NETWORK_DEFAULT 0x00
#define CONFIG_CHANNEL_DEFAULT 0x00 // Channel 0 (868.125 + 0 = 868.125 MHz)
#define CONFIG_LISTEN_BEFORE_TRANSMIT true
#define CONFIG_RSSI_PACKET_DEFAULT true
#define CONFIG_RSSI_CHANNEL_DEFAULT true
#define CONFIG_READ_TIMEOUT_COMMAND_DEFAULT 1000
#define CONFIG_READ_TIMEOUT_PACKET_DEFAULT 5000

typedef struct {
    unsigned short address;
    unsigned char network;
    unsigned char channel;
    bool listen_before_transmit;
    bool rssi_packet, rssi_channel;
    unsigned long read_timeout_command, read_timeout_packet;
#ifdef MODULE_DIP
    void (*set_pin_mx)(bool pin_m0, bool pin_m1);
    bool (*get_pin_aux)(void);
#endif
    bool debug;
} e22900t22_config_t;

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define COMMAND_DELAY_MS 300

e22900t22_config_t config;

const char *get_uart_rate(const unsigned char value);
const char *get_uart_parity(const unsigned char value);
const char *get_packet_rate(const unsigned char value);
const char *get_packet_size(const unsigned char value);
const char *get_transmit_power(const unsigned char value);
const char *get_mode_transmit(const unsigned char value);
const char *get_wor_cycle(const unsigned char value);
const char *get_enabled(const unsigned char value);
float get_frequency(const unsigned char channel);

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void __hexdump(const unsigned char *data, const int size) {

    static const int bytes_per_line = 16;

    for (int offset = 0; offset < size; offset += bytes_per_line) {
        printf("%04x: ", offset);
        for (int i = 0; i < bytes_per_line; i++) {
            if (i == bytes_per_line / 2)
                printf(" ");
            if (offset + i < size)
                printf("%02x ", data[offset + i]);
            else
                printf("   ");
        }
        printf(" ");
        for (int i = 0; i < bytes_per_line; i++) {
            if (i == bytes_per_line / 2)
                printf(" ");
            if (offset + i < size)
                printf("%c", isprint(data[offset + i]) ? data[offset + i] : '.');
            else
                printf(" ");
        }
        printf("\n");
    }
}

void __sleep_us(const unsigned long us) { usleep(us); }

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_wait_ready() {
#ifdef MODULE_DIP
    static const unsigned long timeout_it = 500, timeout_us = 30 * 1000 * 1000;
    unnsigned long timeout_counter = 0;
    while (!config.get_pin_aux()) {
        if ((timeout_counter += timeout_it) > timeout_us)
            return false;
        __sleep_us(timeout_it);
    }
#endif
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_packet_read(unsigned char *packet, const int max_size, int *packet_size, unsigned char *rssi) {
    *packet_size = serial_read_tosize(packet, max_size, config.read_timeout_packet);
    if (*packet_size <= 0)
        return false;
    if (config.rssi_packet && *packet_size > 0)
        *rssi = packet[--*packet_size];
    else
        *rssi = 0;
    return true;
}

void device_packet_display(const unsigned char *packet, const int packet_size, const unsigned char rssi) {
    printf("---- PACKET --------------------------------\n");
    printf("size=%d bytes", packet_size);
    if (config.rssi_packet)
        printf(", rssi=%.2fdBm", -((float)rssi / 2.0));
    printf("\n");
    __hexdump(packet, packet_size);
    printf("----\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_cmd_send(const unsigned char *cmd, const int cmd_len) {

    __sleep_us(COMMAND_DELAY_MS * 1000);

    if (config.debug) {
        printf("command: send: (%d bytes): ", cmd_len);
        for (int i = 0; i < cmd_len; i++)
            printf("%02X ", cmd[i]);
        printf("\n");
    }

    return serial_write(cmd, cmd_len) == cmd_len;
}

int device_cmd_recv_response(unsigned char *buffer, const int buffer_length, const int timeout_ms) {

    const int read_len = serial_read(buffer, buffer_length, timeout_ms);

    if (config.debug) {
        if (read_len > 0) {
            printf("command: recv: (%d bytes): ", read_len);
            for (int i = 0; i < read_len && i < 32; i++)
                printf("%02X ", buffer[i]);
            if (read_len > 32)
                printf("...");
            printf("\n");
        }
    }

    return read_len;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define DEVICE_CMD_HEADER_SIZE 3
#define DEVICE_CMD_HEADER_LENGTH_OFFSET 2

bool device_cmd_send_wrapper(const char *name, const unsigned char *command, const int command_length,
                             unsigned char *response, const int response_length) {

    if (command_length < DEVICE_CMD_HEADER_SIZE)
        return false;
    if (response_length < command[DEVICE_CMD_HEADER_LENGTH_OFFSET])
        return false;
    if (!device_wait_ready()) {
        fprintf(stderr, "device: %s: wait_ready timeout\n", name);
        return false;
    }
    if (!device_cmd_send(command, command_length)) {
        fprintf(stderr, "device: %s: failed to send command\n", name);
        return false;
    }

    unsigned char buffer[64];
    const int length = DEVICE_CMD_HEADER_SIZE + command[DEVICE_CMD_HEADER_LENGTH_OFFSET];
    const int read_len = device_cmd_recv_response(buffer, length, config.read_timeout_command);
    if (read_len < length) {
        fprintf(stderr, "device: %s: failed to read response, received %d bytes, expected %d bytes\n", name, read_len,
                length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != command[1] || buffer[2] != command[2]) {
        fprintf(stderr, "device: %s: invalid response header: %02X %02X %02X\n", name, buffer[0], buffer[1], buffer[2]);
        return false;
    }

    memcpy(response, buffer + DEVICE_CMD_HEADER_SIZE, command[DEVICE_CMD_HEADER_LENGTH_OFFSET]);
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_channel_rssi_read(unsigned char *rssi_value) {

    static const char *name = "channel_rssi_read";
    static const unsigned char command[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x01};
    static const int command_length = sizeof(command);

    serial_flush();
    if (!device_wait_ready()) {
        fprintf(stderr, "device: %s: wait_ready timeout\n", name);
        return false;
    }
    if (serial_write(command, command_length) != command_length) {
        fprintf(stderr, "device: %s: failed to send command\n", name);
        return false;
    }

    unsigned char buffer[4];
    const int length = sizeof(buffer);
    const int read_len = serial_read(buffer, length, 1000);
    if (read_len < length) {
        fprintf(stderr, "device: %s: failed, received %d bytes, expected %d bytes\n", name, read_len, length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != 0x00 || buffer[2] != 0x01) {
        fprintf(stderr, "device: %s: invalid response header: %02X %02X %02X %02X\n", name, buffer[0], buffer[1],
                buffer[2], buffer[3]);
        return false;
    }

    *rssi_value = buffer[3];
    return true;
}

void device_channel_rssi_display(unsigned char rssi_value) {
    const int rssi_dbm = -((int)rssi_value) / 2;
    printf("device: rssi-channel: %d dBm\n", rssi_dbm);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

typedef enum {
    DEVICE_MODE_CONFIG = 0,
    DEVICE_MODE_TRANSFER = 1,
    // DEVICE_MODE_WOR
    // DEVICE_MODE_DEEPSLEEP
} device_mode_t;

const char *device_mode_str(const device_mode_t mode) {
    switch (mode) {
    case DEVICE_MODE_CONFIG:
        return "config";
    case DEVICE_MODE_TRANSFER:
        return "transfer";
    default:
        return "unknown";
    }
}

#ifdef MODULE_USB
bool device_mode_switch_impl(const device_mode_t mode) {
    static const unsigned char cmd_switch_to_config[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x01};
    static const unsigned char cmd_switch_to_transfer[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x00};

    static const char *name = "mode_switch_software";
    const unsigned char *command = (mode == DEVICE_MODE_CONFIG) ? cmd_switch_to_config : cmd_switch_to_transfer;
    const int command_length =
        (mode == DEVICE_MODE_CONFIG) ? sizeof(cmd_switch_to_config) : sizeof(cmd_switch_to_transfer);

    if (!device_cmd_send(command, command_length)) {
        fprintf(stderr, "device: %s: failed to send command\n", name);
        return false;
    }

    unsigned char buffer[64];
    const int length = command_length - 1;
    const int read_len = device_cmd_recv_response(buffer, length, config.read_timeout_command);
    if (read_len < length) {
        fprintf(stderr, "device: %s: failed, received %d bytes, expected %d bytes\n", name, read_len, length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != 0xC2 || buffer[2] != 0xC3 || buffer[3] != 0x02) {
        fprintf(stderr, "device: %s: invalid response header: %02X %02X %02X %02X\n", name, buffer[0], buffer[1],
                buffer[2], buffer[3]);
        return false;
    }
    return true;
}
#elif MODULE_DIP
bool device_mode_switch_impl(const device_mode_t mode) {
    static const char *name = "mode_switch_hardware";
    if (!device_wait_ready()) {
        fprintf(stderr, "device: %s: wait_ready timeout (pre switch)\n", name);
        return false;
    }
    if (mode == DEVICE_MODE_CONFIG)
        config.set_pin_mx(false, true);
    else
        config.set_pin_mx(false, false);
    __sleep_us(500);
    if (!device_wait_ready()) {
        fprintf(stderr, "device: %s: wait_ready timeout (post switch)\n", name);
        return false;
    }
    return true;
}
#endif

bool device_mode_switch(const device_mode_t mode) {
    if (!device_mode_switch_impl(mode))
        return false;
    static const char *name = "mode_switch";
    printf("device: %s: --> %s\n", name, device_mode_str(mode));
    return true;
}
bool device_mode_config(void) { return device_mode_switch(DEVICE_MODE_CONFIG); }
bool device_mode_transfer(void) { return device_mode_switch(DEVICE_MODE_TRANSFER); }

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define DEVICE_PRODUCT_INFO_SIZE 7

bool device_product_info_read(unsigned char *result) {
    static const unsigned char cmd[] = {0xC1, 0x80, DEVICE_PRODUCT_INFO_SIZE};
    return device_cmd_send_wrapper("device_product_info_read", cmd, sizeof(cmd), result, DEVICE_PRODUCT_INFO_SIZE);
}

void device_product_info_display(const unsigned char *info) {
    printf("device: product_info: ");
    for (int i = 0; i < DEVICE_PRODUCT_INFO_SIZE; i++)
        printf("%02X ", info[i]);
    printf("\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

// XXX it would be better to define a packed struct for the config rather than rely on offsets and bit manipulation

#define DEVICE_MODULE_CONFIG_SIZE 9
#define DEVICE_MODULE_CONFIG_SIZE_WRITE 7

bool device_module_config_read(unsigned char *config) {
    static const unsigned char cmd[] = {0xC1, 0x00, DEVICE_MODULE_CONFIG_SIZE};
    return device_cmd_send_wrapper("read_module_config", cmd, sizeof(cmd), config, DEVICE_MODULE_CONFIG_SIZE);
}

bool device_module_config_write(const unsigned char *config) {
    unsigned char cmd[DEVICE_CMD_HEADER_SIZE + DEVICE_MODULE_CONFIG_SIZE_WRITE] = {0xC0, 0x00,
                                                                                   DEVICE_MODULE_CONFIG_SIZE_WRITE};
    memcpy(cmd + DEVICE_CMD_HEADER_SIZE, config, DEVICE_MODULE_CONFIG_SIZE_WRITE);
    unsigned char result[DEVICE_MODULE_CONFIG_SIZE_WRITE];
    if (!device_cmd_send_wrapper("write_module_config", cmd, sizeof(cmd), result, DEVICE_MODULE_CONFIG_SIZE_WRITE))
        return false;
    for (int i = 0; i < DEVICE_MODULE_CONFIG_SIZE_WRITE; i++) {
        if (result[i] != config[i]) {
            fprintf(stderr, "device: write_modify_config: verification failed at %d: %02X != %02X\n", i, result[i],
                    config[i]);
            return false;
        }
    }
    return true;
}

void device_module_config_display(const unsigned char *config) {

    const unsigned short address = config[0] << 8 | config[1]; // Module address (ADDH, ADDL)
    const unsigned char network = config[2];                   // Network ID (NETID)
    const unsigned char reg0 = config[3];                      // REG0 - UART and Air Data Rate
    const unsigned char reg1 = config[4];                      // REG1 - Subpacket size and other settings
    const unsigned char channel = config[5];                   // REG2 - Channel Control (CH)
    const unsigned char reg3 = config[6];                      // REG3 - Various options
    const unsigned short crypt = config[7] << 8 | config[8];   // CRYPT (not readable, will show as 0)

    printf("device: module_config: ");

    printf("address=0x%04X, ", address);
    printf("network=0x%02X, ", network);
    printf("channel=%d (frequency=%.3fMHz), ", channel, get_frequency(channel));

    printf("data-rate=%s, ", get_packet_rate(reg0));
    printf("packet-size=%s, ", get_packet_size(reg1));
    printf("transmit-power=%s, ", get_transmit_power(reg1));
    printf("encryption-key=0x%04X, ", crypt);

    printf("rssi-channel=%s, ", get_enabled(reg1 & 0x20));
    printf("rssi-packet=%s, ", get_enabled(reg3 & 0x80));

    printf("mode-listen-before-tx=%s, ", get_enabled(reg3 & 0x10));
    printf("mode-transmit=%s, ", get_mode_transmit(reg3));
    printf("mode-relay=%s, ", get_enabled(reg3 & 0x20));

#ifdef MODULE_DIP
    printf("mode-wor-enable=%s, ", get_enabled(reg3 & 0x08));
    printf("mode-wor-cycle=%d, ", get_wor_cycle(reg3));
#endif

    printf("uart-rate=%s, ", get_uart_rate(reg0));
    printf("uart-parity=%s, ", get_uart_parity(reg0));

#ifdef MODULE_USB
    printf("switch-config-serial=%s, ", get_enabled(reg1 & 0x04));
#endif

    printf("\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void __update_config_bool(const char *name, unsigned char *byte, const unsigned char bits, const bool setting) {
    const bool value = (bool)(*byte & bits);
    if (value != setting) {
        printf("device: update_configuration: %s: %s --> %s\n", name, value ? "on" : "off", setting ? "on" : "off");
        if (setting)
            *byte |= bits;
        else
            *byte &= ~bits;
    }
}

bool update_configuration(unsigned char *config_device) {
    unsigned char config_device_orig[DEVICE_MODULE_CONFIG_SIZE_WRITE];
    memcpy(config_device_orig, config_device, DEVICE_MODULE_CONFIG_SIZE_WRITE);

    const unsigned short address = config_device[0] << 8 | config_device[1];
    if (address != config.address) {
        printf("device: update_configuration: address: 0x%04X --> 0x%04X\n", address, config.address);
        config_device[0] = (unsigned char)(config.address >> 8);
        config_device[1] = (unsigned char)(config.address & 0xFF);
    }
    const unsigned char network = config_device[2];
    if (network != config.network) {
        printf("device: update_configuration: network: 0x%02X --> 0x%02X\n", network, config.network);
        config_device[2] = config.network;
    }
    const unsigned char channel = config_device[5];
    if (channel != config.channel) {
        printf("device: update_configuration: channel: %d (%.3fMHz) --> %d (%.3fMHz)\n", channel,
               get_frequency(channel), config.channel, get_frequency(config.channel));
        config_device[5] = config.channel;
    }

    __update_config_bool("listen-before-transmit", &config_device[6], 0x10, config.listen_before_transmit);
    __update_config_bool("rssi-channel", &config_device[4], 0x20, config.rssi_channel);
    __update_config_bool("rssi-packet", &config_device[6], 0x80, config.rssi_packet);
#ifdef MODULE_USB
    __update_config_bool("switch-config-serial", &config_device[4], 0x04, true);
#endif

    const bool update_required = memcmp(config_device_orig, config_device, DEVICE_MODULE_CONFIG_SIZE_WRITE) != 0;

    return update_required;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_connect(const e22900t22_config_t *config_device, const serial_config_t *config_serial) {
    memcpy(&config, config_device, sizeof(e22900t22_config_t));
    if (!serial_connect(config_serial)) {
        fprintf(stderr, "device: failed to connect (port=%s, rate=%d, bits=%s)\n", config_serial->port,
                config_serial->rate, serial_bits_str(config_serial->bits));
        return false;
    }
    printf("device: connected (port=%s, rate=%d, bits=%s)\n", config_serial->port, config_serial->rate,
           serial_bits_str(config_serial->bits));
    return true;
}
void device_disconnect() {
    serial_disconnect();
    printf("device: disconnected\n");
}

bool device_info_display() {
    unsigned char product_info[DEVICE_PRODUCT_INFO_SIZE];
    if (!device_product_info_read(product_info)) {
        fprintf(stderr, "device: failed to read product information\n");
        return false;
    }
    device_product_info_display(product_info);
    return true;
}

bool device_config_read_and_update() {
    unsigned char config[DEVICE_MODULE_CONFIG_SIZE];
    if (!device_module_config_read(config)) {
        fprintf(stderr, "device: failed to read module configuration\n");
        return false;
    }
    device_module_config_display(config);
    if (update_configuration(config)) {
        printf("device: update module configuration\n");
        if (!device_module_config_write(config)) {
            fprintf(stderr, "device: failed to write module configuration\n");
            return false;
        }
        printf("device: verify module configuration\n");
        unsigned char config_2[DEVICE_MODULE_CONFIG_SIZE_WRITE];
        if (!device_module_config_read(config_2) || memcmp(config, config_2, sizeof(config_2)) != 0) {
            fprintf(stderr, "device: failed to verify module configuration\n");
            return false;
        }
    }
    return true;
}

void device_packet_read_and_display(volatile bool *is_active) {

    printf("device: packet read and display (with periodic channel_rssi)\n");

    static const int max_packet_size = 256;
    unsigned char packet_buffer[max_packet_size];
    int packet_size;
    unsigned char rssi;

    while (*is_active) {
        if (device_packet_read(packet_buffer, max_packet_size, &packet_size, &rssi)) {
            device_packet_display(packet_buffer, packet_size, rssi);
        } else {
            if (device_channel_rssi_read(&rssi))
                device_channel_rssi_display(rssi);
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

const char *get_uart_rate(const unsigned char value) {
    switch ((value >> 5) & 0x07) {
    case 0:
        return "1200bps";
    case 1:
        return "2400bps";
    case 2:
        return "4800bps";
    case 3:
        return "9600bps (Default)";
    case 4:
        return "19200bps";
    case 5:
        return "38400bps";
    case 6:
        return "57600bps";
    case 7:
        return "115200bps";
    default:
        return "unknown";
    }
}

const char *get_uart_parity(const unsigned char value) {
    switch ((value >> 3) & 0x03) {
    case 0:
        return "8N1 (Default)";
    case 1:
        return "8O1";
    case 2:
        return "8E1";
    case 3:
        return "8N1";
    default:
        return "unknown";
    }
}

const char *get_packet_rate(const unsigned char value) {
    switch (value & 0x07) {
    case 0:
        return "2.4kbps";
    case 1:
        return "2.4kbps";
    case 2:
        return "2.4kbps (Default)";
    case 3:
        return "4.8kbps";
    case 4:
        return "9.6kbps";
    case 5:
        return "19.2kbps";
    case 6:
        return "38.4kbps";
    case 7:
        return "62.5kbps";
    default:
        return "unknown";
    }
}

const char *get_packet_size(const unsigned char value) {
    switch ((value >> 6) & 0x03) {
    case 0:
        return "240bytes (Default)";
    case 1:
        return "128bytes";
    case 2:
        return "64bytes";
    case 3:
        return "32bytes";
    default:
        return "unknown";
    }
}

const char *get_transmit_power(const unsigned char value) {
    switch (value & 0x03) {
    case 0:
        return "22dBm (Default)";
    case 1:
        return "17dBm";
    case 2:
        return "13dBm";
    case 3:
        return "10dBm";
    default:
        return "unknown";
    }
}

const char *get_mode_transmit(const unsigned char value) {
    switch (value & 0x40) {
    case 0:
        return "fixed";
    case 1:
        return "transparent";
    default:
        return "unknown";
    }
}

const char *get_wor_cycle(const unsigned char value) {
    switch (value & 0x07) {
    case 0:
        return "500ms";
    case 1:
        return "1000ms";
    case 2:
        return "1500ms";
    case 3:
        return "2000ms (Default)";
    case 4:
        return "2500ms";
    case 5:
        return "3000ms";
    case 6:
        return "3500ms";
    case 7:
        return "4000ms";
    default:
        return "unknown";
    }
}

const char *get_enabled(const unsigned char value) { return value > 0 ? "on" : "off"; }

float get_frequency(const unsigned char channel) { return 850.125 + channel; }

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
