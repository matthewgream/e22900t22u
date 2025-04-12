
// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

/*
 * E22-900T22U to MQTT
 */

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mosquitto.h>

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool debug_readandsend = false;
bool debug_e22900t22u = false;

void printf_debug(const char *format, ...) {
    if (debug_e22900t22u) {
        va_list args;
        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
    }
}
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

#define PRINTF_DEBUG printf_debug
#define PRINTF_ERROR printf_stderr
#define PRINTF_INFO printf_stdout

#include "include/serial_linux.h"
#undef E22900T22_SUPPORT_MODULE_DIP
#define E22900T22_SUPPORT_MODULE_USB
#include "include/e22900t22.h"
void __sleep_ms(const unsigned long ms) { usleep(ms * 1000); }

#define SERIAL_PORT_DEFAULT "/dev/e22900t22u"
#define SERIAL_RATE_DEFAULT 9600
#define SERIAL_BITS_DEFAULT SERIAL_8N1

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define CONFIG_FILE_DEFAULT "e22900t22utomqtt.cfg"

#include "include/config_linux.h"

// clang-format off
const struct option config_options [] = {
    {"config",                required_argument, 0, 0},
    {"mqtt-server",           required_argument, 0, 0},
    {"mqtt-topic",            required_argument, 0, 0},
    {"port",                  required_argument, 0, 0},
    {"rate",                  required_argument, 0, 0},
    {"bits",                  required_argument, 0, 0},
    {"address",               required_argument, 0, 0},
    {"network",               required_argument, 0, 0},
    {"channel",               required_argument, 0, 0},
    {"packet-size",           required_argument, 0, 0},
    {"listen-before-transmit",required_argument, 0, 0},
    {"rssi-packet",           required_argument, 0, 0},
    {"rssi-channel",          required_argument, 0, 0},
    {"read-timeout-command",  required_argument, 0, 0},
    {"read-timeout-packet",   required_argument, 0, 0},
    {"interval-stat",         required_argument, 0, 0},
    {"interval-rssi",         required_argument, 0, 0},
    {"debug-e22900t22u",      required_argument, 0, 0},
    {"debug",                 required_argument, 0, 0},
    {0, 0, 0, 0}
};
// clang-format on

void config_populate_serial(serial_config_t *config) {
    config->port = config_get_string("port", SERIAL_PORT_DEFAULT);
    config->rate = config_get_integer("rate", SERIAL_RATE_DEFAULT);
    config->bits = config_get_bits("bits", SERIAL_BITS_DEFAULT);

    printf("config: serial: port=%s, rate=%d, bits=%s\n", config->port, config->rate, serial_bits_str(config->bits));
}

void config_populate_e22900t22u(e22900t22_config_t *config) {
    config->address = (unsigned short)config_get_integer("address", CONFIG_ADDRESS_DEFAULT);
    config->network = (unsigned char)config_get_integer("network", CONFIG_NETWORK_DEFAULT);
    config->channel = (unsigned char)config_get_integer("channel", CONFIG_CHANNEL_DEFAULT);
    config->packet_maxsize = (unsigned char)config_get_integer("packet-size", CONFIG_PACKET_MAXSIZE_DEFAULT);
    config->listen_before_transmit = config_get_bool("listen-before-transmit", CONFIG_LISTEN_BEFORE_TRANSMIT);
    config->rssi_packet = config_get_bool("rssi-packet", CONFIG_RSSI_PACKET_DEFAULT);
    config->rssi_channel = config_get_bool("rssi-channel", CONFIG_RSSI_CHANNEL_DEFAULT);
    config->read_timeout_command =
        (unsigned long)config_get_integer("read-timeout-command", CONFIG_READ_TIMEOUT_COMMAND_DEFAULT);
    config->read_timeout_packet =
        (unsigned long)config_get_integer("read-timeout-packet", CONFIG_READ_TIMEOUT_PACKET_DEFAULT);
    config->debug = config_get_bool("debug", false);

    printf("config: e22900t22u: address=0x%04x, network=0x%02x, channel=%d, packet-size=%d, rssi-channel=%s, "
           "rssi-packet=%s, mode-listen-before-tx=%s, read-timeout-command=%lu, read-timeout-packet=%lu, debug=%s\n",
           config->address, config->network, config->channel, config->packet_maxsize,
           config->rssi_channel ? "on" : "off", config->rssi_packet ? "on" : "off",
           config->listen_before_transmit ? "on" : "off", config->read_timeout_command, config->read_timeout_packet,
           config->debug ? "on" : "off");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define MQTT_SERVER_DEFAULT "mqtt://localhost"
#define MQTT_TOPIC_DEFAULT "e22900t22u"

#define MQTT_CONNECT_TIMEOUT 60
#define MQTT_PUBLISH_QOS 0
#define MQTT_PUBLISH_RETAIN false

#include "include/mqtt_linux.h"

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define INTERVAL_STAT_DEFAULT 5 * 60
#define INTERVAL_RSSI_DEFAULT 1 * 60

volatile bool is_active = true;

bool packet_is_reasonable_json(const unsigned char *packet, const int length) {
    if (length < 2)
        return false;
    if (packet[0] != '{' || packet[length - 1] != '}')
        return false;
    for (int index = 0; index < length; index++)
        if (!isprint(packet[index]))
            return false;
    return true;
}

bool capture_rssi_packet = false, capture_rssi_channel = false;
unsigned long stat_channel_rssi_cnt = 0, stat_packet_rssi_cnt = 0;
unsigned char stat_channel_rssi_ema, stat_packet_rssi_ema;
unsigned long stat_packets_okay = 0, stat_packets_drop = 0;
int interval_stat = 0, interval_rssi = 0;
time_t interval_stat_last = 0, interval_rssi_last = 0;

int interval_passed(int interval, time_t *last) {
    time_t now = time(NULL);
    if (*last == 0) {
        *last = now;
        return 0;
    }
    if ((now - *last) > interval) {
        const int diff = now - *last;
        *last = now;
        return diff;
    }
    return 0;
}

void ema_update(unsigned char value, unsigned char *value_ema, unsigned long *value_cnt) {
    *value_ema = (*value_cnt)++ == 0 ? value : (unsigned char)((0.2f * (float)value) + ((1 - 0.2f) * (*value_ema)));
}

void read_and_send(const char *mqtt_topic) {

    const int max_packet_size = E22900T22_PACKET_MAXSIZE + 1; // +1 for RSSI
    unsigned char packet_buffer[max_packet_size];
    int packet_size;

    printf("read-and-publish (topic='%s', stat=%ds, rssi=%ds [packets=%c, channel=%c])\n", mqtt_topic, interval_stat,
           interval_rssi, capture_rssi_packet ? 'y' : 'n', capture_rssi_channel ? 'y' : 'n');

    while (is_active) {

        unsigned char packet_rssi, channel_rssi;

        if (device_packet_read(packet_buffer, config.packet_maxsize + 1, &packet_size, &packet_rssi) && is_active) {
            if (!packet_is_reasonable_json(packet_buffer, packet_size)) {
                fprintf(stderr, "read-and-publish: discarding malformed packet (size=%d)\n", packet_size);
                stat_packets_drop++;
            } else {
                if (capture_rssi_packet)
                    ema_update(packet_rssi, &stat_packet_rssi_ema, &stat_packet_rssi_cnt);
                mqtt_send(mqtt_topic, (const char *)packet_buffer, packet_size);
                stat_packets_okay++;
            }
            if (debug_readandsend)
                device_packet_display(packet_buffer, packet_size, packet_rssi);
        }

        if (is_active && capture_rssi_channel && interval_passed(interval_rssi, &interval_rssi_last)) {
            if (device_channel_rssi_read(&channel_rssi) && is_active)
                ema_update(channel_rssi, &stat_channel_rssi_ema, &stat_channel_rssi_cnt);
        }

        int period_stat;
        if (is_active && (period_stat = interval_passed(interval_stat, &interval_stat_last))) {
            printf("packets-okay=%ld (%.2f/min), packets-drop=%ld (%.2f/min)", stat_packets_okay,
                   ((float)stat_packets_okay / ((float)period_stat / 60.0f)), stat_packets_drop,
                   ((float)stat_packets_drop / ((float)period_stat / 60.0f)));
            stat_packets_okay = stat_packets_drop = 0;
            if (capture_rssi_channel)
                printf(", channel-rssi=%d dBm (%ld)", get_rssi_dbm(stat_channel_rssi_ema), stat_channel_rssi_cnt);
            if (capture_rssi_packet)
                printf(", packet-rssi=%d dbm (%ld)", get_rssi_dbm(stat_packet_rssi_ema), stat_packet_rssi_cnt);
            printf("\n");
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

serial_config_t serial_config;
e22900t22_config_t e22900t22u_config;
const char *mqtt_server, *mqtt_topic;

bool config_setup(const int argc, const char *argv[]) {

    if (!config_load(CONFIG_FILE_DEFAULT, argc, argv, config_options))
        return false;

    config_populate_serial(&serial_config);
    config_populate_e22900t22u(&e22900t22u_config);
    mqtt_server = config_get_string("mqtt-server", MQTT_SERVER_DEFAULT);
    mqtt_topic = config_get_string("mqtt-topic", MQTT_TOPIC_DEFAULT);

    capture_rssi_packet = config_get_bool("rssi-packet", CONFIG_RSSI_PACKET_DEFAULT);
    capture_rssi_channel = config_get_bool("rssi-channel", CONFIG_RSSI_CHANNEL_DEFAULT);
    interval_stat = config_get_integer("interval-stat", INTERVAL_STAT_DEFAULT);
    interval_rssi = config_get_integer("interval-rssi", INTERVAL_RSSI_DEFAULT);

    debug_e22900t22u = config_get_integer("debug-e22900t22u", false);
    debug_readandsend = config_get_bool("debug", false);

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

void signal_handler(int sig __attribute__((unused))) {
    if (is_active) {
        printf("stopping\n");
        is_active = false;
    }
}

int main(int argc, const char *argv[]) {

    printf("starting\n");

    setbuf(stdout, NULL);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!config_setup(argc, argv))
        return EXIT_FAILURE;

    if (!mqtt_begin(mqtt_server))
        return EXIT_FAILURE;

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

    read_and_send(mqtt_topic);

    device_disconnect();
    serial_disconnect();
    mqtt_end();

    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
