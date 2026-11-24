#ifndef ESC_CONFIG_H
#define ESC_CONFIG_H

#include <stdint.h>

#define ESC_CPU_HZ                         (170000000UL)
#define ESC_PWM_HZ                         (20000UL)
#define ESC_PWM_ARR                        (4249U)
#define ESC_PWM_RCR                        (1U)
#define ESC_PWM_PERIOD_US                  (50.0f)
#define ESC_PWM_TICKS_PER_US               (170.0f)
#define ESC_MAX_DUTY                       (0.90f)
#define ESC_ABSOLUTE_MAX_DUTY              (0.95f)

#define ESC_DEADTIME_NS                    (400U)
#define ESC_DEADTIME_NS_PER_DTG            (5.88f)
#define ESC_DEADTIME_DTG                   (((ESC_DEADTIME_NS * 100U) + 294U) / 588U)
_Static_assert(ESC_DEADTIME_DTG <= 127U, "ESC_DEADTIME_DTG exceeds TIM1 DTG range");
#define ESC_ADC_TRIG_LEAD                  (30U)

#define ESC_ADC_RESOLUTION_COUNTS          (4095.0f)
#define ESC_ADC_VREF                       (3.3f)
#define ESC_CS_V_PER_A                     (0.0349f)
#define ESC_CS_OFFSET_COUNTS_A             (2095.0f)
#define ESC_CS_OFFSET_COUNTS_B             (2095.0f)
#define ESC_CS_OFFSET_MIN_COUNTS           (1600U)
#define ESC_CS_OFFSET_MAX_COUNTS           (2600U)
#define ESC_CS_CAL_SETTLE_SAMPLES          (64U)
#define ESC_CS_CAL_SAMPLES                 (1024U)
#define ESC_CURRENT_SIGN_A                 (-1.0f)
#define ESC_CURRENT_SIGN_B                 (-1.0f)
/* TODO(verify): 12 A trip may nuisance-trip during motor startup; revisit before motor tests */
#define ESC_PHASE_PEAK_CURRENT_A           (12.0f)
#define ESC_PHASE_CONT_CURRENT_A           (10.0f)
#define ESC_OC_DEBOUNCE_SAMPLES            (3U)
#define ESC_CONT_FILTER_TAU_MS             (500UL)
#define ESC_CONT_SAMPLE_TIME_S             (1.0f / 20000.0f)
#define ESC_CONT_FILTER_TAU_S              ((float)ESC_CONT_FILTER_TAU_MS / 1000.0f)
#define ESC_CONT_FILTER_ALPHA              (ESC_CONT_SAMPLE_TIME_S / ESC_CONT_FILTER_TAU_S)
#define ESC_PHASE_CONT_CURRENT_A_SQUARED   (ESC_PHASE_CONT_CURRENT_A * ESC_PHASE_CONT_CURRENT_A)

#define ESC_BUS_DIVIDER_RATIO              (0.1247f)
#define ESC_BUS_UNDERVOLTAGE_V             (12.0f)
#define ESC_BUS_OVERVOLTAGE_V              (18.0f)
#define ESC_BUS_REGEN_LIMIT_V              (17.5f)
#define ESC_BUS_TASK_MS                    (1U)
#define ESC_BUS_FILTER_K                   ((float)ESC_BUS_TASK_MS / (float)ESC_CONT_FILTER_TAU_MS)

#define ESC_BOOTSTRAP_PRECHARGE_MS         (10U)
#define ESC_BOOTSTRAP_PRECHARGE_DUTY       (0.01f)

#define ESC_IWDG_LSI_HZ                    (32000UL)
#define ESC_IWDG_PRESCALER                 (32UL)
#define ESC_IWDG_TIMEOUT_MS                (100UL)
#define ESC_IWDG_UNLOCK_KEY                (0x5555UL)
#define ESC_IWDG_START_KEY                 (0xCCCCUL)
#define ESC_IWDG_REFRESH_KEY               (0xAAAAUL)
#define ESC_IWDG_PRESCALER_REGISTER        (0x3UL)
#define ESC_IWDG_RELOAD                    (((ESC_IWDG_LSI_HZ / ESC_IWDG_PRESCALER) * ESC_IWDG_TIMEOUT_MS / 1000UL) - 1UL)

#define ESC_LED_BLINK_ON_MS                (250U)
#define ESC_LED_BLINK_OFF_MS               (250U)
#define ESC_LED_BLINK_GAP_MS               (1250U)

#define ESC_THROTTLE_PULSE_MIN_US          (800U)
#define ESC_THROTTLE_PULSE_MAX_US          (2200U)
#define ESC_THROTTLE_PERIOD_MIN_US         (5000U)
#define ESC_THROTTLE_PERIOD_MAX_US         (50000U)
#define ESC_THROTTLE_TIMEOUT_MS            (100U)
#define ESC_THROTTLE_DEFAULT_MIN_US        (1000U)
#define ESC_THROTTLE_DEFAULT_MAX_US        (2000U)
#define ESC_THROTTLE_DEFAULT_NEUTRAL_US   (1500U)
#define ESC_THROTTLE_NEUTRAL_DEADBAND_US  (50U)
#define ESC_THROTTLE_CAL_ENTER_US          (1950U)
#define ESC_THROTTLE_CAL_MIN_THRESHOLD_US  (1100U)
#define ESC_THROTTLE_CAL_NEUTRAL_MIN_US    (1450U)
#define ESC_THROTTLE_CAL_NEUTRAL_MAX_US    (1550U)
#define ESC_THROTTLE_CAL_STAGE_MS          (1500U)
#define ESC_THROTTLE_ARMING_MS             (1000U)
#define ESC_THROTTLE_CAL_TIMEOUT_MS        (30000UL)
#define ESC_THROTTLE_CAL_ENTRY_TIMEOUT_MS  (5000UL)
#define ESC_THROTTLE_FLASH_MAGIC           (0x45534354UL)
/* TODO: reserve the last 2 KB flash page in the linker script before enabling ESC_FEATURE_THROTTLE */
#define ESC_THROTTLE_FLASH_PAGE            (63U)
#define ESC_THROTTLE_FLASH_ADDRESS         (0x0801F800UL)
#define ESC_THROTTLE_FLASH_CRC_POLY        (0xEDB88320UL)

#define ESC_BEEP_TONE_HALF_MS              (1U)
#define ESC_BEEP_DUTY                      (0.02f)
#define ESC_BEEP_CURRENT_LIMIT_A           (2.0f)
#define ESC_BEEP_STARTUP_TONES             (2U)
#define ESC_BEEP_TONE_MS                   (100U)
#define ESC_BEEP_GAP_MS                    (80U)
#define ESC_BEEP_CAL_TONE_MS               (200U)

#define ESC_REPRECHARGE_IDLE_MS            (300000UL)

/* TODO(verify): Motor electrical parameters are placeholders. */
#define ESC_MOTOR_POLE_PAIRS               (7U)
#define ESC_MOTOR_RS_OHM                   (0.050f)
#define ESC_MOTOR_LS_H                     (0.000050f)
#define ESC_MOTOR_FLUX_VS_RAD              (0.005f)

#define ESC_FEATURE_THROTTLE               (0U)
#define ESC_FEATURE_BEEP                   (0U)
#define ESC_FEATURE_MOTOR_STATE            (0U)

#endif
