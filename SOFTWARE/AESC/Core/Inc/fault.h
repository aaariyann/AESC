#ifndef FAULT_H
#define FAULT_H

#include "esc_types.h"

void fault_boot_init(void);
void fault_latch(esc_fault_t fault);
esc_fault_t fault_get(void);
uint32_t fault_led_code(void);
void fault_led_task(void);
void fault_iwdg_refresh(void);
extern esc_debug_t esc_debug;

#endif
