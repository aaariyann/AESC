#ifndef HW_PWM_H
#define HW_PWM_H

#include "stm32g4xx_hal.h"
#include "esc_types.h"

void hw_pwm_init(TIM_HandleTypeDef *htim);
void hw_pwm_precharge(TIM_HandleTypeDef *htim);
void hw_pwm_force_off(void);
void hw_pwm_set_precharge_duty(TIM_HandleTypeDef *htim);
void hw_pwm_refresh_adc_trigger(void);
HAL_StatusTypeDef hw_pwm_enable_outputs(void);

#endif
