#include "throttle.h"
#include "esc_config.h"
#include "esc_types.h"
#include "fault.h"
#include "beep.h"

#if ESC_FEATURE_THROTTLE

#include <stddef.h>
#include <string.h>

typedef enum {
    THROTTLE_CAL_IDLE = 0,
    THROTTLE_CAL_MAX,
    THROTTLE_CAL_MIN,
    THROTTLE_CAL_NEUTRAL
} throttle_cal_state_t;

static uint16_t s_pulse_us;
static uint32_t s_period_us;
static bool s_valid;
static throttle_flash_t s_flash;
static throttle_cal_state_t s_cal_state;
static bool s_cal_complete;
static uint32_t s_cal_stage_start;
static uint16_t s_cal_max;
static uint16_t s_cal_min;
static uint32_t s_cal_started_tick;
_Static_assert(sizeof(throttle_flash_t) == 16U,
               "throttle_flash_t must fit two flash doublewords");

static uint32_t throttle_crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0U; i < length; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1U) ^ ESC_THROTTLE_FLASH_CRC_POLY;
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

static bool throttle_flash_valid(const throttle_flash_t *flash)
{
    if (flash->magic != ESC_THROTTLE_FLASH_MAGIC) {
        return false;
    }
    if ((flash->min_us < ESC_THROTTLE_PULSE_MIN_US) ||
        (flash->max_us > ESC_THROTTLE_PULSE_MAX_US) ||
        (flash->min_us >= flash->max_us) ||
        (flash->neutral_us < flash->min_us) ||
        (flash->neutral_us > flash->max_us)) {
        return false;
    }
    return throttle_crc32((const uint8_t *)flash,
                          (uint32_t)offsetof(throttle_flash_t, crc)) == flash->crc;
}

static void throttle_flash_load(void)
{
    const throttle_flash_t *stored =
        (const throttle_flash_t *)ESC_THROTTLE_FLASH_ADDRESS;
    if (throttle_flash_valid(stored)) {
        s_flash = *stored;
        return;
    }
    s_flash.magic = ESC_THROTTLE_FLASH_MAGIC;
    s_flash.min_us = ESC_THROTTLE_DEFAULT_MIN_US;
    s_flash.max_us = ESC_THROTTLE_DEFAULT_MAX_US;
    s_flash.neutral_us = ESC_THROTTLE_DEFAULT_NEUTRAL_US;
    s_flash.reserved = 0U;
    s_flash.crc = throttle_crc32((const uint8_t *)&s_flash,
                                 (uint32_t)offsetof(throttle_flash_t, crc));
}

static bool throttle_flash_save(const throttle_flash_t *flash)
{
    uint64_t words[2];
    memcpy(words, flash, sizeof(*flash));

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }
    fault_iwdg_refresh();

    FLASH_EraseInitTypeDef erase;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.Page = ESC_THROTTLE_FLASH_PAGE;
    erase.NbPages = 1U;
    uint32_t page_error = 0U;
    bool ok = (HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK);
    fault_iwdg_refresh();

    if (ok) {
        fault_iwdg_refresh();
        ok = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                ESC_THROTTLE_FLASH_ADDRESS,
                                words[0]) == HAL_OK);
    }
    if (ok) {
        ok = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                ESC_THROTTLE_FLASH_ADDRESS + 8U,
                                words[1]) == HAL_OK);
    }
    (void)HAL_FLASH_Lock();
    return ok;
}

void throttle_init(void)
{
    s_pulse_us = 0U;
    s_period_us = 0U;
    s_cal_started_tick = 0U;
    s_valid = false;
    s_cal_state = THROTTLE_CAL_IDLE;
    s_cal_complete = false;
    s_cal_stage_start = 0U;
    s_cal_max = ESC_THROTTLE_DEFAULT_MAX_US;
    s_cal_min = ESC_THROTTLE_DEFAULT_MIN_US;
    throttle_flash_load();
}

void throttle_task(TIM_HandleTypeDef *htim)
{
    (void)htim;
    if (htim == NULL) {
        s_valid = false;
        return;
    }

    uint32_t elapsed_us = TIM2->CNT;
    uint32_t period = TIM2->CCR1;
    uint32_t pulse = TIM2->CCR2;

    bool valid = (elapsed_us < (ESC_THROTTLE_TIMEOUT_MS * 1000UL)) &&
                 (period >= ESC_THROTTLE_PERIOD_MIN_US) &&
                 (period <= ESC_THROTTLE_PERIOD_MAX_US) &&
                 (pulse >= ESC_THROTTLE_PULSE_MIN_US) &&
                 (pulse <= ESC_THROTTLE_PULSE_MAX_US);
    if (valid) {
        s_period_us = period;
        s_pulse_us = (uint16_t)pulse;
        s_valid = true;
    } else {
        s_valid = false;
        s_pulse_us = 0U;
        s_period_us = 0U;
    }

    esc_debug.throttle_us = s_valid ? s_pulse_us : 0U;
}

uint16_t throttle_get_pulse_us(void)
{
    return s_pulse_us;
}

uint32_t throttle_get_period_us(void)
{
    return s_period_us;
}

bool throttle_is_valid(void)
{
    return s_valid;
}

bool throttle_is_activity(void)
{
    if (!s_valid) {
        return false;
    }
    uint16_t neutral = s_flash.neutral_us;
    return (s_pulse_us < (neutral - ESC_THROTTLE_NEUTRAL_DEADBAND_US)) ||
           (s_pulse_us > (neutral + ESC_THROTTLE_NEUTRAL_DEADBAND_US));
}

bool throttle_calibration_active(void)
{
    return s_cal_state != THROTTLE_CAL_IDLE;
}

void throttle_calibration_start(void)
{
    s_cal_state = THROTTLE_CAL_MAX;
    s_cal_stage_start = HAL_GetTick();
    s_cal_started_tick = s_cal_stage_start;
    s_cal_max = ESC_THROTTLE_PULSE_MIN_US;
    s_cal_min = ESC_THROTTLE_PULSE_MAX_US;
    s_cal_complete = false;
}

void throttle_calibration_task(void)
{
    if (s_cal_state == THROTTLE_CAL_IDLE) {
        return;
    }
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - s_cal_started_tick) >= ESC_THROTTLE_CAL_TIMEOUT_MS) {
        throttle_calibration_abort();
        return;
    }
    if (!s_valid) {
        s_cal_stage_start = HAL_GetTick();
        return;
    }
    if (s_cal_state == THROTTLE_CAL_MAX) {
        if (s_pulse_us > s_cal_max) {
            s_cal_max = s_pulse_us;
        }
        if (s_pulse_us <= ESC_THROTTLE_CAL_MIN_THRESHOLD_US) {
            if ((uint32_t)(now - s_cal_stage_start) >= ESC_THROTTLE_CAL_STAGE_MS) {
                s_cal_state = THROTTLE_CAL_MIN;
                s_cal_stage_start = now;
                s_flash.max_us = s_cal_max;
                beep_request(ESC_BEEP_CAL_STEP);
            }
        } else {
            s_cal_stage_start = now;
        }
    } else if (s_cal_state == THROTTLE_CAL_MIN) {
        if (s_pulse_us < s_cal_min) {
            s_cal_min = s_pulse_us;
        }
        if (s_pulse_us >= ESC_THROTTLE_CAL_NEUTRAL_MIN_US &&
            s_pulse_us <= ESC_THROTTLE_CAL_NEUTRAL_MAX_US) {
            if ((uint32_t)(now - s_cal_stage_start) >= ESC_THROTTLE_CAL_STAGE_MS) {
                s_cal_state = THROTTLE_CAL_NEUTRAL;
                s_cal_stage_start = now;
                s_flash.min_us = s_cal_min;
                beep_request(ESC_BEEP_CAL_STEP);
            }
        } else {
            s_cal_stage_start = now;
        }
    } else {
        if (s_pulse_us >= ESC_THROTTLE_CAL_NEUTRAL_MIN_US &&
            s_pulse_us <= ESC_THROTTLE_CAL_NEUTRAL_MAX_US) {
            if ((uint32_t)(now - s_cal_stage_start) >= ESC_THROTTLE_CAL_STAGE_MS) {
                s_flash.neutral_us = s_pulse_us;
                s_flash.crc = throttle_crc32((const uint8_t *)&s_flash,
                                             (uint32_t)offsetof(throttle_flash_t, crc));
                if (throttle_flash_save(&s_flash)) {
                    s_cal_complete = true;
                } else {
                    fault_latch(ESC_FAULT_DETECTION);
                }
                s_cal_state = THROTTLE_CAL_IDLE;
            }
        } else {
            s_cal_stage_start = now;
        }
    }
}

bool throttle_calibration_complete(void)
{
    return s_cal_complete;
}

uint16_t throttle_get_neutral_us(void)
{
    return s_flash.neutral_us;
}

void throttle_calibration_abort(void)
{
    s_cal_state = THROTTLE_CAL_IDLE;
    s_cal_complete = false;
    s_cal_started_tick = 0U;
}
#endif
