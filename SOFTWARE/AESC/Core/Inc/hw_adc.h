#ifndef HW_ADC_H
#define HW_ADC_H

#include "stm32g4xx_hal.h"
#include "esc_types.h"
#include <stdbool.h>

void hw_adc_boot_init(ADC_HandleTypeDef *adc1,
                      ADC_HandleTypeDef *adc2,
                      OPAMP_HandleTypeDef *opamp1,
                      OPAMP_HandleTypeDef *opamp2);
void hw_adc_start_calibration(void);
void hw_adc_isr(void);
void hw_adc_regular_task(void);
bool hw_adc_cal_done(void);

#endif
