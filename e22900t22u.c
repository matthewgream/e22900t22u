
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define SERIAL_PORT_DEFAULT "/dev/e22900t22u"
#define SERIAL_RATE_DEFAULT 9600

#define CONFIG_ADDRESS 0x0008
#define CONFIG_NETWORK 0x00
#define CONFIG_CHANNEL 0x17 // Channel 23 (868.125 + 23 = 891.125 MHz)
#define CONFIG_RSSI_PACKET 1
#define CONFIG_RSSI_CHANNEL 1

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define SERIAL_READ_TIMEOUT 1000
#define COMMAND_DELAY_MS 300

bool e22900t22u_debug = false;

const char *get_uart_rate(unsigned char value);
const char *get_uart_parity(unsigned char value);
const char *get_packet_rate(unsigned char value);
const char *get_packet_size(unsigned char value);
const char *get_transmit_power(unsigned char value);

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

int serial_fd = -1;

bool serial_connect(const char *port, int baud_rate) {

    serial_fd = open(port, O_RDWR | O_NOCTTY);
    if (serial_fd < 0) {
        fprintf(stderr, "serial: error opening port: %s\n", strerror(errno));
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_fd, &tty) != 0) {
        fprintf(stderr, "serial: error getting port attributes: %s\n", strerror(errno));
        close(serial_fd);
        serial_fd = -1;
        return false;
    }
    speed_t baud;
    switch (baud_rate) {
    case 1200:
        baud = B1200;
        break;
    case 2400:
        baud = B2400;
        break;
    case 4800:
        baud = B4800;
        break;
    case 9600:
        baud = B9600;
        break;
    case 19200:
        baud = B19200;
        break;
    case 38400:
        baud = B38400;
        break;
    case 57600:
        baud = B57600;
        break;
    case 115200:
        baud = B115200;
        break;
    default:
        fprintf(stderr, "serial: unsupported baud rate: %d\n", baud_rate);
        close(serial_fd);
        serial_fd = -1;
        return false;
    }
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      // 8-bit characters
    tty.c_cflag &= ~PARENB;  // No parity
    tty.c_cflag &= ~CSTOPB;  // 1 stop bit
    tty.c_cflag &= ~CRTSCTS; // No hardware flow control
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST; // Raw output
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;
    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "serial: error setting port attributes: %s\n", strerror(errno));
        close(serial_fd);
        serial_fd = -1;
        return false;
    }

    tcflush(serial_fd, TCIOFLUSH);

    return true;
}

void serial_disconnect(void) {
    if (serial_fd < 0)
        return;
    close(serial_fd);
    serial_fd = -1;
}

void serial_flush(void) {
    if (serial_fd < 0)
        return;
    tcflush(serial_fd, TCIOFLUSH);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool cmd_send(const unsigned char *cmd, int cmd_len) {
    if (serial_fd < 0)
        return false;

    usleep(COMMAND_DELAY_MS * 1000);

    if (e22900t22u_debug) {
        printf("command: send: (%d bytes): ", cmd_len);
        for (int i = 0; i < cmd_len; i++)
            printf("%02X ", cmd[i]);
        printf("\n");
    }

    return write(serial_fd, cmd, cmd_len) == (ssize_t)cmd_len;
}

int cmd_recv_response(unsigned char *buffer, int buffer_length, int timeout_ms) {
    if (serial_fd < 0)
        return -1;

    fd_set rdset;
    struct timeval tv;
    FD_ZERO(&rdset);
    FD_SET(serial_fd, &rdset);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int select_result = select(serial_fd + 1, &rdset, NULL, NULL, &tv);
    if (select_result <= 0)
        return select_result; // timeout or error
    const int bytes_read = read(serial_fd, buffer, buffer_length);

    if (e22900t22u_debug) {
        if (bytes_read > 0) {
            printf("command: recv: (%d bytes): ", bytes_read);
            for (int i = 0; i < bytes_read && i < 32; i++)
                printf("%02X ", buffer[i]);
            if (bytes_read > 32)
                printf("...");
            printf("\n");
        }
    }

    return bytes_read;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define DEVICE_CMD_HEADER_SIZE 3
#define DEVICE_CMD_HEADER_LENGTH_OFFSET 2

bool cmd_send_wrapper(const char *name, const unsigned char *command, const int command_length, unsigned char *response,
                      int response_length) {
    if (command_length < DEVICE_CMD_HEADER_SIZE)
        return false;
    if (response_length < command[DEVICE_CMD_HEADER_LENGTH_OFFSET])
        return false;
    if (!cmd_send(command, command_length)) {
        fprintf(stderr, "device: %s: failed to send command\n", name);
        return false;
    }
    unsigned char buffer[64];
    const int length = DEVICE_CMD_HEADER_SIZE + command[DEVICE_CMD_HEADER_LENGTH_OFFSET];
    const int read_len = cmd_recv_response(buffer, length, SERIAL_READ_TIMEOUT);
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

bool cmd_mode_switch(bool to_config_mode) {

    static const unsigned char cmd_switch_to_config[6] = {0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x01};
    static const unsigned char cmd_switch_to_transmit[6] = {0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x00};

    const char *name = "mode_switch";
    const unsigned char *command = to_config_mode ? cmd_switch_to_config : cmd_switch_to_transmit;
    const int command_length = to_config_mode ? sizeof(cmd_switch_to_config) : sizeof(cmd_switch_to_transmit);

    if (!cmd_send(command, command_length)) {
        fprintf(stderr, "device: %s: failed to send command\n", name);
        return false;
    }
    unsigned char buffer[64];
    const int length = command_length - 1;
    const int read_len = cmd_recv_response(buffer, length, SERIAL_READ_TIMEOUT);
    if (read_len < length) {
        fprintf(stderr, "device: %s: failed, received %d bytes, expected %d bytes\n", name, read_len, length);
        return false;
    }
    if (buffer[0] != 0xC1 || buffer[1] != 0xC2 || buffer[2] != 0xC3 || buffer[3] != 0x02) {
        fprintf(stderr, "device: %s: invalid response header: %02X %02X %02X %02X\n", name, buffer[0], buffer[1],
                buffer[2], buffer[3]);
        return false;
    }

    printf("device: %s: --> %s\n", name, to_config_mode ? "config" : "transmit");

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define DEVICE_PRODUCT_INFO_SIZE 7

bool device_product_info_read(unsigned char *result) {
    const unsigned char cmd[] = {0xC1, 0x80, DEVICE_PRODUCT_INFO_SIZE};
    return cmd_send_wrapper("device_product_info_read", cmd, sizeof(cmd), result, DEVICE_PRODUCT_INFO_SIZE);
}

void device_product_info_display(const unsigned char *info) {
    printf("device: product_info: ");
    for (int i = 0; i < DEVICE_PRODUCT_INFO_SIZE; i++)
        printf("%02X ", info[i]);
    printf("\n");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define DEVICE_MODULE_CONFIG_SIZE 9
#define DEVICE_MODULE_CONFIG_SIZE_WRITE 7

bool device_module_config_read(unsigned char *config) {
    const unsigned char cmd[] = {0xC1, 0x00, DEVICE_MODULE_CONFIG_SIZE};
    return cmd_send_wrapper("read_module_config", cmd, sizeof(cmd), config, DEVICE_MODULE_CONFIG_SIZE);
}

bool device_module_config_write(const unsigned char *config) {
    unsigned char cmd[DEVICE_CMD_HEADER_SIZE + DEVICE_MODULE_CONFIG_SIZE_WRITE] = {0xC0, 0x00,
                                                                                   DEVICE_MODULE_CONFIG_SIZE_WRITE};
    memcpy(cmd + DEVICE_CMD_HEADER_SIZE, config, DEVICE_MODULE_CONFIG_SIZE_WRITE);
    unsigned char result[DEVICE_MODULE_CONFIG_SIZE_WRITE];
    if (!cmd_send_wrapper("write_module_config", cmd, sizeof(cmd), result, DEVICE_MODULE_CONFIG_SIZE_WRITE))
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
    printf("device: module_config: ");

    // Module address (ADDH, ADDL)
    const unsigned short address = config[0] << 8 | config[1];
    // Network ID (NETID)
    const unsigned char network = config[2];
    // REG2 - Channel Control (CH)
    const unsigned char channel = config[5];
    const float frequency = 850.125 + channel;

    // REG0 - UART and Air Data Rate
    const unsigned char reg0 = config[3];
    // REG1 - Subpacket size and other settings
    const unsigned char reg1 = config[4];
    // REG3 - Various options
    const unsigned char reg3 = config[6];
    // CRYPT (not readable, will show as 0)
    const unsigned short crypt = config[7] << 8 | config[8];

    printf("address=0x%04X, ", address);
    printf("network=0x%02X, ", network);
    printf("channel=%d (frequency=%.3fMHz), ", channel, frequency);

    printf("data-rate=%s, ", get_packet_rate(reg0));
    printf("packet-size=%s, ", get_packet_size(reg1));
    printf("transmit-power=%s, ", get_transmit_power(reg1));
    printf("encryption-key=0x%04X, ", crypt);

    printf("channel-rssi=%s, ", (reg1 & 0x20) ? "on" : "off");
    printf("packet-rssi=%s, ", (reg3 & 0x80) ? "on" : "off");

    printf("listen-before-transmit=%s, ", (reg3 & 0x10) ? "on" : "off");
    printf("mode-transmit=%s, ", (reg3 & 0x40) ? "fixed" : "transparent");
    printf("mode-relay=%s, ", (reg3 & 0x20) ? "on" : "off");

    printf("uart-rate=%s, ", get_uart_rate(reg0));
    printf("uart-parity=%s, ", get_uart_parity(reg0));

    printf("switch-config-serial=%s\n", (reg1 & 0x04) ? "on" : "off");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool update_configuration(unsigned char *config) {
    unsigned char config_orig[DEVICE_MODULE_CONFIG_SIZE_WRITE];
    memcpy(config_orig, config, DEVICE_MODULE_CONFIG_SIZE_WRITE);

    const unsigned short address = config[0] << 8 | config[1];
    if (address != CONFIG_ADDRESS) {
        printf("device: update_configuration: address: 0x%04X --> 0x%04X\n", address, CONFIG_ADDRESS);
        config[0] = (unsigned char)(CONFIG_ADDRESS >> 8);
        config[1] = (unsigned char)(CONFIG_ADDRESS & 0xFF);
    }
    const unsigned char network = config[2];
    if (network != CONFIG_NETWORK) {
        printf("device: update_configuration: network: 0x%02X --> 0x%02X\n", network, CONFIG_NETWORK);
        config[2] = CONFIG_NETWORK;
    }
    const unsigned char channel = config[5];
    if (channel != CONFIG_CHANNEL) {
        printf("device: update_configuration: channel: %d --> %d\n", channel, CONFIG_CHANNEL);
        config[5] = CONFIG_CHANNEL;
    }

    const bool rssi_channel = (bool)(config[4] & 0x20);
    if (rssi_channel != (bool)CONFIG_RSSI_CHANNEL) {
        printf("device: update_configuration: channel-rssi: %s --> %s\n", rssi_channel ? "on" : "off",
               CONFIG_RSSI_CHANNEL ? "on" : "off");
        if (CONFIG_RSSI_CHANNEL)
            config[4] |= 0x20;
        else
            config[4] &= ~0x20;
    }
    const bool rssi_packet = (bool)(config[6] & 0x80);
    if (rssi_packet != (bool)CONFIG_RSSI_PACKET) {
        printf("device: update_configuration: packet-rssi: %s --> %s\n", rssi_packet ? "on" : "off",
               CONFIG_RSSI_PACKET ? "on" : "off");
        if (CONFIG_RSSI_PACKET)
            config[6] |= 0x80;
        else
            config[6] &= ~0x80;
    }

    const bool switch_config_serial = (bool)(config[4] & 0x04);
    if (!switch_config_serial) {
        printf("device: update_configuration: switch-config-serial: off --> on\n");
        config[4] |= 0x04;
    }

    return memcmp(config_orig, config, DEVICE_MODULE_CONFIG_SIZE_WRITE) != 0;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool device_connect(const char *port, const int rate) {
    if (!serial_connect(port, rate)) {
        fprintf(stderr, "device: failed to connect (port=%s, rate=%d)\n", port, rate);
        return false;
    }
    printf("device: connected (port=%s, rate=%d)\n", port, rate);
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

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    const char *port = SERIAL_PORT_DEFAULT;
    int rate = SERIAL_RATE_DEFAULT;
    printf("E22-900T22U\n");
    if (!device_connect(port, rate))
        return EXIT_FAILURE;
    bool code = EXIT_FAILURE;
    if (cmd_mode_switch(true) && device_info_display() && device_config_read_and_update() && cmd_mode_switch(false))
        code = EXIT_SUCCESS;
    device_disconnect();
    return code;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

const char *get_uart_rate(unsigned char value) {
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

const char *get_uart_parity(unsigned char value) {
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

const char *get_packet_rate(unsigned char value) {
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

const char *get_packet_size(unsigned char value) {
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

const char *get_transmit_power(unsigned char value) {
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

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
