#ifndef ESC_TYPES_H
#define ESC_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ESC_FAULT_NONE = 0,
    ESC_FAULT_OVERCURRENT,
    ESC_FAULT_UNDERVOLTAGE,
    ESC_FAULT_OVERVOLTAGE,
    ESC_FAULT_THROTTLE_LOSS,
    ESC_FAULT_OBSERVER_LOST,
    ESC_FAULT_DETECTION
} esc_fault_t;

typedef enum {
    ESC_MOTOR_STATE_INIT = 0,
    ESC_MOTOR_STATE_CALIBRATING_ADC,
    ESC_MOTOR_STATE_PRECHARGE,
    ESC_MOTOR_STATE_DISARMED,
    ESC_MOTOR_STATE_ARMING,
    ESC_MOTOR_STATE_ARMED,
    ESC_MOTOR_STATE_CALIBRATING,
    ESC_MOTOR_STATE_ALIGN,
    ESC_MOTOR_STATE_OPENLOOP,
    ESC_MOTOR_STATE_RUN,
    ESC_MOTOR_STATE_FAULT
} esc_motor_state_t;

typedef struct {
    volatile esc_fault_t fault;
    volatile uint16_t adc_a_raw;
    volatile uint16_t adc_b_raw;
    volatile float adc_a_offset;
    volatile float adc_b_offset;
    volatile float ia_a;
    volatile float ib_a;
    volatile float ic_a;
    volatile float vbus_v;
    volatile uint32_t adc_injection_count;
    volatile uint32_t fault_led_code;
    volatile uint32_t isr_cycles_max;
    volatile esc_motor_state_t state;
    volatile float rpm_estimate;
    volatile float id_a;
    volatile float iq_a;
    volatile float vd_v;
    volatile float vq_v;
    volatile float duty;
    volatile uint16_t throttle_us;
    volatile bool regen_flag;
} esc_debug_t;

#endif
