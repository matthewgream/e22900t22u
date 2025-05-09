
// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if !defined(E22900T22_SUPPORT_MODULE_DIP) && !defined(E22900T22_SUPPORT_MODULE_USB)
#error "no E22900T22_SUPPORT_MODULE_DIP or E22900T22_SUPPORT_MODULE_USB defined"
#endif

#define E22900T22_PACKET_MAXSIZE 240

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define CONFIG_ADDRESS_DEFAULT 0x0000
#define CONFIG_NETWORK_DEFAULT 0x00
#define CONFIG_CHANNEL_DEFAULT 0x00 // Channel 0 (850.125 + 0 = 850.125 MHz)
#define CONFIG_LISTEN_BEFORE_TRANSMIT true
#define CONFIG_RSSI_PACKET_DEFAULT true
#define CONFIG_RSSI_CHANNEL_DEFAULT true
#define CONFIG_READ_TIMEOUT_COMMAND_DEFAULT 1000
#define CONFIG_READ_TIMEOUT_PACKET_DEFAULT 5000
#define CONFIG_PACKET_MAXSIZE_DEFAULT E22900T22_PACKET_MAXSIZE

typedef struct {
    unsigned short name;
    unsigned char version, maxpower, frequency, type;
} e22900txx_device_t;

typedef enum { E22900T22_MODULE_USB = 0, E22900T22_MODULE_DIP = 1 } e22900t22_module_t;

typedef struct {
    unsigned short address;
    unsigned char network;
    unsigned char channel;
    unsigned char packet_maxsize;
    bool listen_before_transmit;
    bool rssi_packet, rssi_channel;
    unsigned long read_timeout_command, read_timeout_packet;
#ifdef E22900T22_SUPPORT_MODULE_DIP
    void (*set_pin_mx)(const bool pin_m0, const bool pin_m1);
    bool (*get_pin_aux)(void);
#endif
    bool debug;
} e22900t22_config_t;

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

e22900txx_device_t device = {.maxpower = 22};
e22900t22_module_t module;
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
int get_rssi_dbm(const unsigned char rssi);

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void __hexdump(const unsigned char *data, const int size, const char *prefix) {

    static const int bytes_per_line = 16;

    for (int offset = 0; offset < size; offset += bytes_per_line) {
        PRINTF_INFO("%s%04x: ", prefix, offset);
        for (int i = 0; i < bytes_per_line; i++) {
            if (i == bytes_per_line / 2)
                PRINTF_INFO(" ");
            if (offset + i < size)
                PRINTF_INFO("%02x ", data[offset + i]);
            else
                PRINTF_INFO("   ");
        }
        PRINTF_INFO(" ");
        for (int i = 0; i < bytes_per_line; i++) {
            if (i == bytes_per_line / 2)
                PRINTF_INFO(" ");
            if (offset + i < size)
                PRINTF_INFO("%c", isprint(data[offset + i]) ? data[offset + i] : '.');
            else
                PRINTF_INFO(" ");
        }
        PRINTF_INFO("\n");
    }
}

extern void __sleep_ms(const unsigned long ms);

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_wait_ready() {
#ifdef E22900T22_SUPPORT_MODULE_DIP
    if (module == E22900T22_MODULE_DIP) {
        static const unsigned long timeout_it = 1, timeout_ms = 30 * 1000;
        unsigned long timeout_counter = 0;
        while (!config.get_pin_aux()) {
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

bool device_packet_write(const unsigned char *packet, const int length) {
    if (length <= 0 || length > config.packet_maxsize)
        return false;
    return serial_write(packet, length) == length;
}

bool device_packet_read(unsigned char *packet, const int max_size, int *packet_size, unsigned char *rssi) {
    *packet_size = serial_read(packet, max_size, config.read_timeout_packet);
    if (*packet_size <= 0)
        return false;
    if (config.rssi_packet && *packet_size > 0)
        *rssi = packet[--*packet_size];
    else
        *rssi = 0;
    return true;
}

void device_packet_display(const unsigned char *packet, const int packet_size, const unsigned char rssi) {
    PRINTF_INFO("device: packet: size=%d", packet_size);
    if (config.rssi_packet)
        PRINTF_INFO(", rssi=%d dBm", get_rssi_dbm(rssi));
    PRINTF_INFO("\n");
    __hexdump(packet, packet_size, "    ");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_cmd_send(const unsigned char *cmd, const int cmd_len) {

    if (config.debug) {
        PRINTF_DEBUG("command: send: (%d bytes): ", cmd_len);
        for (int i = 0; i < cmd_len; i++)
            PRINTF_DEBUG("%02X ", cmd[i]);
        PRINTF_DEBUG("\n");
    }

    return serial_write(cmd, cmd_len) == cmd_len;
}

int device_cmd_recv_response(unsigned char *buffer, const int buffer_length, const int timeout_ms) {

    const int read_len = serial_read(buffer, buffer_length, timeout_ms);

    if (config.debug) {
        if (read_len > 0) {
            PRINTF_DEBUG("command: recv: (%d bytes): ", read_len);
            for (int i = 0; i < read_len && i < 32; i++)
                PRINTF_DEBUG("%02X ", buffer[i]);
            if (read_len > 32)
                PRINTF_DEBUG("...");
            PRINTF_DEBUG("\n");
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

    if (!device_cmd_send(command, command_length)) {
        PRINTF_ERROR("device: %s: failed to send command\n", name);
        return false;
    }

    unsigned char buffer[64];
    const int length = DEVICE_CMD_HEADER_SIZE + command[DEVICE_CMD_HEADER_LENGTH_OFFSET];
    const int read_len = device_cmd_recv_response(buffer, length, config.read_timeout_command);
    if (read_len < length) {
        PRINTF_ERROR("device: %s: failed to read response, received %d bytes, expected %d bytes\n", name, read_len,
                     length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != command[1] || buffer[2] != command[2]) {
        PRINTF_ERROR("device: %s: invalid response header: %02X %02X %02X\n", name, buffer[0], buffer[1], buffer[2]);
        return false;
    }

    memcpy(response, buffer + DEVICE_CMD_HEADER_SIZE, command[DEVICE_CMD_HEADER_LENGTH_OFFSET]);
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_channel_rssi_read(unsigned char *rssi) {

    static const char *name = "channel_rssi_read";
    static const unsigned char command[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x01};
    static const int command_length = sizeof(command);

    serial_flush();
    if (serial_write(command, command_length) != command_length) {
        PRINTF_ERROR("device: %s: failed to send command\n", name);
        return false;
    }

    unsigned char buffer[4];
    const int length = sizeof(buffer);
    const int read_len = serial_read(buffer, length, config.read_timeout_command);
    if (read_len < length) {
        PRINTF_ERROR("device: %s: failed, received %d bytes, expected %d bytes\n", name, read_len, length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != 0x00 || buffer[2] != 0x01) {
        PRINTF_ERROR("device: %s: invalid response header: %02X %02X %02X %02X\n", name, buffer[0], buffer[1],
                     buffer[2], buffer[3]);
        return false;
    }

    *rssi = buffer[3];
    return true;
}

void device_channel_rssi_display(unsigned char rssi) {
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

#ifdef E22900T22_SUPPORT_MODULE_USB
bool device_mode_switch_impl_software(const device_mode_t mode) {
    static const unsigned char cmd_switch_config[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x01};
    static const unsigned char cmd_switch_transfer[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x00};

    static const char *name = "mode_switch_software";
    const unsigned char *command = (mode == DEVICE_MODE_CONFIG) ? cmd_switch_config : cmd_switch_transfer;
    const int command_length = (mode == DEVICE_MODE_CONFIG) ? sizeof(cmd_switch_config) : sizeof(cmd_switch_transfer);

    serial_flush();
    if (!device_cmd_send(command, command_length)) {
        PRINTF_ERROR("device: %s: failed to send command\n", name);
        return false;
    }

    unsigned char buffer[64];
    const int length = command_length - 1;
    const int read_len = device_cmd_recv_response(buffer, length, config.read_timeout_command);
    if (read_len == 3 && (buffer[0] == 0xFF && buffer[1] == 0xFF && buffer[2] == 0xFF)) {
        PRINTF_INFO("device: %$s: already appears to be in required mode, will accept\n", name);
        return true;
    }
    if (read_len < length) {
        PRINTF_ERROR("device: %s: failed, received %d bytes, expected %d bytes\n", name, read_len, length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != 0xC2 || buffer[2] != 0xC3 || buffer[3] != 0x02) {
        PRINTF_ERROR("device: %s: invalid response header: %02X %02X %02X %02X\n", name, buffer[0], buffer[1],
                     buffer[2], buffer[3]);
        return false;
    }
    return true;
}
#endif

#ifdef E22900T22_SUPPORT_MODULE_DIP
bool device_mode_switch_impl_hardware(const device_mode_t mode) {
    static const char *name = "mode_switch_hardware";
    if (!device_wait_ready()) {
        PRINTF_ERROR("device: %s: wait_ready timeout (pre switch)\n", name);
        return false;
    }
    if (mode == DEVICE_MODE_CONFIG)
        config.set_pin_mx(false, true);
    else
        config.set_pin_mx(false, false);
    if (!device_wait_ready()) {
        PRINTF_ERROR("device: %s: wait_ready timeout (post switch)\n", name);
        return false;
    }
    return true;
}
#endif

bool device_mode_switch(const device_mode_t mode) {
    if (
#ifdef E22900T22_SUPPORT_MODULE_DIP
        (module == E22900T22_MODULE_DIP && !device_mode_switch_impl_hardware(mode))
#endif
#if defined(E22900T22_SUPPORT_MODULE_DIP) && defined(E22900T22_SUPPORT_MODULE_USB)
        ||
#endif
#ifdef E22900T22_SUPPORT_MODULE_USB
        (module == E22900T22_MODULE_USB && !device_mode_switch_impl_software(mode))
#endif
    )
        return false;
    static const char *name = "mode_switch";
    PRINTF_DEBUG("device: %s: --> %s\n", name, device_mode_str(mode));
    return true;
}
bool device_mode_config(void) { return device_mode_switch(DEVICE_MODE_CONFIG); }
bool device_mode_transfer(void) { return device_mode_switch(DEVICE_MODE_TRANSFER); }

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define DEVICE_PRODUCT_INFO_SIZE 7
#define DEVICE_PRODUCT_INFO_OFFSET_NAME_H 0
#define DEVICE_PRODUCT_INFO_OFFSET_NAME_L 1
#define DEVICE_PRODUCT_INFO_OFFSET_VERSION 2
#define DEVICE_PRODUCT_INFO_OFFSET_MAXPOWER 3
#define DEVICE_PRODUCT_INFO_OFFSET_FREQUENCY 4
#define DEVICE_PRODUCT_INFO_OFFSET_TYPE 5

bool device_product_info_read(unsigned char *result) {
    static const unsigned char cmd[] = {0xC1, 0x80, DEVICE_PRODUCT_INFO_SIZE};
    return device_cmd_send_wrapper("device_product_info_read", cmd, sizeof(cmd), result, DEVICE_PRODUCT_INFO_SIZE);
}

void device_product_info_display(const unsigned char *info) {
    PRINTF_INFO("device: product_info: ");
    PRINTF_INFO("name=%04X, version=%d, maxpower=%d, frequency=%d, type=%d",
                info[DEVICE_PRODUCT_INFO_OFFSET_NAME_H] << 8 | info[DEVICE_PRODUCT_INFO_OFFSET_NAME_L],
                info[DEVICE_PRODUCT_INFO_OFFSET_VERSION], info[DEVICE_PRODUCT_INFO_OFFSET_MAXPOWER],
                info[DEVICE_PRODUCT_INFO_OFFSET_FREQUENCY], info[DEVICE_PRODUCT_INFO_OFFSET_TYPE]);
    PRINTF_INFO(" [");
    for (int i = 0; i < DEVICE_PRODUCT_INFO_SIZE; i++)
        PRINTF_INFO("%s%02X", (i == 0 ? "" : " "), info[i]);
    PRINTF_INFO("]\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

// XXX it would be better to define a packed struct for the config rather than rely on offsets and bit manipulation

#define DEVICE_MODULE_CONF_SIZE 9
#define DEVICE_MODULE_CONF_SIZE_WRITE 7

bool device_module_config_read(unsigned char *config) {
    static const unsigned char cmd[] = {0xC1, 0x00, DEVICE_MODULE_CONF_SIZE};
    return device_cmd_send_wrapper("read_module_config", cmd, sizeof(cmd), config, DEVICE_MODULE_CONF_SIZE);
}

bool device_module_config_write(const unsigned char *config) {
    unsigned char cmd[DEVICE_CMD_HEADER_SIZE + DEVICE_MODULE_CONF_SIZE_WRITE] = {0xC0, 0x00,
                                                                                 DEVICE_MODULE_CONF_SIZE_WRITE};
    memcpy(cmd + DEVICE_CMD_HEADER_SIZE, config, DEVICE_MODULE_CONF_SIZE_WRITE);
    unsigned char result[DEVICE_MODULE_CONF_SIZE_WRITE];
    if (!device_cmd_send_wrapper("write_module_config", cmd, sizeof(cmd), result, DEVICE_MODULE_CONF_SIZE_WRITE))
        return false;
    for (int i = 0; i < DEVICE_MODULE_CONF_SIZE_WRITE; i++) {
        if (result[i] != config[i]) {
            PRINTF_ERROR("device: write_modify_config: verification failed at %d: %02X != %02X\n", i, result[i],
                         config[i]);
            return false;
        }
    }
    return true;
}

void device_module_config_display(const unsigned char *config_device) {

    const unsigned short address = config_device[0] << 8 | config_device[1]; // Module address (ADDH, ADDL)
    const unsigned char network = config_device[2];                          // Network ID (NETID)
    const unsigned char reg0 = config_device[3];                             // REG0 - UART and Air Data Rate
    const unsigned char reg1 = config_device[4];                             // REG1 - Subpacket size and other settings
    const unsigned char channel = config_device[5];                          // REG2 - Channel Control (CH)
    const unsigned char reg3 = config_device[6];                             // REG3 - Various options
    const unsigned short crypt = config_device[7] << 8 | config_device[8];   // CRYPT (not readable, will show as 0)

    PRINTF_INFO("device: module_config: ");

    PRINTF_INFO("address=0x%04X, ", address);
    PRINTF_INFO("network=0x%02X, ", network);
    PRINTF_INFO("channel=%d (frequency=%.3fMHz), ", channel, get_frequency(channel));

    PRINTF_INFO("data-rate=%s, ", get_packet_rate(reg0));
    PRINTF_INFO("packet-size=%s, ", get_packet_size(reg1));
    PRINTF_INFO("transmit-power=%s, ", get_transmit_power(reg1));
    PRINTF_INFO("encryption-key=0x%04X, ", crypt);

    PRINTF_INFO("rssi-channel=%s, ", get_enabled(reg1 & 0x20));
    PRINTF_INFO("rssi-packet=%s, ", get_enabled(reg3 & 0x80));

    PRINTF_INFO("mode-listen-before-tx=%s, ", get_enabled(reg3 & 0x10));
    PRINTF_INFO("mode-transmit=%s, ", get_mode_transmit(reg3));
    PRINTF_INFO("mode-relay=%s, ", get_enabled(reg3 & 0x20));

#ifdef E22900T22_SUPPORT_MODULE_DIP
    if (module == E22900T22_MODULE_DIP) {
        PRINTF_INFO("mode-wor-enable=%s, ", get_enabled(reg3 & 0x08));
        PRINTF_INFO("mode-wor-cycle=%s, ", get_wor_cycle(reg3));
    }
#endif

    PRINTF_INFO("uart-rate=%s, ", get_uart_rate(reg0));
    PRINTF_INFO("uart-parity=%s, ", get_uart_parity(reg0));

#ifdef E22900T22_SUPPORT_MODULE_USB
    if (module == E22900T22_MODULE_USB)
        PRINTF_INFO("switch-config-serial=%s, ", get_enabled(reg1 & 0x04));
#endif

    PRINTF_INFO("\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void __update_config_bool(const char *name, unsigned char *byte, const unsigned char bits, const bool setting) {
    const bool value = (bool)(*byte & bits);
    if (value != setting) {
        PRINTF_INFO("device: update_configuration: %s: %s --> %s\n", name, get_enabled(value), get_enabled(setting));
        if (setting)
            *byte |= bits;
        else
            *byte &= ~bits;
    }
}

bool update_configuration(unsigned char *config_device) {

    unsigned char config_device_orig[DEVICE_MODULE_CONF_SIZE_WRITE];
    memcpy(config_device_orig, config_device, DEVICE_MODULE_CONF_SIZE_WRITE);

    const unsigned short address = config_device[0] << 8 | config_device[1];
    if (address != config.address) {
        PRINTF_INFO("device: update_configuration: address: 0x%04X --> 0x%04X\n", address, config.address);
        config_device[0] = (unsigned char)(config.address >> 8);
        config_device[1] = (unsigned char)(config.address & 0xFF);
    }
    const unsigned char network = config_device[2];
    if (network != config.network) {
        PRINTF_INFO("device: update_configuration: network: 0x%02X --> 0x%02X\n", network, config.network);
        config_device[2] = config.network;
    }
    const unsigned char channel = config_device[5];
    if (channel != config.channel) {
        PRINTF_INFO("device: update_configuration: channel: %d (%.3fMHz) --> %d (%.3fMHz)\n", channel,
                    get_frequency(channel), config.channel, get_frequency(config.channel));
        config_device[5] = config.channel;
    }

    __update_config_bool("listen-before-transmit", &config_device[6], 0x10, config.listen_before_transmit);
    __update_config_bool("rssi-channel", &config_device[4], 0x20, config.rssi_channel);
    __update_config_bool("rssi-packet", &config_device[6], 0x80, config.rssi_packet);
#ifdef E22900T22_SUPPORT_MODULE_USB
    if (module == E22900T22_MODULE_USB)
        __update_config_bool("switch-config-serial", &config_device[4], 0x04, true);
#endif

    const bool update_required = memcmp(config_device_orig, config_device, DEVICE_MODULE_CONF_SIZE_WRITE) != 0;

    return update_required;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_config(const e22900t22_config_t *config_device) {
    memcpy(&config, config_device, sizeof(e22900t22_config_t));
    if (!config.read_timeout_command)
        config.read_timeout_command = CONFIG_READ_TIMEOUT_COMMAND_DEFAULT;
    if (!config.read_timeout_packet)
        config.read_timeout_packet = CONFIG_READ_TIMEOUT_PACKET_DEFAULT;
    if (!config.packet_maxsize)
        config.packet_maxsize = CONFIG_PACKET_MAXSIZE_DEFAULT;
    else if (config.packet_maxsize > E22900T22_PACKET_MAXSIZE)
        return false;
#ifdef E22900T22_SUPPORT_MODULE_DIP
    if (module == E22900T22_MODULE_DIP && (config.set_pin_mx == NULL || config.get_pin_aux == NULL))
        return false;
#endif
    return true;
}

bool device_connect(const e22900t22_module_t config_module, const e22900t22_config_t *config_device) {

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
    module = config_module;

    if (!device_config(config_device)) {
        PRINTF_ERROR("device: failed to set config\n");
        return false;
    }

    return true;
}
void device_disconnect() { PRINTF_DEBUG("device: disconnected\n"); }

bool device_info_read() {

    unsigned char product_info[DEVICE_PRODUCT_INFO_SIZE];

    if (!device_product_info_read(product_info)) {
        PRINTF_ERROR("device: failed to read product information\n");
        return false;
    }

    device_product_info_display(product_info);

    device.name =
        product_info[DEVICE_PRODUCT_INFO_OFFSET_NAME_H] << 8 | product_info[DEVICE_PRODUCT_INFO_OFFSET_NAME_L];
    device.version = product_info[DEVICE_PRODUCT_INFO_OFFSET_VERSION];
    device.maxpower = product_info[DEVICE_PRODUCT_INFO_OFFSET_MAXPOWER];
    device.frequency = product_info[DEVICE_PRODUCT_INFO_OFFSET_FREQUENCY];
    device.type = product_info[DEVICE_PRODUCT_INFO_OFFSET_TYPE];

    return true;
}

bool device_config_read_and_update() {

    unsigned char config[DEVICE_MODULE_CONF_SIZE];

    if (!device_module_config_read(config)) {
        PRINTF_ERROR("device: failed to read module configuration\n");
        return false;
    }

    device_module_config_display(config);

    if (update_configuration(config)) {

        PRINTF_DEBUG("device: update module configuration\n");
        if (!device_module_config_write(config)) {
            PRINTF_ERROR("device: failed to write module configuration\n");
            return false;
        }

        __sleep_ms(50);

        PRINTF_DEBUG("device: verify module configuration\n");
        unsigned char config_2[DEVICE_MODULE_CONF_SIZE_WRITE];
        if (!device_module_config_read(config_2) || memcmp(config, config_2, sizeof(config_2)) != 0) {
            PRINTF_ERROR("device: failed to verify module configuration\n");
            return false;
        }
    }

    return true;
}

void device_packet_read_and_display(volatile bool *is_active) {

    PRINTF_DEBUG("device: packet read and display (with periodic channel_rssi)\n");

    static const int max_packet_size = E22900T22_PACKET_MAXSIZE + 1; // RSSI
    unsigned char packet_buffer[max_packet_size];
    int packet_size;
    unsigned char rssi;

    while (*is_active) {
        if (device_packet_read(packet_buffer, config.packet_maxsize + 1, &packet_size, &rssi) && *is_active) {
            device_packet_display(packet_buffer, packet_size, rssi);
        } else if (*is_active) {
            if (device_channel_rssi_read(&rssi) && *is_active)
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
        return "NOT_REACHED";
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
        return "NOT_REACHED";
    }
}

const struct __packet_rate_reg {
    const char *rate_map[8];
} __packet_rate_map[] = {
    {{"2.4kbps", "2.4kbps", "2.4kbps (Default)", "4.8kbps", "9.6kbps", "19.2kbps", "38.4kbps",
      "62.5kbps"}}, // E22-400/900Txx
    {{"2.4kbps", "2.4kbps", "2.4kbps (Default)", "2.4kbps", "4.8kbps", "9.6kbps", "15.6kbps", "15.6kbps"}} // E22-230Txx
};

const char *get_packet_rate(const unsigned char value) {
    switch (device.frequency) {
    // case ??: return __packet_rate_map [1].rate_map [value & 0x07]; // E22-230Txx
    // case ??: // E22-400Txx
    case 11:
        return __packet_rate_map[0].rate_map[value & 0x07]; // E22-900Txx
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
        return "NOT_REACHED";
    }
}

const struct __transmit_power_reg {
    unsigned char power_max;
    const char *power_map[4];
} __transmit_power_map[] = {
    {20, {"20dBm (Default)", "17dBm", "14dBm", "10dBm"}}, // E22-xxxT20
    {22, {"22dBm (Default)", "17dBm", "13dBm", "10dBm"}}, // E22-xxxT22
    {30, {"30dBm (Default)", "27dBm", "24dBm", "21dBm"}}, // E22-xxxT30
    {33, {"33dBm (Default)", "30dBm", "27dBm", "24dBm"}}, // E22-xxxT33
};

const char *get_transmit_power(const unsigned char value) {
    for (int i = 0; i < (int)(sizeof(__transmit_power_map) / sizeof(struct __transmit_power_reg)); i++)
        if (device.maxpower == __transmit_power_map[i].power_max)
            return __transmit_power_map[i].power_map[value & 0x03];
    return "unknown";
}

const char *get_mode_transmit(const unsigned char value) {
    switch (value & 0x40) {
    case 0:
        return "fixed-point";
    case 1:
        return "transparent";
    default:
        return "NOT_REACHED";
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
        return "NOT_REACHED";
    }
}

const char *get_enabled(const unsigned char value) { return value > 0 ? "on" : "off"; }

float get_frequency(const unsigned char channel) {
    switch (device.frequency) {
    // case ??: return 220.125 + (channel * 0.25); // E22-230Txx
    // case ??: return 410.125 + (channel * 1.0); // E22-400Txx
    case 11:
        return 850.125 + (channel * 1.0); // E22-900Txx
    default:
        return 0.0;
    }
}

int get_rssi_dbm(const unsigned char rssi) {
#ifdef E22900T22_SUPPORT_MODULE_DIP
    if (module == E22900T22_MODULE_DIP)
        return -(256 - rssi);
#endif
#ifdef E22900T22_SUPPORT_MODULE_USB
    if (module == E22900T22_MODULE_USB)
        return -(((int)rssi) / 2);
#endif
    return 0;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
