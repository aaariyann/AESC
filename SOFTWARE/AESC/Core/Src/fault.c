#include "fault.h"
#include "esc_config.h"
#include "main.h"

esc_debug_t esc_debug;

void fault_boot_init(void)
{
    esc_debug.fault = ESC_FAULT_NONE;
    esc_debug.fault_led_code = 0U;
    esc_debug.adc_a_raw = 0U;
    esc_debug.adc_b_raw = 0U;
    esc_debug.adc_a_offset = ESC_CS_OFFSET_COUNTS_A;
    esc_debug.adc_b_offset = ESC_CS_OFFSET_COUNTS_B;
    esc_debug.ia_a = 0.0f;
    esc_debug.ib_a = 0.0f;
    esc_debug.ic_a = 0.0f;
    esc_debug.vbus_v = 0.0f;
    esc_debug.adc_injection_count = 0U;
    esc_debug.isr_cycles_max = 0U;
    esc_debug.state = ESC_MOTOR_STATE_INIT;
    esc_debug.rpm_estimate = 0.0f;
    esc_debug.id_a = 0.0f;
    esc_debug.iq_a = 0.0f;
    esc_debug.vd_v = 0.0f;
    esc_debug.vq_v = 0.0f;
    esc_debug.duty = 0.0f;
    esc_debug.throttle_us = 0U;
    esc_debug.regen_flag = false;
    HAL_GPIO_WritePin(STATUS_GPIO_Port, STATUS_Pin, GPIO_PIN_RESET);

    IWDG->KR = ESC_IWDG_START_KEY;
    IWDG->KR = ESC_IWDG_UNLOCK_KEY;
    IWDG->PR = ESC_IWDG_PRESCALER_REGISTER;
    IWDG->RLR = ESC_IWDG_RELOAD;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t iwdg_start = DWT->CYCCNT;
    bool iwdg_wait_timed_out = true;
    uint32_t iwdg_wait = 0U;
    while (iwdg_wait < (170000000UL / 50UL)) {
        if (IWDG->SR == 0U) {
            iwdg_wait_timed_out = false;
            break;
        }
        iwdg_wait++;
        if ((DWT->CYCCNT - iwdg_start) >= (170000000UL / 50UL)) {
            break;
        }
    }
    if (iwdg_wait_timed_out) {
        fault_latch(ESC_FAULT_DETECTION);
        return;
    }
    IWDG->KR = ESC_IWDG_REFRESH_KEY;
}

void fault_latch(esc_fault_t fault)
{
    if (esc_debug.fault == ESC_FAULT_NONE) {
        esc_debug.fault = fault;
        esc_debug.fault_led_code = (uint32_t)fault;
    }
    TIM1->CCR1 = 0U;
    TIM1->CCR2 = 0U;
    TIM1->CCR3 = 0U;
    TIM1->BDTR &= ~TIM_BDTR_MOE;
}

esc_fault_t fault_get(void)
{
    return esc_debug.fault;
}

uint32_t fault_led_code(void)
{
    return esc_debug.fault_led_code;
}

void fault_led_task(void)
{
    static uint32_t phase_start;
    static uint32_t gap_start;
    static uint8_t pulse_index;

    uint32_t now = HAL_GetTick();
    if (esc_debug.fault == ESC_FAULT_NONE) {
        HAL_GPIO_WritePin(STATUS_GPIO_Port, STATUS_Pin, GPIO_PIN_RESET);
        phase_start = now;
        gap_start = now;
        pulse_index = 0U;
        return;
    }

    uint32_t code = esc_debug.fault_led_code;
    uint32_t elapsed;

    if (pulse_index >= code) {
        elapsed = now - gap_start;
        HAL_GPIO_WritePin(STATUS_GPIO_Port, STATUS_Pin, GPIO_PIN_RESET);
        if (elapsed >= ESC_LED_BLINK_GAP_MS) {
            pulse_index = 0U;
            phase_start = now;
            gap_start = now;
        }
        return;
    }

    elapsed = now - phase_start;
    if (elapsed < ESC_LED_BLINK_ON_MS) {
        HAL_GPIO_WritePin(STATUS_GPIO_Port, STATUS_Pin, GPIO_PIN_SET);
    } else if (elapsed < (ESC_LED_BLINK_ON_MS + ESC_LED_BLINK_OFF_MS)) {
        HAL_GPIO_WritePin(STATUS_GPIO_Port, STATUS_Pin, GPIO_PIN_RESET);
    } else {
        pulse_index++;
        phase_start = now;
        if (pulse_index >= code) {
            gap_start = now;
        }
    }
}

void fault_iwdg_refresh(void)
{
    IWDG->KR = ESC_IWDG_REFRESH_KEY;
}
