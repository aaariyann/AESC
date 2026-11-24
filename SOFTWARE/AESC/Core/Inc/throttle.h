#ifndef THROTTLE_H
#define THROTTLE_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32g4xx_hal.h"

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t min_us;
    uint16_t max_us;
    uint16_t neutral_us;
    uint16_t reserved;
    uint32_t crc;
} throttle_flash_t;

_Static_assert(sizeof(throttle_flash_t) == 16U,
               "throttle_flash_t must fit two flash doublewords");

void throttle_init(void);
void throttle_task(TIM_HandleTypeDef *htim);
uint16_t throttle_get_pulse_us(void);
uint32_t throttle_get_period_us(void);
bool throttle_is_valid(void);
bool throttle_is_activity(void);
bool throttle_calibration_active(void);
void throttle_calibration_start(void);
void throttle_calibration_task(void);
bool throttle_calibration_complete(void);
uint16_t throttle_get_neutral_us(void);
void throttle_calibration_abort(void);

#endif
