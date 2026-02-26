
// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <arpa/inet.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

time_t intervalable(const time_t interval, time_t *last) {
    time_t now = time(NULL);
    if (*last == 0) {
        *last = now;
        return 0;
    }
    if ((now - *last) > interval) {
        const time_t diff = now - *last;
        *last = now;
        return diff;
    }
    return 0;
}

bool is_reasonable_json(const uint8_t *packet, const int length) {
    if (length < 2)
        return false;
    if (!(packet[0] == '{' || packet[0] == '[') || !(packet[length - 1] == '}' || packet[length - 1] == ']'))
        return false;
    for (int index = 0; index < length; index++)
        if (!isprint(packet[index]))
            return false;
    return true;
}

// 0.2 ≈ 51/256, 0.8 ≈ 205/256
#define EMA_ALPHA_NUM   51
#define EMA_ALPHA_DENOM 256
void ema_update(uint8_t value, uint8_t *value_ema, uint32_t *value_cnt) {
    if ((*value_cnt)++ == 0)
        *value_ema = value;
    else
        *value_ema = (uint8_t)((EMA_ALPHA_NUM * (uint16_t)value + (EMA_ALPHA_DENOM - EMA_ALPHA_NUM) * (uint16_t)(*value_ema)) / EMA_ALPHA_DENOM);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
