
// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <ArduinoJson.h>

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

typedef unsigned long interval_t;
typedef unsigned long counter_t;

class Intervalable {
    interval_t _interval, _previous;
    counter_t _exceeded = 0;

  public:
    explicit Intervalable(const interval_t interval = 0, const interval_t previous = 0)
        : _interval(interval), _previous(previous) {}
    operator bool() {
        const interval_t current = millis();
        if (current - _previous > _interval) {
            _previous = current;
            return true;
        }
        return false;
    }
    bool active() const { return _interval > 0; }
    interval_t remaining() const {
        const interval_t current = millis();
        return _interval - (current - _previous);
    }
    bool passed(interval_t *interval = nullptr, const bool atstart = false) {
        const interval_t current = millis();
        if ((atstart && _previous == 0) || current - _previous > _interval) {
            if (interval != nullptr)
                (*interval) = current - _previous;
            _previous = current;
            return true;
        }
        return false;
    }
    void reset(const interval_t interval = std::numeric_limits<interval_t>::max()) {
        if (interval != std::numeric_limits<interval_t>::max())
            _interval = interval;
        _previous = millis();
    }
    void setat(const interval_t place) { _previous = millis() - ((_interval - place) % _interval); }
    void wait() {
        const interval_t current = millis();
        if (current - _previous < _interval)
            delay(_interval - (current - _previous));
        else if (_previous > 0)
            _exceeded++;
        _previous = millis();
    }
    counter_t exceeded() const { return _exceeded; }
};

#include <esp_mac.h>

template <size_t N> String BytesToHexString(const uint8_t bytes[], const char *separator = ":") {
    constexpr size_t separator_max = 1; // change if needed
    if (strlen(separator) > separator_max)
        return String("");
    char buffer[(N * 2) + ((N - 1) * separator_max) + 1] = {'\0'}, *buffer_ptr = buffer;
    for (size_t i = 0; i < N; i++) {
        if (i > 0 && separator[0] != '\0')
            for (const char *separator_ptr = separator; *separator_ptr != '\0';)
                *buffer_ptr++ = *separator_ptr++;
        static const char hex_chars[] = "0123456789abcdef";
        *buffer_ptr++ = hex_chars[(bytes[i] >> 4) & 0x0F];
        *buffer_ptr++ = hex_chars[bytes[i] & 0x0F];
    }
    *buffer_ptr = '\0';
    return String(buffer);
}

String getMacAddressBase(const char *separator = ":") {
    uint8_t macaddr[6];
    esp_read_mac(macaddr, ESP_MAC_BASE);
    return BytesToHexString<6>(macaddr, separator);
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

static inline constexpr gpio_num_t PIN_E22900T22D_M0 = GPIO_NUM_5;
static inline constexpr gpio_num_t PIN_E22900T22D_M1 = GPIO_NUM_6;
static inline constexpr gpio_num_t PIN_E22900T22D_RXD = GPIO_NUM_7;
static inline constexpr gpio_num_t PIN_E22900T22D_TXD = GPIO_NUM_20;
static inline constexpr gpio_num_t PIN_E22900T22D_AUX = GPIO_NUM_21;

#define PRINTF_DEBUG Serial.printf
#define PRINTF_INFO Serial.printf
#define PRINTF_ERROR Serial.printf

void __sleep_ms(const unsigned long ms) { delay(ms); }

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#include <HardwareSerial.h>

const int serialId = 1;
HardwareSerial serial_hw(serialId);

bool serial_connect(void) {
    serial_hw.setRxBufferSize(512);
    serial_hw.setTxBufferSize(512);
    serial_hw.setTimeout(500); // yuck, should be related to values in driver
    serial_hw.begin(9600, SERIAL_8N1, PIN_E22900T22D_TXD, PIN_E22900T22D_RXD, false);
    return true;
}

void serial_disconnect(void) { serial_hw.end(); }

void serial_flush(void) {
    while (serial_hw.available())
        serial_hw.read();
}

int serial_write(const unsigned char *buffer, const int length) {
    __sleep_ms(50); // yuck
    return serial_hw.write(buffer, length);
}

int serial_read(unsigned char *buffer, const int length, const int timeout_ms) {
    // yuck, ignoring timeout_ms
    return serial_hw.readBytes(buffer, length);
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

#define E22900T22_SUPPORT_MODULE_DIP
#undef E22900T22_SUPPORT_MODULE_USB
#include "../../include/e22xxxtxx.h"

void e22900t22d_cfg_pin() {
    pinMode(PIN_E22900T22D_M0, OUTPUT);
    digitalWrite(PIN_E22900T22D_M0, HIGH);
    pinMode(PIN_E22900T22D_M1, OUTPUT);
    digitalWrite(PIN_E22900T22D_M1, HIGH);
    pinMode(PIN_E22900T22D_RXD, OUTPUT);
    pinMode(PIN_E22900T22D_TXD, INPUT);
    pinMode(PIN_E22900T22D_AUX, INPUT_PULLUP);
}
void e22900t22d_set_pin_mx(const bool pin_m0, const bool pin_m1) {
    digitalWrite(PIN_E22900T22D_M0, pin_m0 ? HIGH : LOW);
    digitalWrite(PIN_E22900T22D_M1, pin_m1 ? HIGH : LOW);
}
bool e22900t22d_get_pin_aux(void) { return digitalRead(PIN_E22900T22D_AUX) == HIGH ? true : false; }

e22900t22_config_t e22900t22u_config = {
    .address = 0x0008,
    .network = 0x00,
    .channel = 0x17, // Channel 23 (850.125 + 23 = 873.125 MHz)
    .packet_maxsize = CONFIG_PACKET_MAXSIZE_DEFAULT,
    .listen_before_transmit = true,
    .rssi_packet = true,
    .rssi_channel = true,
    .read_timeout_command = CONFIG_READ_TIMEOUT_COMMAND_DEFAULT,
    .read_timeout_packet = CONFIG_READ_TIMEOUT_PACKET_DEFAULT,
    .set_pin_mx = e22900t22d_set_pin_mx,
    .get_pin_aux = e22900t22d_get_pin_aux,
    .debug = false,
};

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------

void halt() {
    while (1)
        delay(100);
}

void setup() {
    Serial.begin(115200);
    delay(5 * 1000);
    Serial.println("UP");

    e22900t22d_cfg_pin();
    serial_connect();

    if (!device_connect(E22900T22_MODULE_DIP, &e22900t22u_config)) {
        PRINTF_ERROR("setup: device_connect failed\n");
        halt();
    }
    if (!(device_mode_config() && device_info_read() && device_config_read_and_update() && device_mode_transfer())) {
        PRINTF_ERROR("setup: device_mode/info/config failed\n");
        halt();
    }
}

// -----------------------------------------------------------------------------------------------

Intervalable secs(5 * 1000);
Intervalable ping(30 * 1000);

void loop() {
    secs.wait();

    unsigned char rssi;
    if (!device_channel_rssi_read(&rssi))
        PRINTF_ERROR("loop: device_channel_rssi_read failed\n");
    else
        device_channel_rssi_display(rssi);

    if (ping) {
        static int counts = 1;
        JsonDocument jsonDoc;
        String jsonStr;
        jsonDoc["ping"]["source"] = getMacAddressBase();
        jsonDoc["ping"]["millis"] = millis();
        jsonDoc["ping"]["counts"] = counts++;
        serializeJson(jsonDoc, jsonStr);
        PRINTF_INFO("loop: device_packet_write <<<%s>>>\n", jsonStr.c_str());
        if (!device_packet_write((const unsigned char *)jsonStr.c_str(), jsonStr.length()))
            PRINTF_ERROR("loop: device_packet_write failed\n");
    }
}

// -----------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------
