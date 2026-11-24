#include "motor_state.h"
#include "esc_config.h"
#include "esc_types.h"
#include "beep.h"
#include "throttle.h"
#include "hw_adc.h"
#include "hw_pwm.h"
#include "fault.h"

#if ESC_FEATURE_MOTOR_STATE

static esc_motor_state_t s_state;
static bool s_precharged;
static bool s_startup_beep_done;
static bool s_cal_enter_seen;
static uint32_t s_state_enter_tick;
static uint32_t s_idle_tick;
static uint32_t s_arming_tick;
static uint32_t s_powerup_tick;
static bool s_armed_forced_off;

static void motor_state_set(esc_motor_state_t state)
{
    s_state = state;
    esc_debug.state = state;
    s_state_enter_tick = HAL_GetTick();
}

void motor_state_init(void)
{
    throttle_init();
    beep_init();
    s_precharged = false;
    s_startup_beep_done = false;
    s_cal_enter_seen = false;
    s_idle_tick = HAL_GetTick();
    s_arming_tick = 0U;
    s_powerup_tick = HAL_GetTick();
    s_armed_forced_off = false;
    motor_state_set(ESC_MOTOR_STATE_INIT);
}

void motor_state_task(TIM_HandleTypeDef *htim)
{
    (void)htim;
    if (fault_get() != ESC_FAULT_NONE) {
        hw_pwm_force_off();
        motor_state_set(ESC_MOTOR_STATE_FAULT);
        if (!beep_is_active()) {
            beep_request(ESC_BEEP_FAULT);
        }
        return;
    }

    uint32_t now = HAL_GetTick();
    if (s_state == ESC_MOTOR_STATE_INIT) {
        motor_state_set(hw_adc_cal_done()
                            ? ESC_MOTOR_STATE_PRECHARGE
                            : ESC_MOTOR_STATE_CALIBRATING_ADC);
    } else if (s_state == ESC_MOTOR_STATE_CALIBRATING_ADC) {
        if (hw_adc_cal_done()) {
            motor_state_set(ESC_MOTOR_STATE_PRECHARGE);
        }
    } else if (s_state == ESC_MOTOR_STATE_PRECHARGE) {
        if (esc_debug.vbus_v >= ESC_BUS_UNDERVOLTAGE_V &&
            esc_debug.vbus_v <= ESC_BUS_OVERVOLTAGE_V) {
            hw_pwm_precharge(htim);
            s_precharged = true;
            s_idle_tick = now;
            motor_state_set(ESC_MOTOR_STATE_DISARMED);
        }
    } else if (s_state == ESC_MOTOR_STATE_DISARMED) {
        if (!s_startup_beep_done) {
            beep_request(ESC_BEEP_STARTUP);
            s_startup_beep_done = true;
        }

        if ((uint32_t)(now - s_powerup_tick) <= ESC_THROTTLE_CAL_ENTRY_TIMEOUT_MS &&
            !s_cal_enter_seen &&
            throttle_is_valid() &&
            throttle_get_pulse_us() >= ESC_THROTTLE_CAL_ENTER_US) {
            s_cal_enter_seen = true;
            s_state_enter_tick = now;
        }
        if (s_cal_enter_seen &&
            throttle_is_valid() &&
            throttle_get_pulse_us() >= ESC_THROTTLE_CAL_ENTER_US &&
            (uint32_t)(now - s_state_enter_tick) >= ESC_THROTTLE_ARMING_MS) {
            beep_request(ESC_BEEP_CAL_ENTER);
            throttle_calibration_start();
            motor_state_set(ESC_MOTOR_STATE_CALIBRATING);
        }

        if (!throttle_calibration_active() &&
            throttle_is_valid() &&
            throttle_get_pulse_us() >=
                (throttle_get_neutral_us() - ESC_THROTTLE_NEUTRAL_DEADBAND_US) &&
            throttle_get_pulse_us() <=
                (throttle_get_neutral_us() + ESC_THROTTLE_NEUTRAL_DEADBAND_US)) {
            motor_state_set(ESC_MOTOR_STATE_ARMING);
            s_arming_tick = now;
        }

        if (s_precharged &&
            (uint32_t)(now - s_idle_tick) >= ESC_REPRECHARGE_IDLE_MS) {
            s_precharged = false;
            motor_state_set(ESC_MOTOR_STATE_PRECHARGE);
        }
    } else if (s_state == ESC_MOTOR_STATE_ARMING) {
        if (!throttle_is_valid() ||
            throttle_get_pulse_us() <
                (throttle_get_neutral_us() - ESC_THROTTLE_NEUTRAL_DEADBAND_US) ||
            throttle_get_pulse_us() >
                (throttle_get_neutral_us() + ESC_THROTTLE_NEUTRAL_DEADBAND_US)) {
            motor_state_set(ESC_MOTOR_STATE_DISARMED);
        } else if ((uint32_t)(now - s_arming_tick) >= ESC_THROTTLE_ARMING_MS) {
            beep_request(ESC_BEEP_ARMING);
            s_armed_forced_off = false;
            motor_state_set(ESC_MOTOR_STATE_ARMED);
        }
    } else if (s_state == ESC_MOTOR_STATE_ARMED) {
        if (!throttle_is_valid()) {
            hw_pwm_force_off();
            s_armed_forced_off = true;
            motor_state_set(ESC_MOTOR_STATE_DISARMED);
        } else if (!s_armed_forced_off) {
            hw_pwm_force_off();
            s_armed_forced_off = true;
        }
    } else if (s_state == ESC_MOTOR_STATE_CALIBRATING) {
        throttle_calibration_task();
        if (!throttle_calibration_active()) {
            if (fault_get() == ESC_FAULT_NONE &&
                throttle_calibration_complete()) {
                beep_request(ESC_BEEP_CAL_DONE);
            }
            s_cal_enter_seen = false;
            motor_state_set(ESC_MOTOR_STATE_DISARMED);
        }
    }
}
#endif
