#ifndef MOTOR_STATE_H
#define MOTOR_STATE_H

#include "stm32g4xx_hal.h"

void motor_state_init(void);
void motor_state_task(TIM_HandleTypeDef *htim);

#endif
