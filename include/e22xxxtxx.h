#pragma once

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void __sleep_ms(const uint32_t ms);

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#if !defined(E22900T22_SUPPORT_MODULE_DIP) && !defined(E22900T22_SUPPORT_MODULE_USB)
#error "no E22900T22_SUPPORT_MODULE_DIP or E22900T22_SUPPORT_MODULE_USB defined"
#endif

#if defined(E22900T22_SUPPORT_NO_TRANSMIT) && defined(E22900T22_SUPPORT_NO_RECEIVE)
#error "both E22900T22_SUPPORT_NO_TRANSMIT and E22900T22_SUPPORT_NO_RECEIVE defined"
#endif

#define E22900T22_PACKET_MAXSIZE_32                   32
#define E22900T22_PACKET_MAXSIZE_64                   64
#define E22900T22_PACKET_MAXSIZE_128                  128
#define E22900T22_PACKET_MAXSIZE_240                  240
#define E22900T22_PACKET_MAXSIZE                      E22900T22_PACKET_MAXSIZE_240

#define E22900T22_PACKET_MAXRATE_2400                 2
#define E22900T22_PACKET_MAXRATE_4800                 4
#define E22900T22_PACKET_MAXRATE_9600                 9
#define E22900T22_PACKET_MAXRATE_19200                19
#define E22900T22_PACKET_MAXRATE_38400                38
#define E22900T22_PACKET_MAXRATE_62500                62
#define E22900T22_PACKET_MAXRATE                      E22900T22_PACKET_MAXRATE_62500

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define E22900T22_CONFIG_ADDRESS_DEFAULT              0x0000
#define E22900T22_CONFIG_NETWORK_DEFAULT              0x00
#define E22900T22_CONFIG_CHANNEL_DEFAULT              0x00 // Channel 0 (850.125 + 0 = 850.125 MHz)
#define E22900T22_CONFIG_LISTEN_BEFORE_TRANSMIT       true
#define E22900T22_CONFIG_RSSI_PACKET_DEFAULT          true
#define E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT         true
#define E22900T22_CONFIG_READ_TIMEOUT_COMMAND_DEFAULT 1000
#define E22900T22_CONFIG_READ_TIMEOUT_PACKET_DEFAULT  5000
#define E22900T22_CONFIG_PACKET_MAXSIZE_DEFAULT       E22900T22_PACKET_MAXSIZE_240
#define E22900T22_CONFIG_PACKET_MAXRATE_DEFAULT       E22900T22_PACKET_MAXRATE_2400
#define E22900T22_CONFIG_CRYPT_DEFAULT                0x0000
#define E22900T22_CONFIG_WOR_ENABLED_DEFAULT          false
#define E22900T22_CONFIG_WOR_CYCLE_DEFAULT            2000
#define E22900T22_CONFIG_WOR_CYCLE_MIN                500
#define E22900T22_CONFIG_WOR_CYCLE_MAX                4000
#define E22900T22_CONFIG_WOR_CYCLE_INCREMENT          500
#define E22900T22_CONFIG_TRANSMIT_POWER_DEFAULT       3
#define E22900T22_CONFIG_TRANSMIT_POWER_MIN           0
#define E22900T22_CONFIG_TRANSMIT_POWER_MAX           3

typedef struct {
    uint16_t name;
    uint8_t version, maxpower, frequency, type;
} e22900txx_device_t;

typedef enum {
    E22900T22_MODULE_USB = 0,
    E22900T22_MODULE_DIP = 1,
} e22900t22_module_t;

typedef struct {
    uint16_t address;
    uint8_t network;
    uint8_t channel;
    uint8_t packet_maxsize;
    uint8_t packet_maxrate;
    uint16_t crypt;
    bool wor_enabled;
    uint16_t wor_cycle;
    uint8_t transmit_power;
    bool listen_before_transmit;
    bool rssi_packet, rssi_channel;
    uint32_t read_timeout_command, read_timeout_packet;
#ifdef E22900T22_SUPPORT_MODULE_DIP
    void (*set_pin_mx)(const bool pin_m0, const bool pin_m1);
    bool (*get_pin_aux)(void);
#endif
    bool debug;
} e22900t22_config_t;

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static e22900txx_device_t _e22900txx_device = { .maxpower = 22 };
static e22900t22_module_t _e22900txx_module;
static e22900t22_config_t _e22900txx_config;

static const char *get_uart_rate(const uint8_t value);
static const char *get_uart_parity(const uint8_t value);
static const char *get_packet_rate(const uint8_t value);
static const char *get_packet_size(const uint8_t value);
static const char *get_transmit_power(const uint8_t value);
static const char *get_mode_transmit(const uint8_t value);
static const char *get_wor_cycle(const uint8_t value);
static const char *get_enabled(const uint8_t value);
static uint32_t get_frequency1000(const uint8_t channel);
static int get_rssi_dbm(const uint8_t rssi);

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static void __print_hex_dump(const uint8_t *data, const int size, const char *prefix) {
    static const int bytes_per_line = 16;
    for (int offset = 0; offset < size; offset += bytes_per_line) {
        PRINTF_INFO("%s%04x: ", prefix, offset);
        for (int i = 0; i < bytes_per_line; i++)
            if (offset + i < size)
                PRINTF_INFO("%s%02" PRIX8 " ", i == bytes_per_line / 2 ? " " : "", data[offset + i]);
            else
                PRINTF_INFO("%s   ", i == bytes_per_line / 2 ? " " : "");
        PRINTF_INFO(" ");
        for (int i = 0; i < bytes_per_line; i++)
            if (offset + i < size)
                PRINTF_INFO("%s%c", i == bytes_per_line / 2 ? " " : "", isprint(data[offset + i]) ? data[offset + i] : '.');
            else
                PRINTF_INFO("%s ", i == bytes_per_line / 2 ? " " : "");
        PRINTF_INFO("\n");
    }
}

static void __print_hex_debug(const uint8_t *data, const int length, const int max) {
    const int n = (max > 0 && length > max) ? max : length;
    for (int i = 0; i < n; i++)
        PRINTF_DEBUG("%02" PRIX8 " ", data[i]);
    if (max > 0 && length > max)
        PRINTF_DEBUG("...");
    PRINTF_DEBUG("\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static bool device_wait_ready(void) {
#ifdef E22900T22_SUPPORT_MODULE_DIP
    if (_e22900txx_module == E22900T22_MODULE_DIP) {
        static const uint32_t timeout_it = 1, timeout_ms = 30 * 1000;
        uint32_t timeout_counter = 0;
        while (!_e22900txx_config.get_pin_aux()) {
            if ((timeout_counter += timeout_it) > timeout_ms)
                return false;
            __sleep_ms(timeout_it);
        }
        if (timeout_counter > 0)
            __sleep_ms(50);
    }
#endif
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static bool device_packet_write(const uint8_t *packet, const int length) {
    if (length <= 0 || length > _e22900txx_config.packet_maxsize)
        return false;
    return serial_write(packet, length) == length;
}

static bool device_packet_read(uint8_t *packet, const int max_size, int *packet_size, uint8_t *rssi) {
    *packet_size = serial_read(packet, max_size, _e22900txx_config.read_timeout_packet);
    if (*packet_size <= 0)
        return false;
    *rssi = _e22900txx_config.rssi_packet ? packet[--*packet_size] : 0;
    return true;
}

static void device_packet_display(const uint8_t *packet, const int packet_size, const uint8_t rssi) {
    PRINTF_INFO("device: packet: size=%d", packet_size);
    if (_e22900txx_config.rssi_packet)
        PRINTF_INFO(", rssi=%d dBm", get_rssi_dbm(rssi));
    PRINTF_INFO("\n");
    __print_hex_dump(packet, packet_size, "    ");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static bool device_cmd_send(const uint8_t *cmd, const int cmd_len) {

    if (_e22900txx_config.debug) {
        PRINTF_DEBUG("command: send: (%d bytes): ", cmd_len);
        __print_hex_debug(cmd, cmd_len, 0);
    }

    return serial_write(cmd, cmd_len) == cmd_len;
}

static int device_cmd_recv_response(uint8_t *buffer, const int buffer_length, const uint32_t timeout_ms) {

    const int read_len = serial_read(buffer, buffer_length, timeout_ms);

    if (_e22900txx_config.debug) {
        if (read_len > 0) {
            PRINTF_DEBUG("command: recv: (%d bytes): ", read_len);
            __print_hex_debug(buffer, read_len, 32);
        }
    }

    return read_len;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define E22900T22_DEVICE_CMD_HEADER_SIZE          3
#define E22900T22_DEVICE_CMD_HEADER_LENGTH_OFFSET 2

static bool device_cmd_send_wrapper(const char *name, const uint8_t *command, const int command_length, uint8_t *response, const int response_length) {

    if (command_length < E22900T22_DEVICE_CMD_HEADER_SIZE)
        return false;
    if (response_length < command[E22900T22_DEVICE_CMD_HEADER_LENGTH_OFFSET])
        return false;

    if (!device_cmd_send(command, command_length)) {
        PRINTF_ERROR("device: %s: failed to send command\n", name);
        return false;
    }

    uint8_t buffer[64]; // XXX
    const int length = E22900T22_DEVICE_CMD_HEADER_SIZE + command[E22900T22_DEVICE_CMD_HEADER_LENGTH_OFFSET];
    if (length > (int)sizeof(buffer))
        return false;
    const int read_len = device_cmd_recv_response(buffer, length, _e22900txx_config.read_timeout_command);
    if (read_len < length) {
        PRINTF_ERROR("device: %s: failed to read response, received %d bytes, expected %d bytes\n", name, read_len, length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != command[1] || buffer[2] != command[2]) {
        PRINTF_ERROR("device: %s: invalid response header: %02" PRIX8 " %02" PRIX8 " %02" PRIX8 "\n", name, buffer[0], buffer[1], buffer[2]);
        return false;
    }

    memcpy(response, buffer + E22900T22_DEVICE_CMD_HEADER_SIZE, command[E22900T22_DEVICE_CMD_HEADER_LENGTH_OFFSET]);
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static bool device_channel_rssi_read(uint8_t *rssi) {

    static const char *name = "channel_rssi_read";
    static const uint8_t command[] = { 0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x01 };
    static const int command_length = sizeof(command);

    serial_flush();
    if (serial_write(command, command_length) != command_length) {
        PRINTF_ERROR("device: %s: failed to send command\n", name);
        return false;
    }

    uint8_t buffer[4]; // XXX
    const int length = sizeof(buffer);
    const int read_len = serial_read(buffer, length, _e22900txx_config.read_timeout_command);
    if (read_len < length) {
        PRINTF_ERROR("device: %s: failed, received %d bytes, expected %d bytes\n", name, read_len, length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != 0x00 || buffer[2] != 0x01) {
        PRINTF_ERROR("device: %s: invalid response header: %02" PRIX8 " %02" PRIX8 " %02" PRIX8 " %02" PRIX8 "\n", name, buffer[0], buffer[1], buffer[2], buffer[3]);
        return false;
    }

    *rssi = buffer[3];
    return true;
}

static void device_channel_rssi_display(uint8_t rssi) {
    PRINTF_INFO("device: rssi-channel: %d dBm\n", get_rssi_dbm(rssi));
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

typedef enum {
    DEVICE_MODE_CONFIG = 0,
    DEVICE_MODE_TRANSFER = 1,
    // DEVICE_MODE_WOR
    // DEVICE_MODE_DEEPSLEEP
} device_mode_t;

static const char *device_mode_str(const device_mode_t mode) {
    switch (mode) {
    case DEVICE_MODE_CONFIG:
        return "config";
    case DEVICE_MODE_TRANSFER:
        return "transfer";
    default:
        return "unknown";
    }
}

#ifdef E22900T22_SUPPORT_MODULE_USB
static bool device_mode_switch_impl_software(const device_mode_t mode) {
    static const uint8_t cmd_switch_config[] = { 0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x01 };
    static const uint8_t cmd_switch_transfer[] = { 0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x00 };

    static const char *name = "mode_switch_software";
    const uint8_t *command = (mode == DEVICE_MODE_CONFIG) ? cmd_switch_config : cmd_switch_transfer;
    const int command_length = (mode == DEVICE_MODE_CONFIG) ? sizeof(cmd_switch_config) : sizeof(cmd_switch_transfer);

    serial_flush();
    if (!device_cmd_send(command, command_length)) {
        PRINTF_ERROR("device: %s: failed to send command\n", name);
        return false;
    }

    uint8_t buffer[64]; // XXX
    const int length = command_length - 1;
    const int read_len = device_cmd_recv_response(buffer, length, _e22900txx_config.read_timeout_command);
    if (read_len == 3 && (buffer[0] == 0xFF && buffer[1] == 0xFF && buffer[2] == 0xFF)) {
        PRINTF_INFO("device: %s: already appears to be in required mode, will accept\n", name);
        return true;
    }
    if (read_len < length) {
        PRINTF_ERROR("device: %s: failed, received %d bytes, expected %d bytes\n", name, read_len, length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != 0xC2 || buffer[2] != 0xC3 || buffer[3] != 0x02) {
        PRINTF_ERROR("device: %s: invalid response header: %02" PRIX8 " %02" PRIX8 " %02" PRIX8 " %02" PRIX8 "\n", name, buffer[0], buffer[1], buffer[2], buffer[3]);
        return false;
    }
    return true;
}
#endif

#ifdef E22900T22_SUPPORT_MODULE_DIP
static bool device_mode_switch_impl_hardware(const device_mode_t mode) {
    static const char *name = "mode_switch_hardware";
    if (!device_wait_ready()) {
        PRINTF_ERROR("device: %s: wait_ready timeout (pre switch)\n", name);
        return false;
    }
    if (mode == DEVICE_MODE_CONFIG)
        _e22900txx_config.set_pin_mx(false, true);
    else
        _e22900txx_config.set_pin_mx(false, false);
    if (!device_wait_ready()) {
        PRINTF_ERROR("device: %s: wait_ready timeout (post switch)\n", name);
        return false;
    }
    return true;
}
#endif

static bool device_mode_switch(const device_mode_t mode) {
    bool result = false;
#ifdef E22900T22_SUPPORT_MODULE_DIP
    if (_e22900txx_module == E22900T22_MODULE_DIP)
        result = device_mode_switch_impl_hardware(mode);
#endif
#ifdef E22900T22_SUPPORT_MODULE_USB
    if (_e22900txx_module == E22900T22_MODULE_USB)
        result = device_mode_switch_impl_software(mode);
#endif
    if (!result)
        return false;
    static const char *name = "mode_switch";
    PRINTF_DEBUG("device: %s: --> %s\n", name, device_mode_str(mode));
    return true;
}

static bool device_mode_config(void) {
    return device_mode_switch(DEVICE_MODE_CONFIG);
}

static bool device_mode_transfer(void) {
    return device_mode_switch(DEVICE_MODE_TRANSFER);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define E22900T22_DEVICE_PRD_INFO_SIZE             7
#define E22900T22_DEVICE_PRD_INFO_OFFSET_NAME_H    0
#define E22900T22_DEVICE_PRD_INFO_OFFSET_NAME_L    1
#define E22900T22_DEVICE_PRD_INFO_OFFSET_VERSION   2
#define E22900T22_DEVICE_PRD_INFO_OFFSET_MAXPOWER  3
#define E22900T22_DEVICE_PRD_INFO_OFFSET_FREQUENCY 4
#define E22900T22_DEVICE_PRD_INFO_OFFSET_TYPE      5

static bool device_product_info_read(uint8_t *result) {
    static const uint8_t cmd[] = { 0xC1, 0x80, E22900T22_DEVICE_PRD_INFO_SIZE };
    return device_cmd_send_wrapper("device_product_info_read", cmd, sizeof(cmd), result, E22900T22_DEVICE_PRD_INFO_SIZE);
}

static void device_product_info_display(const uint8_t *info) {
    PRINTF_INFO("device: product_info: ");
    PRINTF_INFO("name=%04" PRIX16 ", version=%" PRIu8 ", maxpower=%" PRIu8 ", frequency=%" PRIu8 ", type=%" PRIu8 "", (uint16_t)info[E22900T22_DEVICE_PRD_INFO_OFFSET_NAME_H] << 8 | info[E22900T22_DEVICE_PRD_INFO_OFFSET_NAME_L],
                info[E22900T22_DEVICE_PRD_INFO_OFFSET_VERSION], info[E22900T22_DEVICE_PRD_INFO_OFFSET_MAXPOWER], info[E22900T22_DEVICE_PRD_INFO_OFFSET_FREQUENCY], info[E22900T22_DEVICE_PRD_INFO_OFFSET_TYPE]);
    PRINTF_INFO(" [");
    for (int i = 0; i < E22900T22_DEVICE_PRD_INFO_SIZE; i++)
        PRINTF_INFO("%s%02" PRIX8, (i == 0 ? "" : " "), info[i]);
    PRINTF_INFO("]\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

// XXX it would be better to define a packed struct for the config rather than rely on offsets and bit manipulation

#define E22900T22_DEVICE_MOD_CONF_SIZE       9
#define E22900T22_DEVICE_MOD_CONF_SIZE_WRITE 7

static bool device_module_config_read(uint8_t *cfg) {
    static const uint8_t cmd[] = { 0xC1, 0x00, E22900T22_DEVICE_MOD_CONF_SIZE };
    return device_cmd_send_wrapper("read_module_config", cmd, sizeof(cmd), cfg, E22900T22_DEVICE_MOD_CONF_SIZE);
}

static bool device_module_config_write(const uint8_t *cfg) {
    uint8_t cmd[E22900T22_DEVICE_CMD_HEADER_SIZE + E22900T22_DEVICE_MOD_CONF_SIZE_WRITE] = { 0xC0, 0x00, E22900T22_DEVICE_MOD_CONF_SIZE_WRITE };
    memcpy(cmd + E22900T22_DEVICE_CMD_HEADER_SIZE, cfg, E22900T22_DEVICE_MOD_CONF_SIZE_WRITE);
    uint8_t res[E22900T22_DEVICE_MOD_CONF_SIZE_WRITE];
    if (!device_cmd_send_wrapper("write_module_config", cmd, sizeof(cmd), res, E22900T22_DEVICE_MOD_CONF_SIZE_WRITE))
        return false;
    for (int i = 0; i < E22900T22_DEVICE_MOD_CONF_SIZE_WRITE; i++)
        if (res[i] != cfg[i]) {
            PRINTF_ERROR("device: write_modify_config: verification failed at %d: %02" PRIX8 " != %02" PRIX8 "\n", i, res[i], cfg[i]);
            return false;
        }
    return true;
}

static void device_module_config_display(const uint8_t *config_device) {

    const uint16_t address = (uint16_t)config_device[0] << 8 | config_device[1]; // Module address (ADDH, ADDL)
    const uint8_t network = config_device[2];                                    // Network ID (NETID)
    const uint8_t reg0 = config_device[3];                                       // REG0 - UART and Air Data Rate
    const uint8_t reg1 = config_device[4];                                       // REG1 - Subpacket size and other settings
    const uint8_t channel = config_device[5];                                    // REG2 - Channel Control (CH)
    const uint8_t reg3 = config_device[6];                                       // REG3 - Various options
    const uint16_t crypt = (uint16_t)config_device[7] << 8 | config_device[8];   // CRYPT (not readable, will show as 0)

    PRINTF_INFO("device: module_config: ");

    PRINTF_INFO("address=0x%04" PRIX16 ", ", address);
    PRINTF_INFO("network=0x%02" PRIX8 ", ", network);
    const uint32_t frequency1000 = get_frequency1000(channel);
    PRINTF_INFO("channel=%d (frequency=%" PRIu32 ".%03" PRIu32 "MHz), ", channel, frequency1000 / 1000, frequency1000 % 1000);

    PRINTF_INFO("data-rate=%s, ", get_packet_rate(reg0));
    PRINTF_INFO("packet-size=%s, ", get_packet_size(reg1));
    PRINTF_INFO("transmit-power=%s, ", get_transmit_power(reg1));
    PRINTF_INFO("encryption-key=0x%04" PRIX16 ", ", crypt);

    PRINTF_INFO("rssi-channel=%s, ", get_enabled(reg1 & 0x20));
    PRINTF_INFO("rssi-packet=%s, ", get_enabled(reg3 & 0x80));

    PRINTF_INFO("mode-listen-before-tx=%s, ", get_enabled(reg3 & 0x10));
    PRINTF_INFO("mode-transmit=%s, ", get_mode_transmit(reg3));
    PRINTF_INFO("mode-relay=%s, ", get_enabled(reg3 & 0x20));

#ifdef E22900T22_SUPPORT_MODULE_DIP
    if (_e22900txx_module == E22900T22_MODULE_DIP) {
        PRINTF_INFO("mode-wor-enable=%s, ", get_enabled(reg3 & 0x08));
        PRINTF_INFO("mode-wor-cycle=%s, ", get_wor_cycle(reg3));
    }
#endif

    PRINTF_INFO("uart-rate=%s, ", get_uart_rate(reg0));
    PRINTF_INFO("uart-parity=%s", get_uart_parity(reg0));

#ifdef E22900T22_SUPPORT_MODULE_USB
    if (_e22900txx_module == E22900T22_MODULE_USB)
        PRINTF_INFO(", switch-config-serial=%s", get_enabled(reg1 & 0x04));
#endif

    PRINTF_INFO("\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static void __update_config_bool(const char *name, uint8_t *byte, const uint8_t bits, const bool setting) {
    const bool value = (bool)(*byte & bits);
    if (value != setting) {
        PRINTF_INFO("device: update_configuration: %s: %s --> %s\n", name, get_enabled(value), get_enabled(setting));
        if (setting)
            *byte |= bits;
        else
            *byte &= ~bits;
    }
}

static bool update_configuration(uint8_t *config_device) {

    uint8_t config_device_orig[E22900T22_DEVICE_MOD_CONF_SIZE_WRITE];
    memcpy(config_device_orig, config_device, E22900T22_DEVICE_MOD_CONF_SIZE_WRITE);

    const uint16_t address = (uint16_t)config_device[0] << 8 | config_device[1];
    if (address != _e22900txx_config.address) {
        PRINTF_INFO("device: update_configuration: address: 0x%04" PRIX16 " --> 0x%04" PRIX16 "\n", address, _e22900txx_config.address);
        config_device[0] = (uint8_t)(_e22900txx_config.address >> 8);
        config_device[1] = (uint8_t)(_e22900txx_config.address & 0xFF);
    }
    const uint8_t network = config_device[2];
    if (network != _e22900txx_config.network) {
        PRINTF_INFO("device: update_configuration: network: 0x%02" PRIX8 " --> 0x%02" PRIX8 "\n", network, _e22900txx_config.network);
        config_device[2] = _e22900txx_config.network;
    }
    // XXX config_device[3] // serial_port_rate (7/6/5) / serial_port_bits (4/3)
    // XXX config_device[3] // packet_rate (2/1/0)
    // XXX config_device[4] // packet_size (7/6)
    __update_config_bool("rssi-channel", &config_device[4], 0x20, _e22900txx_config.rssi_channel);
    // XXX config_device[4] // reserved (4/3)
#ifdef E22900T22_SUPPORT_MODULE_USB
    if (_e22900txx_module == E22900T22_MODULE_USB)
        __update_config_bool("switch-config-serial", &config_device[4], 0x04, true);
#endif
    const uint8_t transmit_power = config_device[4] & 0x03;
    if (transmit_power != _e22900txx_config.transmit_power) {
        PRINTF_INFO("device: update_configuration: transmit_power: %" PRIu8 " (%s) --> %" PRIu8 " (%s)\n", transmit_power, get_transmit_power(transmit_power), _e22900txx_config.transmit_power,
                    get_transmit_power(_e22900txx_config.transmit_power));
        config_device[4] = (config_device[4] & (uint8_t)~0x03) | (_e22900txx_config.transmit_power & 0x03);
    }

    const uint8_t channel = config_device[5];
    if (channel != _e22900txx_config.channel) {
        const uint32_t frequency1000_a = get_frequency1000(channel), frequency1000_b = get_frequency1000(_e22900txx_config.channel);
        PRINTF_INFO("device: update_configuration: channel: %d (%" PRIu32 ".%03" PRIu32 "MHz) --> %d (%" PRIu32 ".%03" PRIu32 "MHz)\n", channel, frequency1000_a / 1000, frequency1000_a % 1000, _e22900txx_config.channel,
                    frequency1000_b / 1000, frequency1000_b % 1000);
        config_device[5] = _e22900txx_config.channel;
    }

    __update_config_bool("rssi-packet", &config_device[6], 0x80, _e22900txx_config.rssi_packet);
    // XXX config_device[6] // transmission_method (0x40) / relay_function (0x20) [NOT SUPPORTED]
    __update_config_bool("listen-before-transmit", &config_device[6], 0x10, _e22900txx_config.listen_before_transmit);
    __update_config_bool("wor-enabled", &config_device[6], 0x08, _e22900txx_config.wor_enabled);
    const uint16_t wor_cycle = (uint16_t)E22900T22_CONFIG_WOR_CYCLE_MIN + (uint16_t)((config_device[6] & 0x07) * E22900T22_CONFIG_WOR_CYCLE_INCREMENT);
    if (wor_cycle != _e22900txx_config.wor_cycle) {
        PRINTF_INFO("device: update_configuration: wor_cycle: %" PRIu16 "ms --> %" PRIu16 "ms\n", wor_cycle, _e22900txx_config.wor_cycle);
        config_device[6] = (config_device[6] & (uint8_t)~0x07) | (uint8_t)(((_e22900txx_config.wor_cycle - E22900T22_CONFIG_WOR_CYCLE_MIN) / E22900T22_CONFIG_WOR_CYCLE_INCREMENT) & 0x07);
    }
    const uint16_t crypt = (uint16_t)config_device[7] << 8 | config_device[8];
    if (crypt != _e22900txx_config.crypt) {
        PRINTF_INFO("device: update_configuration: crypt: 0x%04" PRIX16 " --> 0x%04" PRIX16 "\n", crypt, _e22900txx_config.crypt);
        config_device[7] = (uint8_t)(_e22900txx_config.crypt >> 8);
        config_device[8] = (uint8_t)(_e22900txx_config.crypt & 0xFF);
    }

    const bool update_required = memcmp(config_device_orig, config_device, E22900T22_DEVICE_MOD_CONF_SIZE_WRITE) != 0;

    return update_required;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static bool device_config(const e22900t22_config_t *config_device) {
    memcpy(&_e22900txx_config, config_device, sizeof(e22900t22_config_t));
    if (!_e22900txx_config.read_timeout_command)
        _e22900txx_config.read_timeout_command = E22900T22_CONFIG_READ_TIMEOUT_COMMAND_DEFAULT;
    if (!_e22900txx_config.read_timeout_packet)
        _e22900txx_config.read_timeout_packet = E22900T22_CONFIG_READ_TIMEOUT_PACKET_DEFAULT;
    if (!_e22900txx_config.packet_maxsize)
        _e22900txx_config.packet_maxsize = E22900T22_CONFIG_PACKET_MAXSIZE_DEFAULT;
    else if (_e22900txx_config.packet_maxsize > E22900T22_PACKET_MAXSIZE_240)
        return false;
    if (!_e22900txx_config.packet_maxrate)
        _e22900txx_config.packet_maxrate = E22900T22_CONFIG_PACKET_MAXRATE_DEFAULT;
    else if (_e22900txx_config.packet_maxrate > E22900T22_PACKET_MAXRATE_62500)
        return false;
#ifdef E22900T22_SUPPORT_MODULE_DIP
    if (_e22900txx_module == E22900T22_MODULE_DIP && (_e22900txx_config.set_pin_mx == NULL || _e22900txx_config.get_pin_aux == NULL))
        return false;
#endif
    return true;
}

static bool device_connect(const e22900t22_module_t config_module, const e22900t22_config_t *config_device) {

#ifndef E22900T22_SUPPORT_MODULE_USB
    if (config_module == E22900T22_MODULE_USB) {
        PRINTF_ERROR("device: no support for USB\n");
        return false;
    }
#endif
#ifndef E22900T22_SUPPORT_MODULE_DIP
    if (config_module == E22900T22_MODULE_DIP) {
        PRINTF_ERROR("device: no support for DIP\n");
        return false;
    }
#endif
    _e22900txx_module = config_module;

    if (!device_config(config_device)) {
        PRINTF_ERROR("device: failed to set config\n");
        return false;
    }

    return true;
}
static void device_disconnect(void) {
    PRINTF_DEBUG("device: disconnected\n");
}

static bool device_info_read(void) {

    uint8_t prd[E22900T22_DEVICE_PRD_INFO_SIZE];

    if (!device_product_info_read(prd)) {
        PRINTF_ERROR("device: failed to read product information\n");
        return false;
    }

    device_product_info_display(prd);

    _e22900txx_device.name = (uint16_t)prd[E22900T22_DEVICE_PRD_INFO_OFFSET_NAME_H] << 8 | prd[E22900T22_DEVICE_PRD_INFO_OFFSET_NAME_L];
    _e22900txx_device.version = prd[E22900T22_DEVICE_PRD_INFO_OFFSET_VERSION];
    _e22900txx_device.maxpower = prd[E22900T22_DEVICE_PRD_INFO_OFFSET_MAXPOWER];
    _e22900txx_device.frequency = prd[E22900T22_DEVICE_PRD_INFO_OFFSET_FREQUENCY];
    _e22900txx_device.type = prd[E22900T22_DEVICE_PRD_INFO_OFFSET_TYPE];

    return true;
}

static bool device_config_read_and_update(void) {

    uint8_t cfg[E22900T22_DEVICE_MOD_CONF_SIZE];

    if (!device_module_config_read(cfg)) {
        PRINTF_ERROR("device: failed to read module configuration\n");
        return false;
    }

    device_module_config_display(cfg);

    if (update_configuration(cfg)) {

        PRINTF_DEBUG("device: update module configuration\n");
        if (!device_module_config_write(cfg)) {
            PRINTF_ERROR("device: failed to write module configuration\n");
            return false;
        }

        __sleep_ms(50);

        PRINTF_DEBUG("device: verify module configuration\n");
        uint8_t cfg_2[E22900T22_DEVICE_MOD_CONF_SIZE];
        if (!device_module_config_read(cfg_2) || memcmp(cfg, cfg_2, sizeof(cfg)) != 0) {
            PRINTF_ERROR("device: failed to verify module configuration\n");
            return false;
        }
    }

    return true;
}

static void device_packet_read_and_display(volatile bool *is_active) {

    PRINTF_DEBUG("device: packet read and display (with periodic channel_rssi)\n");

    uint8_t packet_buffer[E22900T22_PACKET_MAXSIZE + 1];
    int packet_size;
    uint8_t rssi;

    while (*is_active) {
        if (device_packet_read(packet_buffer, _e22900txx_config.packet_maxsize + 1, &packet_size, &rssi) && *is_active)
            device_packet_display(packet_buffer, packet_size, rssi);
        else if (*is_active) {
            if (device_channel_rssi_read(&rssi) && *is_active)
                device_channel_rssi_display(rssi);
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

static const char *get_uart_rate(const uint8_t reg) {
    static const char *map[] = { "1200bps", "2400bps", "4800bps", "9600bps (Default)", "19200bps", "38400bps", "57600bps", "115200bps" };
    return map[(reg >> 5) & 0x07];
}

static const char *get_uart_parity(const uint8_t reg) {
    static const char *map[] = { "8N1 (Default)", "8O1", "8E1", "8N1" };
    return map[(reg >> 3) & 0x03];
}

static const char *get_packet_rate(const uint8_t reg) {
    static const struct __packet_rate_reg {
        const char *rates[8];
    } map[] = {
        // { { "0.3kbps", "1.2kbps", "2.4kbps (Default)", "4.8kbps", "9.6kbps", "19.2kbps", "38.4kbps", "62.5kbps" } }, // E22-900T22D/900T30D
        { { "2.4kbps", "2.4kbps", "2.4kbps (Default)", "4.8kbps", "9.6kbps", "19.2kbps", "38.4kbps", "62.5kbps" } }, // E22-400/900T22U
        { { "2.4kbps", "2.4kbps", "2.4kbps (Default)", "2.4kbps", "4.8kbps", "9.6kbps", "15.6kbps", "15.6kbps" } }   // E22-230T22U
    };
    switch (_e22900txx_device.frequency) {
    // case ??: return __packet_rate_map [1].rate_map [reg & 0x07]; // E22-230Txx
    // case ??: // E22-400Txx
    case 11:
        return map[0].rates[reg & 0x07]; // E22-900Txx
    default:
        return "UNKNOWN";
    }
}

static const char *get_packet_size(const uint8_t reg) {
    static const char *map[] = { "240bytes (Default)", "128bytes", "64bytes", "32bytes" };
    return map[(reg >> 6) & 0x03];
}

static const char *get_transmit_power(const uint8_t reg) {
    static const struct __transmit_power_reg {
        uint8_t max;
        const char *map[4];
    } map[] = {
        { 20, { "20dBm (Default)", "17dBm", "14dBm", "10dBm" } }, // E22-xxxT20
        { 22, { "22dBm (Default)", "17dBm", "13dBm", "10dBm" } }, // E22-xxxT22
        { 30, { "30dBm (Default)", "27dBm", "24dBm", "21dBm" } }, // E22-xxxT30
        { 33, { "33dBm (Default)", "30dBm", "27dBm", "24dBm" } }, // E22-xxxT33
    };
    for (int i = 0; i < (int)(sizeof(map) / sizeof(struct __transmit_power_reg)); i++)
        if (_e22900txx_device.maxpower == map[i].max)
            return map[i].map[reg & 0x03];
    return "UNKNOWN";
}

static const char *get_mode_transmit(const uint8_t reg) {
    static const char *map[] = { "fixed-point", "transparent" };
    return map[(reg >> 6) & 0x01];
}

static const char *get_wor_cycle(const uint8_t reg) {
    static const char *map[] = { "500ms", "1000ms", "1500ms", "2000ms (Default)", "2500ms", "3000ms", "3500ms", "4000ms" };
    return map[reg & 0x07];
}

static const char *get_enabled(const uint8_t value) {
    return value > 0 ? "on" : "off";
}

static uint32_t get_frequency1000(const uint8_t channel) {
    switch (_e22900txx_device.frequency) {
    // case ??: return 220.125 + (channel * 0.25); // E22-230Txx
    // case ??: return 410.125 + (channel * 1.0); // E22-400Txx
    case 11:
        return 850125 + (uint32_t)(channel * 1000); // E22-900Txx
    default:
        return 0;
    }
}

static int get_rssi_dbm(const uint8_t rssi) {
#ifdef E22900T22_SUPPORT_MODULE_DIP
    if (_e22900txx_module == E22900T22_MODULE_DIP)
        return -(256 - rssi);
#endif
#ifdef E22900T22_SUPPORT_MODULE_USB
    if (_e22900txx_module == E22900T22_MODULE_USB)
        return -(((int)rssi) / 2);
#endif
    return 0;
}

#pragma GCC diagnostic pop

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
