
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

serial_config_t serial_config = {
    .port = "/dev/e22900t22u",
    .rate = 9600,
    .bits = SERIAL_8N1,
};

e22900t22_config_t e22900t22u_config = {
    .address = 0x0008,
    .network = 0x00,
    .channel = 0x17, // Channel 23 (850.125 + 23 = 873.125 MHz)
    .packet_maxsize = CONFIG_PACKET_MAXSIZE_DEFAULT,
    .listen_before_transmit = false,
    .rssi_packet = true,
    .rssi_channel = true,
    .read_timeout_command = CONFIG_READ_TIMEOUT_COMMAND_DEFAULT,
    .read_timeout_packet = CONFIG_READ_TIMEOUT_PACKET_DEFAULT,
    .debug = false,
};

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

#define CONFIG_MAX_LINE 256
#define CONFIG_MAX_VALUE 128

const char *config_file = CONFIG_FILE_DEFAULT;
char config_mqtt_server[CONFIG_MAX_VALUE] = MQTT_SERVER_DEFAULT;
char config_mqtt_topic[CONFIG_MAX_VALUE] = MQTT_TOPIC_DEFAULT;

bool config_load(int argc, const char **argv) {
    if (argc > 1)
        config_file = argv[1];
    FILE *file = fopen(config_file, "r");
    if (file == NULL) {
        fprintf(stderr, "config: could not load '%s', using defaults (which may not work correctly)\n", config_file);
        return false;
    }
    char line[CONFIG_MAX_LINE];
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
            if (strcmp(key, "MQTT_SERVER") == 0)
                strncpy(config_mqtt_server, value, sizeof(config_mqtt_server) - 1);
            else if (strcmp(key, "MQTT_TOPIC") == 0)
                strncpy(config_mqtt_topic, value, sizeof(config_mqtt_topic) - 1);
        }
    }
    fclose(file);
    printf("config: '%s': mqtt=%s, topic=%s\n", config_file, config_mqtt_server, config_mqtt_topic);
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

struct mosquitto *mosq = NULL;

void mqtt_send(const char *topic, const char *message, const int length) {
    if (!mosq)
        return;
    int result = mosquitto_publish(mosq, NULL, topic, length, message, MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN);
    if (result != MOSQ_ERR_SUCCESS)
        fprintf(stderr, "mqtt: publish error: %s\n", mosquitto_strerror(result));
}

bool mqtt_parse(const char *string, char *host, int length, int *port, bool *ssl) {
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

bool mqtt_begin(void) {
    char host[CONFIG_MAX_VALUE];
    int port;
    bool ssl;
    if (!mqtt_parse(config_mqtt_server, host, sizeof(host), &port, &ssl)) {
        fprintf(stderr, "mqtt: error parsing details in '%s'\n", config_mqtt_server);
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

void read_and_send() {
    static const int max_packet_size = E22900T22_PACKET_MAXSIZE + 1; // RSSI
    unsigned char packet_buffer[max_packet_size];
    int packet_size;
    unsigned char rssi;

    while (is_active) {
        if (device_packet_read(packet_buffer, config.packet_maxsize + 1, &packet_size, &rssi) && is_active) {
            if (!packet_is_reasonable_json(packet_buffer, packet_size))
                fprintf(stderr, "read_and_send: discarding malformed packet\n");
            else
                mqtt_send(config_mqtt_topic, (const char *)packet_buffer, packet_size);
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

    if (!config_load(argc, argv))
        return EXIT_FAILURE;

    if (!mqtt_begin())
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

    read_and_send();

    device_disconnect();
    serial_disconnect();
    mqtt_end();

    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
