
// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

/*
 * E22-900T22U to MQTT
 */

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <inttypes.h>
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
    if (!debug_e22900t22u)
        return;
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
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
#define PRINTF_INFO  printf_stdout

#include "include/serial_linux.h"

#undef E22900T22_SUPPORT_MODULE_DIP
#define E22900T22_SUPPORT_MODULE_USB
#include "include/e22xxxtxx.h"

void __sleep_ms(const uint32_t ms) {
    usleep((useconds_t)ms * 1000);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define CONFIG_FILE_DEFAULT    "e22900t22utomqtt.cfg"

#define SERIAL_PORT_DEFAULT    "/dev/e22900t22u"
#define SERIAL_RATE_DEFAULT    9600
#define SERIAL_BITS_DEFAULT    SERIAL_8N1

#define MQTT_CLIENT_DEFAULT    "e22900t22utomqtt"
#define MQTT_SERVER_DEFAULT    "mqtt://localhost"
#define MQTT_TOPIC_DEFAULT     "e22900t22u"

#define INTERVAL_STAT_DEFAULT  5 * 60
#define INTERVAL_RSSI_DEFAULT  1 * 60

#define DATA_TYPE_TYPE_DEFAULT "json-convert"

#include "include/config_linux.h"

// clang-format off
const struct option config_options [] = {
    {"config",                required_argument, 0, 0},
    {"mqtt-client",           required_argument, 0, 0},
    {"mqtt-server",           required_argument, 0, 0},
    {"port",                  required_argument, 0, 0},
    {"rate",                  required_argument, 0, 0},
    {"bits",                  required_argument, 0, 0},
    {"address",               required_argument, 0, 0},
    {"network",               required_argument, 0, 0},
    {"channel",               required_argument, 0, 0},
    {"packet-size",           required_argument, 0, 0},
    {"packet-rate",           required_argument, 0, 0},
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

void config_populate_serial(serial_config_t *cfg) {
    cfg->port = config_get_string("port", SERIAL_PORT_DEFAULT);
    cfg->rate = config_get_integer("rate", SERIAL_RATE_DEFAULT);
    cfg->bits = config_get_bits("bits", SERIAL_BITS_DEFAULT);

    printf("config: serial: port=%s, rate=%d, bits=%s\n", cfg->port, cfg->rate, serial_bits_str(cfg->bits));
}

void config_populate_e22900t22u(e22900t22_config_t *cfg) {
    cfg->address = (uint16_t)config_get_integer("address", E22900T22_CONFIG_ADDRESS_DEFAULT);
    cfg->network = (uint8_t)config_get_integer("network", E22900T22_CONFIG_NETWORK_DEFAULT);
    cfg->channel = (uint8_t)config_get_integer("channel", E22900T22_CONFIG_CHANNEL_DEFAULT);
    cfg->packet_maxsize = (uint8_t)config_get_integer("packet-size", E22900T22_CONFIG_PACKET_MAXSIZE_DEFAULT);
    cfg->packet_maxrate = (uint8_t)config_get_integer("packet-rate", E22900T22_CONFIG_PACKET_MAXRATE_DEFAULT);
    cfg->crypt = E22900T22_CONFIG_CRYPT_DEFAULT;
    cfg->wor_enabled = E22900T22_CONFIG_WOR_ENABLED_DEFAULT;
    cfg->wor_cycle = E22900T22_CONFIG_WOR_CYCLE_DEFAULT;
    cfg->transmit_power = E22900T22_CONFIG_TRANSMIT_POWER_DEFAULT;
    cfg->transmission_method = E22900T22_CONFIG_TRANSMISSION_METHOD_DEFAULT;
    cfg->relay_enabled = E22900T22_CONFIG_RELAY_ENABLED_DEFAULT;
    cfg->listen_before_transmit = config_get_bool("listen-before-transmit", E22900T22_CONFIG_LISTEN_BEFORE_TRANSMIT);
    cfg->rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    cfg->rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
    cfg->read_timeout_command = (uint32_t)config_get_integer("read-timeout-command", E22900T22_CONFIG_READ_TIMEOUT_COMMAND_DEFAULT);
    cfg->read_timeout_packet = (uint32_t)config_get_integer("read-timeout-packet", E22900T22_CONFIG_READ_TIMEOUT_PACKET_DEFAULT);
    cfg->debug = config_get_bool("debug", false);

    printf("config: e22900t22u: address=0x%04" PRIX16 ", network=0x%02" PRIX8 ", channel=%d, packet-size=%d, packet-rate=%d, rssi-channel=%s, rssi-packet=%s, mode-listen-before-tx=%s, read-timeout-command=%" PRIu32
           ", read-timeout-packet=%" PRIu32 ", crypt=%04" PRIX16 ", wor=%s, wor-cycle=%" PRIu16 "ms, transmit-power=%" PRIu8 ", transmission-method=%s, mode-relay=%s, debug=%s\n",
           cfg->address, cfg->network, cfg->channel, cfg->packet_maxsize, cfg->packet_maxrate, cfg->rssi_channel ? "on" : "off", cfg->rssi_packet ? "on" : "off", cfg->listen_before_transmit ? "on" : "off", cfg->read_timeout_command,
           cfg->read_timeout_packet, cfg->crypt, cfg->wor_enabled ? "on" : "off", cfg->wor_cycle, cfg->transmit_power, cfg->transmission_method == E22900T22_CONFIG_TRANSMISSION_METHOD_TRANSPARENT ? "transparent" : "fixed-point",
           cfg->relay_enabled ? "on" : "off", cfg->debug ? "on" : "off");
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define MQTT_CONNECT_TIMEOUT 60
#define MQTT_PUBLISH_QOS     0
#define MQTT_PUBLISH_RETAIN  false

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

#define MAX_TOPIC_ROUTES 16

typedef struct {
    const char *key;
    const char *value;
    const char *topic;
} topic_route_t;

topic_route_t topic_routes[MAX_TOPIC_ROUTES];
const char *topic_route_default = NULL;
size_t topic_route_count = 0;

void config_populate_topic_routes(const char *topic_default) {
    topic_route_default = topic_default;
    topic_route_count = 0;
    for (int i = 0; i < MAX_TOPIC_ROUTES; i++) {
        char key_name[64];
        snprintf(key_name, sizeof(key_name), "topic-route.%d.key", i);
        const char *key = config_get_string(key_name, NULL);
        if (!key)
            continue;
        char value_name[64], topic_name[64];
        snprintf(value_name, sizeof(value_name), "topic-route.%d.value", i);
        snprintf(topic_name, sizeof(topic_name), "topic-route.%d.topic", i);
        const char *value = config_get_string(value_name, NULL);
        const char *topic = config_get_string(topic_name, NULL);
        if (value && topic) {
            topic_routes[topic_route_count].key = key;
            topic_routes[topic_route_count].value = value;
            topic_routes[topic_route_count].topic = topic;
            printf("config: topic-route[%d]: key='%s', value='%s', topic='%s'\n", (int)topic_route_count, key, value, topic);
            topic_route_count++;
        }
    }
    if (topic_route_count == 0)
        printf("config: no topic routes configured, using default topic\n");
}
bool route_topic_match_json(const uint8_t *packet, const int packet_size, const char *key, const char *value) {
    char search_pattern[64 + 64 + 64];
    const int pattern_len = snprintf(search_pattern, sizeof(search_pattern), "\"%s\":\"%s\"", key, value);
    if (pattern_len >= packet_size)
        return false;
    const uint8_t first_char = (uint8_t)search_pattern[0];
    for (int i = 0; i <= packet_size - pattern_len; i++)
        if (packet[i] == first_char && memcmp(packet + i, search_pattern, (size_t)pattern_len) == 0)
            return true;
    return false;
}
bool route_topic_match_binary(const uint8_t *packet, const int packet_size, const char *key, const char *value) {
    const int offset = atoi(key);
    if (offset < 0 || offset >= packet_size)
        return false;
    if (strlen(value) != 2)
        return false;
    uint8_t expected_value = 0;
    for (int i = 0; i < 2; i++) {
        char c = value[i];
        char digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            return false; // Invalid hex character
        expected_value = (expected_value << 4) | (uint8_t)digit;
    }
    return packet[offset] == expected_value;
}
const char *route_topic_select(const uint8_t *packet, const int packet_size, const data_type_t data_type) {
    if (topic_route_count == 0)
        return topic_route_default;
    for (size_t i = 0; i < topic_route_count; i++) {
        bool match = false;
        if (data_type == DATA_TYPE_JSON)
            match = route_topic_match_json(packet, packet_size, topic_routes[i].key, topic_routes[i].value);
        else
            match = route_topic_match_binary(packet, packet_size, topic_routes[i].key, topic_routes[i].value);
        if (match)
            return topic_routes[i].topic;
    }
    return NULL;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool capture_rssi_packet = false, capture_rssi_channel = false;
uint32_t stat_channel_rssi_cnt = 0, stat_packet_rssi_cnt = 0;
uint8_t stat_channel_rssi_ema, stat_packet_rssi_ema;
uint32_t stat_packets_okay = 0, stat_packets_drop = 0;
time_t interval_stat = 0, interval_stat_last = 0;
time_t interval_rssi = 0, interval_rssi_last = 0;
#define PACKET_BUFFER_MAX ((E22900T22_PACKET_MAXSIZE * 2) + 4) // has +1 for RSSI; adds 2 for '["' <HEX> '"]'

void read_and_send(volatile bool *running, const data_type_t data_type) {

    uint8_t packet_buffer[PACKET_BUFFER_MAX];
    int packet_size;

    printf("read-and-publish (stat=%lds, rssi=%lds [packets=%c, channel=%c], data-type=%s)\n", interval_stat, interval_rssi, capture_rssi_packet ? 'y' : 'n', capture_rssi_channel ? 'y' : 'n', data_type_tostring(data_type));

    while (*running) {

        uint8_t packet_rssi = 0, channel_rssi = 0;

        if (device_packet_read(packet_buffer, E22900T22_PACKET_MAXSIZE + 1, &packet_size, &packet_rssi) && *running) {
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
                    const int data_offset = PACKET_BUFFER_MAX - packet_size;
                    memmove(packet_buffer + data_offset, packet_buffer, (size_t)packet_size);
                    packet_buffer[0] = '[';
                    packet_buffer[1] = '"';
                    for (int i = 0; i < packet_size; i++) {
                        const uint8_t byte = packet_buffer[data_offset + i];
                        packet_buffer[2 + (i * 2)] = (uint8_t)"0123456789abcdef"[byte >> 4];
                        packet_buffer[2 + (i * 2) + 1] = (uint8_t)"0123456789abcdef"[byte & 0x0f];
                    }
                    packet_buffer[2 + (packet_size * 2)] = '"';
                    packet_buffer[2 + (packet_size * 2) + 1] = ']';
                    packet_size = json_size;
                }
                deliver = true;
                break;
            case DATA_TYPE_ANY:
            default:
                deliver = true;
                break;
            }
            if (deliver) {
                const char *topic = route_topic_select(packet_buffer, packet_size, data_type);
                if (topic) {
                    if (capture_rssi_packet)
                        ema_update(packet_rssi, &stat_packet_rssi_ema, &stat_packet_rssi_cnt);
                    if (mqtt_send(topic, (const char *)packet_buffer, packet_size))
                        stat_packets_okay++;
                    else {
                        fprintf(stderr, "read-and-publish: mqtt send failed, discarding packet (size=%d)\n", packet_size);
                        stat_packets_drop++;
                    }
                } else {
                    fprintf(stderr, "read-and-publish: no topic route match, discarding packet (size=%d)\n", packet_size);
                    stat_packets_drop++;
                }
            }
            if (debug_readandsend)
                device_packet_display(packet_buffer, packet_size, packet_rssi);
        }

        if (*running && capture_rssi_channel && intervalable(interval_rssi, &interval_rssi_last)) {
            if (device_channel_rssi_read(&channel_rssi) && *running)
                ema_update(channel_rssi, &stat_channel_rssi_ema, &stat_channel_rssi_cnt);
        }

        time_t period_stat;
        if (*running && (period_stat = intervalable(interval_stat, &interval_stat_last))) {
            const uint32_t rate_okay = (stat_packets_okay * 6000) / (uint32_t)period_stat, rate_drop = (stat_packets_drop * 6000) / (uint32_t)period_stat;
            printf("packets-okay=%" PRIu32 " (%" PRIu32 ".%02" PRIu32 "/min), packets-drop=%" PRIu32 " (%" PRIu32 ".%02" PRIu32 "/min)", stat_packets_okay, rate_okay / 100, rate_okay % 100, stat_packets_drop, rate_drop / 100,
                   rate_drop % 100);
            stat_packets_okay = stat_packets_drop = 0;
            if (capture_rssi_channel)
                printf(", channel-rssi=%d dBm (%" PRIu32 ")", get_rssi_dbm(stat_channel_rssi_ema), stat_channel_rssi_cnt);
            if (capture_rssi_packet)
                printf(", packet-rssi=%d dbm (%" PRIu32 ")", get_rssi_dbm(stat_packet_rssi_ema), stat_packet_rssi_cnt);
            printf("\n");
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

serial_config_t serial_config;
e22900t22_config_t e22900t22u_config;
const char *mqtt_client, *mqtt_server;
data_type_t data_type;

bool config_setup(int argc, char *argv[]) {

    if (!config_load(CONFIG_FILE_DEFAULT, argc, argv, config_options))
        return false;

    config_populate_serial(&serial_config);
    config_populate_e22900t22u(&e22900t22u_config);
    config_populate_topic_routes(MQTT_TOPIC_DEFAULT);
    mqtt_client = config_get_string("mqtt-client", MQTT_CLIENT_DEFAULT);
    mqtt_server = config_get_string("mqtt-server", MQTT_SERVER_DEFAULT);

    capture_rssi_packet = config_get_bool("rssi-packet", E22900T22_CONFIG_RSSI_PACKET_DEFAULT);
    capture_rssi_channel = config_get_bool("rssi-channel", E22900T22_CONFIG_RSSI_CHANNEL_DEFAULT);
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

int main(int argc, char *argv[]) {

    setbuf(stdout, NULL);
    printf("starting\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!config_setup(argc, argv))
        return EXIT_FAILURE;

    if (!serial_begin(&serial_config) || !serial_connect()) {
        fprintf(stderr, "device: failed to connect (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate, serial_bits_str(serial_config.bits));
        return EXIT_FAILURE;
    }
    if (!device_connect(E22900T22_MODULE_USB, &e22900t22u_config)) {
        serial_end();
        return EXIT_FAILURE;
    }
    printf("device: connected (port=%s, rate=%d, bits=%s)\n", serial_config.port, serial_config.rate, serial_bits_str(serial_config.bits));
    if (!(device_mode_config() && device_info_read() && device_config_read_and_update() && device_mode_transfer())) {
        device_disconnect();
        serial_end();
        return EXIT_FAILURE;
    }

    if (!mqtt_begin(mqtt_server, mqtt_client, false)) {
        device_disconnect();
        serial_end();
        return EXIT_FAILURE;
    }

    read_and_send(&running, data_type);

    device_disconnect();
    serial_end();
    mqtt_end();

    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
