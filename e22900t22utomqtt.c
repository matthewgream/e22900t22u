
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

#include "include/util_linux.h"

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
#include "include/e22xxxtxx.h"

void __sleep_ms(const unsigned long ms) { usleep(ms * 1000); }

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define CONFIG_FILE_DEFAULT "e22900t22utomqtt.cfg"

#define SERIAL_PORT_DEFAULT "/dev/e22900t22u"
#define SERIAL_RATE_DEFAULT 9600
#define SERIAL_BITS_DEFAULT SERIAL_8N1

#define MQTT_CLIENT_DEFAULT "e22900t22utomqtt"
#define MQTT_SERVER_DEFAULT "mqtt://localhost"
#define MQTT_TOPIC_DEFAULT "e22900t22u"

#define INTERVAL_STAT_DEFAULT 5 * 60
#define INTERVAL_RSSI_DEFAULT 1 * 60

#define DATA_TYPE_TYPE_DEFAULT "json-convert"

#include "include/config_linux.h"

// clang-format off
const struct option config_options [] = {
    {"config",                required_argument, 0, 0},
    {"mqtt-client",           required_argument, 0, 0},
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
    {"data-type",             required_argument, 0, 0},
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

#define MQTT_CONNECT_TIMEOUT 60
#define MQTT_PUBLISH_QOS 0
#define MQTT_PUBLISH_RETAIN false

#include "include/mqtt_linux.h"

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

typedef enum {
    DATA_TYPE_JSON = 0,
    DATA_TYPE_ANY = 1,
    DATA_TYPE_JSON_CONVERT = 2,
} data_type_t;

data_type_t data_type_parse(const char *data_type_str) {
    if (strcmp(data_type_str, "json") == 0)
        return DATA_TYPE_JSON;
    else if (strcmp(data_type_str, "json-convert") == 0)
        return DATA_TYPE_JSON_CONVERT;
    else if (strcmp(data_type_str, "any") == 0)
        return DATA_TYPE_ANY;
    else {
        fprintf(stderr, "warning: unknown data-type '%s', using default 'json-convert'\n", data_type_str);
        return DATA_TYPE_JSON_CONVERT;
    }
}
const char *data_type_tostring(const data_type_t data_type) {
    switch (data_type) {
    case DATA_TYPE_JSON:
        return "json";
    case DATA_TYPE_JSON_CONVERT:
        return "json-convert";
    case DATA_TYPE_ANY:
    default:
        return "any";
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool capture_rssi_packet = false, capture_rssi_channel = false;
unsigned long stat_channel_rssi_cnt = 0, stat_packet_rssi_cnt = 0;
unsigned char stat_channel_rssi_ema, stat_packet_rssi_ema;
unsigned long stat_packets_okay = 0, stat_packets_drop = 0;
time_t interval_stat = 0, interval_stat_last = 0;
time_t interval_rssi = 0, interval_rssi_last = 0;
#define PACKET_BUFFER_MAX ((E22900T22_PACKET_MAXSIZE * 2) + 4) // has +1 for RSSI; adds 2 for '["' <HEX> '"]'

void read_and_send(volatile bool *running, const data_type_t data_type, const char *mqtt_topic) {

    unsigned char packet_buffer[PACKET_BUFFER_MAX];
    int packet_size;

    printf("read-and-publish (topic='%s', stat=%lds, rssi=%lds [packets=%c, channel=%c], data-type=%s)\n", mqtt_topic,
           interval_stat, interval_rssi, capture_rssi_packet ? 'y' : 'n', capture_rssi_channel ? 'y' : 'n',
           data_type_tostring(data_type));

    while (*running) {

        unsigned char packet_rssi = 0, channel_rssi = 0;

        if (device_packet_read(packet_buffer, config.packet_maxsize + 1, &packet_size, &packet_rssi) && *running) {
            bool deliver = false;
            switch (data_type) {
            case DATA_TYPE_JSON:
                if (!(deliver = is_reasonable_json(packet_buffer, packet_size))) {
                    fprintf(stderr, "read-and-publish: discarding non-json packet (size=%d)\n", packet_size);
                    stat_packets_drop++;
                }
                break;
            case DATA_TYPE_JSON_CONVERT:
                if (!is_reasonable_json(packet_buffer, packet_size)) {
                    const int json_size = 4 + (packet_size * 2);
                    if (json_size >= PACKET_BUFFER_MAX) {
                        fprintf(stderr, "read-and-publish: packet too large for conversion (size=%d)\n", packet_size);
                        deliver = false;
                        stat_packets_drop++;
                        break;
                    }
                    int data_offset = PACKET_BUFFER_MAX - packet_size;
                    memmove(packet_buffer + data_offset, packet_buffer, packet_size);
                    packet_buffer[0] = '[';
                    packet_buffer[1] = '"';
                    for (int i = 0; i < packet_size; i++) {
                        const unsigned char byte = packet_buffer[data_offset + i];
                        packet_buffer[2 + (i * 2)] = "0123456789abcdef"[byte >> 4];
                        packet_buffer[2 + (i * 2) + 1] = "0123456789abcdef"[byte & 0x0f];
                    }
                    packet_buffer[2 + (packet_size * 2)] = '"';
                    packet_buffer[2 + (packet_size * 2) + 1] = ']';
                    packet_size = json_size;
                }
                deliver = true;
                break;
            case DATA_TYPE_ANY:
                deliver = true;
                break;
            }
            if (deliver) {
                if (capture_rssi_packet)
                    ema_update(packet_rssi, &stat_packet_rssi_ema, &stat_packet_rssi_cnt);
                mqtt_send(mqtt_topic, (const char *)packet_buffer, packet_size);
                stat_packets_okay++;
            }
            if (debug_readandsend)
                device_packet_display(packet_buffer, packet_size, packet_rssi);
        }

        if (*running && capture_rssi_channel && intervalable(interval_rssi, &interval_rssi_last)) {
            if (device_channel_rssi_read(&channel_rssi) && *running)
                ema_update(channel_rssi, &stat_channel_rssi_ema, &stat_channel_rssi_cnt);
        }

        int period_stat;
        if (*running && (period_stat = intervalable(interval_stat, &interval_stat_last))) {
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
const char *mqtt_client, *mqtt_server, *mqtt_topic;
data_type_t data_type;

bool config_setup(const int argc, const char *argv[]) {

    if (!config_load(CONFIG_FILE_DEFAULT, argc, argv, config_options))
        return false;

    config_populate_serial(&serial_config);
    config_populate_e22900t22u(&e22900t22u_config);
    mqtt_client = config_get_string("mqtt-client", MQTT_CLIENT_DEFAULT);
    mqtt_server = config_get_string("mqtt-server", MQTT_SERVER_DEFAULT);
    mqtt_topic = config_get_string("mqtt-topic", MQTT_TOPIC_DEFAULT);

    capture_rssi_packet = config_get_bool("rssi-packet", CONFIG_RSSI_PACKET_DEFAULT);
    capture_rssi_channel = config_get_bool("rssi-channel", CONFIG_RSSI_CHANNEL_DEFAULT);
    interval_stat = config_get_integer("interval-stat", INTERVAL_STAT_DEFAULT);
    interval_rssi = config_get_integer("interval-rssi", INTERVAL_RSSI_DEFAULT);

    data_type = data_type_parse(config_get_string("data-type", DATA_TYPE_TYPE_DEFAULT));

    debug_e22900t22u = config_get_integer("debug-e22900t22u", false);
    debug_readandsend = config_get_bool("debug", false);

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

volatile bool running = true;

void signal_handler(const int sig __attribute__((unused))) {
    if (running) {
        printf("stopping\n");
        running = false;
    }
}

int main(const int argc, const char *argv[]) {

    printf("starting\n");

    setbuf(stdout, NULL);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!config_setup(argc, argv))
        return EXIT_FAILURE;

    if (!serial_begin(&serial_config) || !serial_connect()) {
        fprintf(stderr, "device: failed to connect (port=%s, rate=%d, bits=%s)\n", serial_config.port,
                serial_config.rate, serial_bits_str(serial_config.bits));
        return EXIT_FAILURE;
    }
    if (!device_connect(E22900T22_MODULE_USB, &e22900t22u_config)) {
        serial_end();
        return EXIT_FAILURE;
    }
    printf("device: connected (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate,
           serial_bits_str(serial_config.bits));
    if (!(device_mode_config() && device_info_read() && device_config_read_and_update() && device_mode_transfer())) {
        device_disconnect();
        serial_end();
        return EXIT_FAILURE;
    }

    if (!mqtt_begin(mqtt_server, mqtt_client)) {
        device_disconnect();
        serial_end();
        return EXIT_FAILURE;
    }

    read_and_send(&running, data_type, mqtt_topic);

    device_disconnect();
    serial_end();
    mqtt_end();

    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
