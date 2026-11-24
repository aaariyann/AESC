#include "hw_pwm.h"
#include "esc_config.h"
#include "fault.h"
#include "hw_adc.h"

void hw_pwm_force_off(void)
{
    TIM1->CCR1 = 0U;
    TIM1->CCR2 = 0U;
    TIM1->CCR3 = 0U;
    TIM1->BDTR &= ~TIM_BDTR_MOE;
}

HAL_StatusTypeDef hw_pwm_enable_outputs(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    TIM1->BDTR &= ~TIM_BDTR_MOE;

    if (fault_get() != ESC_FAULT_NONE ||
        !hw_adc_cal_done()) {
        if (primask == 0U) {
            __enable_irq();
        }
        return HAL_ERROR;
    }

    float vbus = esc_debug.vbus_v;
    if ((vbus <= 0.0f) || (vbus < ESC_BUS_UNDERVOLTAGE_V) ||
        (vbus > ESC_BUS_OVERVOLTAGE_V)) {
        if (primask == 0U) {
            __enable_irq();
        }
        return HAL_ERROR;
    }

    TIM1->BDTR |= TIM_BDTR_MOE;
    if ((TIM1->BDTR & TIM_BDTR_MOE) == 0U ||
        fault_get() != ESC_FAULT_NONE) {
        hw_pwm_force_off();
        if (primask == 0U) {
            __enable_irq();
        }
        return HAL_ERROR;
    }
    if (primask == 0U) {
        __enable_irq();
    }
    return HAL_OK;
}

void hw_pwm_init(TIM_HandleTypeDef *htim)
{
    if (htim == NULL) {
        return;
    }

    hw_pwm_force_off();
    TIM1->ARR = ESC_PWM_ARR;
    TIM1->RCR = ESC_PWM_RCR;
    TIM1->CCR1 = 0U;
    TIM1->CCR2 = 0U;
    TIM1->CCR3 = 0U;
    TIM1->CCR4 = ESC_PWM_ARR - ESC_ADC_TRIG_LEAD;

    TIM1->AF1 &= ~TIM1_AF1_BKINE;

    TIM1->BDTR = (TIM1->BDTR & ~TIM_BDTR_DTG) | ESC_DEADTIME_DTG;
    /* Lock level 1 prevents software changes until MCU reset. */
    TIM1->BDTR = (TIM1->BDTR & ~TIM_BDTR_LOCK) | TIM_BDTR_LOCK_1;
    TIM1->DIER |= TIM_DIER_BIE;

    if ((TIM1->AF1 & TIM1_AF1_BKINE) != 0U) {
        /* TODO(verify): TIM1 BKIN pin source remains enabled after AF1 clear. */
    }

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    SYSCFG->CFGR2 |= SYSCFG_CFGR2_CLL;

    FLASH->ACR |= FLASH_ACR_PRFTEN | FLASH_ACR_ICEN;

    if (HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
    if (HAL_TIMEx_PWMN_Start(htim, TIM_CHANNEL_1) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
    if (HAL_TIM_PWM_Start(htim, TIM_CHANNEL_2) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
    if (HAL_TIMEx_PWMN_Start(htim, TIM_CHANNEL_2) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
    if (HAL_TIM_PWM_Start(htim, TIM_CHANNEL_3) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
    if (HAL_TIMEx_PWMN_Start(htim, TIM_CHANNEL_3) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
    if (HAL_TIM_PWM_Start(htim, TIM_CHANNEL_4) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }

    hw_pwm_force_off();
    if ((TIM1->BDTR & TIM_BDTR_MOE) != 0U) {
        fault_latch(ESC_FAULT_DETECTION);
        hw_pwm_force_off();
    }
}

void hw_pwm_set_precharge_duty(TIM_HandleTypeDef *htim)
{
    if (htim == NULL) {
        return;
    }
    uint32_t duty = (uint32_t)(ESC_BOOTSTRAP_PRECHARGE_DUTY * (float)ESC_PWM_ARR);
    TIM1->CCR1 = duty;
    TIM1->CCR2 = duty;
    TIM1->CCR3 = duty;
}

void hw_pwm_precharge(TIM_HandleTypeDef *htim)
{
    hw_pwm_set_precharge_duty(htim);
    if (hw_pwm_enable_outputs() == HAL_OK) {
        HAL_Delay(ESC_BOOTSTRAP_PRECHARGE_MS);
    }
    hw_pwm_force_off();
}

void hw_pwm_refresh_adc_trigger(void)
{
    TIM1->CCR4 = ESC_PWM_ARR - ESC_ADC_TRIG_LEAD;
}
