
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
#define SERIAL_READ_TIMEOUT 1000

#define COMMAND_DELAY_MS 300

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define DESIRED_ADDH 0x00
#define DESIRED_ADDL 0x02
#define DESIRED_NETID 0x00
#define DESIRED_CHANNEL 0x17 // Channel 23 (868.125 + 23 = 891.125 MHz)

const char *get_uart_baud_rate(unsigned char reg_value);
const char *get_uart_parity(unsigned char reg_value);
const char *get_air_data_rate(unsigned char reg_value);
const char *get_subpacket_size(unsigned char reg_value);
const char *get_transmission_power(unsigned char reg_value);

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void msleep(int milliseconds) { usleep(milliseconds * 1000); }

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

int serial_fd = -1;

bool serial_connect(const char *port, int baud_rate) {
    serial_fd = open(port, O_RDWR | O_NOCTTY);

    if (serial_fd < 0) {
        fprintf(stderr, "Error opening serial port: %s\n", strerror(errno));
        return false;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(serial_fd, &tty) != 0) {
        fprintf(stderr, "Error getting serial port attributes: %s\n", strerror(errno));
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
        fprintf(stderr, "Unsupported baud rate: %d\n", baud_rate);
        close(serial_fd);
        serial_fd = -1;
        return false;
    }

    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    tty.c_cflag |= (CLOCAL | CREAD); // Ignore modem controls
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
        fprintf(stderr, "Error setting serial port attributes: %s\n", strerror(errno));
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

bool cmd_send(const unsigned char *cmd, size_t cmd_len) {
    if (serial_fd < 0)
        return false;

    printf("command: send: (%ld bytes): ", cmd_len);
    for (size_t i = 0; i < cmd_len; i++)
        printf("%02X ", cmd[i]);
    printf("\n");

    return write(serial_fd, cmd, cmd_len) == (ssize_t)cmd_len;
}

int cmd_recv_response(unsigned char *buffer, size_t buffer_size, int timeout_ms) {
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

    int bytes_read = read(serial_fd, buffer, buffer_size);
    if (bytes_read > 0) {
        printf("command: recv: (%d bytes): ", bytes_read);
        for (int i = 0; i < bytes_read && i < 32; i++)
            printf("%02X ", buffer[i]);
        if (bytes_read > 32)
            printf("...");
        printf("\n");
    }

    return bytes_read;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool cmd_mode_switch(bool to_config_mode) {
    const unsigned char CMD_SWITCH_TO_CONFIG[6] = {0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x01};
    const unsigned char CMD_SWITCH_TO_TRANS[6] = {0xC0, 0xC1, 0xC2, 0xC3, 0x02, 0x00};
    const unsigned char *cmd = to_config_mode ? CMD_SWITCH_TO_CONFIG : CMD_SWITCH_TO_TRANS;
    int len = to_config_mode ? sizeof(CMD_SWITCH_TO_CONFIG) : sizeof(CMD_SWITCH_TO_TRANS);
    printf("Switching to %s mode...\n", to_config_mode ? "configuration" : "transmission");

    if (!cmd_send(cmd, len)) {
        fprintf(stderr, "Failed to send mode switch command\n");
        return false;
    }
    unsigned char response[3 + 2];
    int read_len = cmd_recv_response(response, sizeof(response), SERIAL_READ_TIMEOUT);
    if (read_len < 5) { // C1 + C2 + C3 + 02 + 1 bytes of data
        fprintf(stderr, "Failed to switch mode, received %d bytes\n", read_len);
        return false;
    }
    if (response[0] != 0xC1 || response[1] != 0xC2 || response[2] != 0xC3 || response[3] != 0x02) {
        fprintf(stderr, "Invalid response header for mode switch: %02X %02X %02X %02X\n", response[0], response[1],
                response[2], response[3]);
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool read_product_info(void) {

    serial_flush();

    unsigned char read_cmd[] = {0xC1, 0x80, 0x07};
    if (!cmd_send(read_cmd, sizeof(read_cmd))) {
        fprintf(stderr, "Failed to send read command for product info\n");
        return false;
    }

    unsigned char response[3 + 7];
    int read_len = cmd_recv_response(response, sizeof(response), SERIAL_READ_TIMEOUT);
    if (read_len < 10) { // C1 + address + length + 7 bytes of data
        fprintf(stderr, "Failed to read product information, received %d bytes\n", read_len);
        return false;
    }
    if (response[0] != 0xC1 || response[1] != 0x80 || response[2] != 0x07) {
        fprintf(stderr, "Invalid response header for product info: %02X %02X %02X\n", response[0], response[1],
                response[2]);
        return false;
    }

    printf("Product Information: ");
    for (int i = 0; i < 7; i++)
        printf("%02X ", response[3 + i]);
    printf("\n");
    printf("Product Info (ASCII): ");
    for (int i = 0; i < 7; i++) {
        if (isprint(response[3 + i])) {
            printf("%c", response[3 + i]);
        } else {
            printf(".");
        }
    }
    printf("\n");

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool read_configuration(unsigned char *config) {

    serial_flush();

    unsigned char read_cmd[] = {0xC1, 0x00, 0x09};
    if (!cmd_send(read_cmd, sizeof(read_cmd))) {
        fprintf(stderr, "Failed to send read command for configuration\n");
        return false;
    }

    unsigned char response[3 + 9];
    int read_len = cmd_recv_response(response, sizeof(response), SERIAL_READ_TIMEOUT);
    if (read_len < 12) { // C1 + address + length + 9 bytes of data
        fprintf(stderr, "Failed to read configuration, received %d bytes\n", read_len);
        return false;
    }
    if (response[0] != 0xC1 || response[1] != 0x00 || response[2] != 0x09) {
        fprintf(stderr, "Invalid response header for configuration: %02X %02X %02X\n", response[0], response[1],
                response[2]);
        return false;
    }

    memcpy(config, response + 3, 9);

    return true;
}

bool write_configuration(const unsigned char *config) {

    serial_flush();

    unsigned char write_cmd[3 + 7] = {0xC0, 0x00, 0x07};
    memcpy(write_cmd + 3, config, 7);
    if (!cmd_send(write_cmd, sizeof(write_cmd))) {
        fprintf(stderr, "Failed to send write command\n");
        return false;
    }

    unsigned char response[3 + 7];
    int read_len = cmd_recv_response(response, sizeof(response), SERIAL_READ_TIMEOUT);
    if (read_len < 10) { // C1 + address + length + 7 bytes of data
        fprintf(stderr, "Failed to read write response, received %d bytes\n", read_len);
        return false;
    }
    if (response[0] != 0xC1 || response[1] != 0x00 || response[2] != 0x07) {
        fprintf(stderr, "Invalid write response header: %02X %02X %02X\n", response[0], response[1], response[2]);
        return false;
    }
    for (int i = 0; i < 7; i++) {
        if (response[i + 3] != config[i]) {
            fprintf(stderr, "Write verification failed at offset %d: %02X != %02X\n", i, response[i + 3], config[i]);
            return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void print_configuration(const unsigned char *config) {
    printf("Module Configuration:\n");
    printf("---------------------\n");

    // Module address (ADDH, ADDL)
    printf("Module Address: 0x%02X%02X\n", config[0], config[1]);

    // Network ID (NETID)
    printf("Network ID: 0x%02X\n", config[2]);

    // REG0 - UART and Air Data Rate
    unsigned char reg0 = config[3];
    printf("UART Baud Rate: %s\n", get_uart_baud_rate(reg0));
    printf("UART Parity: %s\n", get_uart_parity(reg0));
    printf("Air Data Rate: %s\n", get_air_data_rate(reg0));

    // REG1 - Subpacket size and other settings
    unsigned char reg1 = config[4];
    printf("Subpacket Size: %s\n", get_subpacket_size(reg1));
    printf("RSSI Ambient Noise Enable: %s\n", (reg1 & 0x20) ? "Enabled" : "Disabled");
    printf("Software Mode Switching: %s\n", (reg1 & 0x04) ? "Enabled" : "Disabled");
    printf("Transmission Power: %s\n", get_transmission_power(reg1));

    // REG2 - Channel Control (CH)
    printf("Channel: %d (Actual frequency: %.3f MHz)\n", config[5], 850.125 + config[5]);

    // REG3 - Various options
    unsigned char reg3 = config[6];
    printf("RSSI Bytes Enable: %s\n", (reg3 & 0x80) ? "Enabled" : "Disabled");
    printf("Transmission Method: %s\n", (reg3 & 0x40) ? "Fixed-point" : "Transparent");
    printf("Relay Function: %s\n", (reg3 & 0x20) ? "Enabled" : "Disabled");
    printf("LBT Enable: %s\n", (reg3 & 0x10) ? "Enabled" : "Disabled");

    // CRYPT (not readable, will show as 0)
    printf("Encryption Key: 0x%02X%02X (Note: this value is write-only)\n", config[7], config[8]);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool update_configuration(unsigned char *config) {
    bool needs_update = false;

    if (config[0] != DESIRED_ADDH) {
        printf("Updating ADDH from 0x%02X to 0x%02X\n", config[0], DESIRED_ADDH);
        config[0] = DESIRED_ADDH;
        needs_update = true;
    }

    if (config[1] != DESIRED_ADDL) {
        printf("Updating ADDL from 0x%02X to 0x%02X\n", config[1], DESIRED_ADDL);
        config[1] = DESIRED_ADDL;
        needs_update = true;
    }

    if (config[2] != DESIRED_NETID) {
        printf("Updating Network ID from 0x%02X to 0x%02X\n", config[2], DESIRED_NETID);
        config[2] = DESIRED_NETID;
        needs_update = true;
    }

    if (!(config[4] & 0x20)) {
        printf("Enabling RSSI environmental noise\n");
        config[4] |= 0x20;
        needs_update = true;
    }

    if (!(config[4] & 0x04)) {
        printf("Enabling software mode switching\n");
        config[4] |= 0x04;
        needs_update = true;
    }

    if (!(config[6] & 0x80)) {
        printf("Enabling RSSI bytes\n");
        config[6] |= 0x80;
        needs_update = true;
    }

    if (config[5] != DESIRED_CHANNEL) {
        printf("Updating channel from %d to %d\n", config[5], DESIRED_CHANNEL);
        config[5] = DESIRED_CHANNEL;
        needs_update = true;
    }

    if (!needs_update) {
        printf("Configuration is already up to date\n");
        return false;
    }

    printf("Configuration needs updating\n");
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    const char *port = SERIAL_PORT_DEFAULT;
    int baud_rate = SERIAL_RATE_DEFAULT;
    unsigned char config[9];

    printf("E22-900T22U Configuration Manager\n");
    printf("--------------------------------\n");
    printf("Port: %s\n", port);
    printf("Baud Rate: %d\n\n", baud_rate);

    if (!serial_connect(port, baud_rate)) {
        fprintf(stderr, "Failed to connect to serial port\n");
        return EXIT_FAILURE;
    }
    printf("Device connected [transmission mode]\n");

    if (!cmd_mode_switch(true)) {
        fprintf(stderr, "Failed to switch to configuration mode\n");
        serial_disconnect();
        return EXIT_FAILURE;
    }
    printf("Device now in configuration mode\n");

    msleep(COMMAND_DELAY_MS);
    printf("\nReading product information...\n");
    if (!read_product_info()) {
        fprintf(stderr, "Failed to read product information...\n");
        serial_disconnect();
        return EXIT_FAILURE;
    }

    msleep(COMMAND_DELAY_MS);
    printf("\nReading configuration...\n");
    if (!read_configuration(config)) {
        fprintf(stderr, "Failed to read configuration...\n");
        serial_disconnect();
        return EXIT_FAILURE;
    }
    print_configuration(config);

    printf("\nChecking if configuration needs updates...\n");
    unsigned char config_2[9];
    if (update_configuration(config)) {
        msleep(COMMAND_DELAY_MS);
        printf("\nWriting updated configuration...\n");
        if (!write_configuration(config)) {
            fprintf(stderr, "Failed to write configuration...\n");
            serial_disconnect();
            return EXIT_FAILURE;
        }
        msleep(COMMAND_DELAY_MS);
        printf("\nVerifying updated configuration...\n");
        if (!read_configuration(config_2) || memcmp(config, config_2, sizeof(config)) != 0) {
            fprintf(stderr, "Failed to verify configuration updates\n");
            serial_disconnect();
            return EXIT_FAILURE;
        }
    }

    msleep(COMMAND_DELAY_MS);
    if (!cmd_mode_switch(false)) {
        fprintf(stderr, "Warning: Failed to switch back to transmission mode...\n");
        serial_disconnect();
        return EXIT_FAILURE;
    }

    serial_disconnect();
    printf("\nConfiguration management complete\n");
    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

const char *get_uart_baud_rate(unsigned char reg_value) {
    switch ((reg_value >> 5) & 0x07) {
    case 0:
        return "1200 bps";
    case 1:
        return "2400 bps";
    case 2:
        return "4800 bps";
    case 3:
        return "9600 bps (Default)";
    case 4:
        return "19200 bps";
    case 5:
        return "38400 bps";
    case 6:
        return "57600 bps";
    case 7:
        return "115200 bps";
    default:
        return "Unknown";
    }
}

const char *get_uart_parity(unsigned char reg_value) {
    switch ((reg_value >> 3) & 0x03) {
    case 0:
        return "8N1 (Default)";
    case 1:
        return "8O1";
    case 2:
        return "8E1";
    case 3:
        return "8N1 (Equivalent to 0)";
    default:
        return "Unknown";
    }
}

const char *get_air_data_rate(unsigned char reg_value) {
    switch (reg_value & 0x07) {
    case 0:
        return "2.4 kbps";
    case 1:
        return "2.4 kbps";
    case 2:
        return "2.4 kbps (Default)";
    case 3:
        return "4.8 kbps";
    case 4:
        return "9.6 kbps";
    case 5:
        return "19.2 kbps";
    case 6:
        return "38.4 kbps";
    case 7:
        return "62.5 kbps";
    default:
        return "Unknown";
    }
}

const char *get_subpacket_size(unsigned char reg_value) {
    switch ((reg_value >> 6) & 0x03) {
    case 0:
        return "240 Bytes (Default)";
    case 1:
        return "128 Bytes";
    case 2:
        return "64 Bytes";
    case 3:
        return "32 Bytes";
    default:
        return "Unknown";
    }
}

const char *get_transmission_power(unsigned char reg_value) {
    switch (reg_value & 0x03) {
    case 0:
        return "22 dBm (Default)";
    case 1:
        return "17 dBm";
    case 2:
        return "13 dBm";
    case 3:
        return "10 dBm";
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
