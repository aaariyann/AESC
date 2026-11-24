#include "beep.h"
#include "esc_config.h"
#include "esc_types.h"
#include "fault.h"
#include "hw_pwm.h"

#if ESC_FEATURE_BEEP

typedef enum {
    BEEP_STATE_IDLE = 0,
    BEEP_STATE_TONE,
    BEEP_STATE_GAP
} beep_state_t;

static volatile esc_beep_event_t s_event;
static volatile bool s_active;
static volatile bool s_phase;
static volatile beep_state_t s_state;
static volatile uint32_t s_tone_index;
static volatile uint32_t s_tone_count;
static volatile uint32_t s_tone_ticks;
static volatile uint32_t s_toggle_ticks;
static volatile uint32_t s_gap_ticks;
static volatile bool s_outputs_enabled;

static uint32_t beep_half_ticks(uint32_t frequency_hz)
{
    if (frequency_hz == 0U) {
        return 1U;
    }
    return (1000000UL / (frequency_hz * 100UL));
}

static uint32_t beep_tone_frequency(void)
{
    if (s_event == ESC_BEEP_STARTUP) {
        static const uint32_t startup_table[] = {1000U, 1300U, 1600U};
        uint32_t index = s_tone_index;
        uint32_t table_size = (uint32_t)(sizeof(startup_table) / sizeof(startup_table[0]));
        if (index >= table_size) {
            index = table_size - 1U;
        }
        return startup_table[index];
    }
    if (s_event == ESC_BEEP_ARMING) {
        return 1500U;
    }
    if (s_event == ESC_BEEP_CAL_ENTER) {
        return 1200U;
    }
    if (s_event == ESC_BEEP_CAL_STEP) {
        return 1800U;
    }
    if (s_event == ESC_BEEP_CAL_DONE) {
        return 2000U;
    }
    return 500U;
}

static uint32_t beep_tone_target(void)
{
    if (s_event == ESC_BEEP_STARTUP) {
        return 3U;
    }
    if (s_event == ESC_BEEP_CAL_DONE) {
        return 2U;
    }
    return 1U;
}

void beep_init(void)
{
    s_event = ESC_BEEP_NONE;
    s_active = false;
    s_state = BEEP_STATE_IDLE;
    s_tone_index = 0U;
    s_tone_ticks = 0U;
    s_toggle_ticks = 0U;
    s_gap_ticks = 0U;
    s_outputs_enabled = false;
    s_phase = false;
    s_tone_count = 0U;
}

void beep_request(esc_beep_event_t event)
{
    if (event == ESC_BEEP_NONE) {
        return;
    }
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_event = event;
    s_active = true;
    s_phase = false;
    s_tone_count = 0U;
    s_tone_index = 0U;
    s_state = BEEP_STATE_IDLE;
    s_outputs_enabled = false;
    if (primask == 0U) {
        __enable_irq();
    }
}

bool beep_is_active(void)
{
    return s_active;
}

void beep_task(bool throttle_activity)
{
    if (!s_active) {
        return;
    }
    if (throttle_activity ||
        (esc_debug.ia_a > ESC_BEEP_CURRENT_LIMIT_A) ||
        (esc_debug.ia_a < -ESC_BEEP_CURRENT_LIMIT_A) ||
        (esc_debug.ib_a > ESC_BEEP_CURRENT_LIMIT_A) ||
        (esc_debug.ib_a < -ESC_BEEP_CURRENT_LIMIT_A)) {
        hw_pwm_force_off();
        s_active = false;
        s_event = ESC_BEEP_NONE;
        return;
    }
    if (fault_get() != ESC_FAULT_NONE &&
        s_event != ESC_BEEP_FAULT) {
        hw_pwm_force_off();
        s_active = false;
        s_event = ESC_BEEP_NONE;
        return;
    }
    if (s_state == BEEP_STATE_IDLE) {
        if (hw_pwm_enable_outputs() != HAL_OK) {
            hw_pwm_force_off();
            s_active = false;
            s_event = ESC_BEEP_NONE;
            return;
        }
        s_outputs_enabled = true;
        s_state = BEEP_STATE_TONE;
        s_tone_ticks = 0U;
        s_toggle_ticks = beep_half_ticks(beep_tone_frequency());
        s_phase = false;
    }
}

void beep_tick(void)
{
    if (!s_active) {
        return;
    }

    if (s_state == BEEP_STATE_TONE) {
        if (s_tone_ticks == 0U) {
            s_phase = false;
        }
        if (s_toggle_ticks == 0U) {
            s_phase = !s_phase;
            uint32_t duty = (uint32_t)(ESC_BEEP_DUTY * (float)ESC_PWM_ARR);
            TIM1->CCR1 = s_phase ? duty : 0U;
            TIM1->CCR2 = s_phase ? 0U : duty;
            TIM1->CCR3 = 0U;
            s_toggle_ticks = beep_half_ticks(beep_tone_frequency());
        } else {
            s_toggle_ticks--;
        }

        s_tone_ticks++;
        uint32_t tone_ticks = (ESC_BEEP_TONE_MS * 20U);
        if (s_tone_ticks >= tone_ticks) {
            hw_pwm_force_off();
            s_outputs_enabled = false;
            s_tone_count++;
            s_tone_ticks = 0U;
            s_tone_index++;
            s_gap_ticks = ESC_BEEP_GAP_MS * 20U;
            s_state = BEEP_STATE_GAP;
        }
    } else if (s_state == BEEP_STATE_GAP) {
        if (s_gap_ticks > 0U) {
            s_gap_ticks--;
            return;
        }
        if (s_tone_count >= beep_tone_target()) {
            s_active = false;
            s_event = ESC_BEEP_NONE;
            s_state = BEEP_STATE_IDLE;
            hw_pwm_force_off();
            return;
        }
        if (hw_pwm_enable_outputs() != HAL_OK) {
            hw_pwm_force_off();
            s_active = false;
            s_event = ESC_BEEP_NONE;
            s_state = BEEP_STATE_IDLE;
            return;
        }
        s_outputs_enabled = true;
        s_state = BEEP_STATE_TONE;
        s_toggle_ticks = beep_half_ticks(beep_tone_frequency());
    }
}
#endif
