
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

#include <mosquitto.h>

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

bool debug = false;

void printf_debug(const char *format, ...) {
    if (debug) {
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

#include "serial.h"
#undef E22900T22_SUPPORT_MODULE_DIP
#define E22900T22_SUPPORT_MODULE_USB
#include "src/e22900t22.h"
void __sleep_ms(const unsigned long ms) { usleep(ms * 1000); }

#define SERIAL_PORT_DEFAULT "/dev/e22900t22u"
#define SERIAL_RATE_DEFAULT 9600
#define SERIAL_BITS_DEFAULT SERIAL_8N1

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define CONFIG_FILE_DEFAULT "e22900t22utomqtt.cfg"

#define MQTT_SERVER_DEFAULT "mqtt://localhost"
#define MQTT_TOPIC_DEFAULT "e22900t22u"

#define MQTT_CONNECT_TIMEOUT 60
#define MQTT_PUBLISH_QOS 0
#define MQTT_PUBLISH_RETAIN false

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <ctype.h>
#include <getopt.h>

#define CONFIG_MAX_STRING 255

typedef struct {
    char *key;
    char *value;
} config_entry_t;

#define CONFIG_MAX_ENTRIES 32
config_entry_t config_entries[CONFIG_MAX_ENTRIES];
int config_entry_count = 0;

void __config_set_value(const char *key, const char *value) {
    for (int i = 0; i < config_entry_count; i++)
        if (strcmp(config_entries[i].key, key) == 0) {
            free(config_entries[i].value);
            config_entries[i].value = strdup(value);
            return;
        }
    if (config_entry_count < CONFIG_MAX_ENTRIES) {
        config_entries[config_entry_count].key = strdup(key);
        config_entries[config_entry_count].value = strdup(value);
        config_entry_count++;
    } else
        fprintf(stderr, "config: too many entries, ignoring %s=%s\n", key, value);
}

const char *config_get_string(const char *key, const char *default_value) {
    for (int i = 0; i < config_entry_count; i++)
        if (strcmp(config_entries[i].key, key) == 0)
            return config_entries[i].value;
    return default_value;
}

int config_get_integer(const char *key, const int default_value) {
    for (int i = 0; i < config_entry_count; i++)
        if (strcmp(config_entries[i].key, key) == 0) {
            char *endptr;
            const long val = strtol(config_entries[i].value, &endptr, 0);
            if (*endptr == '\0')
                return (int)val;
            else {
                fprintf(stderr, "config: invalid integer value '%s' for key '%s', using default\n",
                        config_entries[i].value, key);
                return default_value;
            }
        }
    return default_value;
}

bool config_get_bool(const char *key, const bool default_value) {
    for (int i = 0; i < config_entry_count; i++)
        if (strcmp(config_entries[i].key, key) == 0) {
            if (strcasecmp(config_entries[i].value, "true") == 0 || strcmp(config_entries[i].value, "1") == 0)
                return true;
            else if (strcasecmp(config_entries[i].value, "false") == 0 || strcmp(config_entries[i].value, "0") == 0)
                return false;
            fprintf(stderr, "config: invalid boolean value '%s' for key '%s', using default\n", config_entries[i].value,
                    key);
        }
    return default_value;
}

serial_bits_t config_get_bits(const char *key, const serial_bits_t default_value) {
    for (int i = 0; i < config_entry_count; i++)
        if (strcmp(config_entries[i].key, key) == 0) {
            if (strcmp(config_entries[i].value, "8N1") == 0)
                return SERIAL_8N1;
            fprintf(stderr, "config: invalid bits value '%s', using default\n", config_entries[i].value);
        }
    return default_value;
}

void __config_load_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "config: could not load '%s'\n", filename);
        return;
    }
    char line[CONFIG_MAX_STRING];
    while (fgets(line, sizeof(line), file)) {
        char *equals = strchr(line, '=');
        if (equals) {
            *equals = '\0';
            char *key = line;
            char *value = equals + 1;
            while (*key && isspace(*key))
                key++;
            char *end = key + strlen(key) - 1;
            while (end > key && isspace(*end))
                *end-- = '\0';
            while (*value && isspace(*value))
                value++;
            end = value + strlen(value) - 1;
            while (end > value && isspace(*end))
                *end-- = '\0';
            __config_set_value(key, value);
        }
    }
    fclose(file);
}

// clang-format off
const struct option options_long [] = {
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
    {"debug",                 required_argument, 0, 0},
    {0, 0, 0, 0}
};
// clang-format on

bool config_load(const char *config_file, const int argc, const char *argv[]) {
    int c;
    int option_index = 0;
    optind = 0;
    while ((c = getopt_long(argc, (char **)argv, "", options_long, &option_index)) != -1) {
        if (c == 0)
            if (strcmp(options_long[option_index].name, "config") == 0) {
                config_file = optarg;
                break;
            }
    }
    __config_load_file(config_file);
    optind = 0;
    while ((c = getopt_long(argc, (char **)argv, "", options_long, &option_index)) != -1) {
        if (c == 0)
            if (strcmp(options_long[option_index].name, "config") != 0)
                __config_set_value(options_long[option_index].name, optarg);
    }
    printf("config: loaded from '%s' and command line\n", config_file);
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

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

struct mosquitto *mosq = NULL;

void mqtt_send(const char *topic, const char *message, const int length) {
    if (!mosq)
        return;
    const int result = mosquitto_publish(mosq, NULL, topic, length, message, MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN);
    if (result != MOSQ_ERR_SUCCESS)
        fprintf(stderr, "mqtt: publish error: %s\n", mosquitto_strerror(result));
}

bool mqtt_parse(const char *string, char *host, const int length, int *port, bool *ssl) {
    host[0] = '\0';
    *port = 1883;
    *ssl = false;
    if (strncmp(string, "mqtt://", 7) == 0) {
        strncpy(host, string + 7, length - 1);
    } else if (strncmp(string, "mqtts://", 8) == 0) {
        strncpy(host, string + 8, length - 1);
        *ssl = true;
        *port = 8883;
    } else {
        strcpy(host, string);
    }
    char *port_str = strchr(host, ':');
    if (port_str) {
        *port_str = '\0'; // Terminate host string at colon
        *port = atoi(port_str + 1);
    }
    return true;
}

void mqtt_connect_callback(struct mosquitto *m, void *o __attribute__((unused)), int r) {
    if (m != mosq)
        return;
    if (r != 0) {
        fprintf(stderr, "mqtt: connect failed: %s\n", mosquitto_connack_string(r));
        return;
    }
    printf("mqtt: connected\n");
}

bool mqtt_begin(const char *server) {
    char host[CONFIG_MAX_STRING];
    int port;
    bool ssl;
    if (!mqtt_parse(server, host, sizeof(host), &port, &ssl)) {
        fprintf(stderr, "mqtt: error parsing details in '%s'\n", server);
        return false;
    }
    printf("mqtt: connecting (host='%s', port=%d, ssl=%s)\n", host, port, ssl ? "true" : "false");
    char client_id[24];
    sprintf(client_id, "sensor-radiation-%06X", rand() & 0xFFFFFF);
    int result;
    mosquitto_lib_init();
    mosq = mosquitto_new(client_id, true, NULL);
    if (!mosq) {
        fprintf(stderr, "mqtt: error creating client instance\n");
        return false;
    }
    if (ssl)
        mosquitto_tls_insecure_set(mosq, true); // Skip certificate validation
    mosquitto_connect_callback_set(mosq, mqtt_connect_callback);
    if ((result = mosquitto_connect(mosq, host, port, MQTT_CONNECT_TIMEOUT)) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "mqtt: error connecting to broker: %s\n", mosquitto_strerror(result));
        mosquitto_destroy(mosq);
        mosq = NULL;
        return false;
    }
    if ((result = mosquitto_loop_start(mosq)) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "mqtt: error starting loop: %s\n", mosquitto_strerror(result));
        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
        mosq = NULL;
        return false;
    }
    return true;
}

void mqtt_end(void) {
    if (mosq) {
        mosquitto_loop_stop(mosq, true);
        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
        mosq = NULL;
    }
    mosquitto_lib_cleanup();
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

volatile bool is_active = true;

void signal_handler(int sig __attribute__((unused))) {
    if (is_active) {
        printf("stopping\n");
        is_active = false;
    }
}

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

void read_and_send(const char *mqtt_topic) {
    const int max_packet_size = E22900T22_PACKET_MAXSIZE + 1; // RSSI
    unsigned char packet_buffer[max_packet_size];
    int packet_size;
    unsigned char rssi;

    printf("read-and-publish (topic='%s')\n", mqtt_topic);

    while (is_active) {
        if (device_packet_read(packet_buffer, config.packet_maxsize + 1, &packet_size, &rssi) && is_active) {
            if (!packet_is_reasonable_json(packet_buffer, packet_size))
                fprintf(stderr, "read-and-publish: discarding malformed packet (size=%d)\n", packet_size);
            else
                mqtt_send(mqtt_topic, (const char *)packet_buffer, packet_size);
            device_packet_display(packet_buffer, packet_size, rssi);
        } else if (is_active) {
            if (device_channel_rssi_read(&rssi) && is_active)
                device_channel_rssi_display(rssi);
        }
    }
}

int main(int argc, const char *argv[]) {

    printf("starting\n");

    setbuf(stdout, NULL);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!config_load(CONFIG_FILE_DEFAULT, argc, argv))
        return EXIT_FAILURE;
    const char *mqtt_server = config_get_string("mqtt-server", MQTT_SERVER_DEFAULT);
    const char *mqtt_topic = config_get_string("mqtt-topic", MQTT_TOPIC_DEFAULT);
    serial_config_t serial_config;
    e22900t22_config_t e22900t22u_config;
    config_populate_serial(&serial_config);
    config_populate_e22900t22u(&e22900t22u_config);

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
