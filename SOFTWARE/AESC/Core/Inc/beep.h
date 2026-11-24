#ifndef BEEP_H
#define BEEP_H

#include <stdbool.h>

typedef enum {
    ESC_BEEP_NONE = 0,
    ESC_BEEP_STARTUP,
    ESC_BEEP_ARMING,
    ESC_BEEP_CAL_ENTER,
    ESC_BEEP_CAL_STEP,
    ESC_BEEP_CAL_DONE,
    ESC_BEEP_FAULT
} esc_beep_event_t;

void beep_init(void);
void beep_request(esc_beep_event_t event);
void beep_task(bool throttle_activity);
bool beep_is_active(void);
void beep_tick(void);

#endif
