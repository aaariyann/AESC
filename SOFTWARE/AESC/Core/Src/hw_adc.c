#include "hw_adc.h"
#include "esc_config.h"
#include "fault.h"

static ADC_HandleTypeDef *s_adc1;
static ADC_HandleTypeDef *s_adc2;
static float s_cal_sum_a;
static float s_cal_sum_b;
static uint32_t s_cal_count;
static volatile bool s_cal_done;
static uint32_t s_oc_debounce_count;
static float s_cont_i_amp_sq;
static uint16_t s_regular_pending;
static uint32_t s_bus_last_tick;
static uint32_t s_missing_pair_count;

void hw_adc_boot_init(ADC_HandleTypeDef *adc1,
                      ADC_HandleTypeDef *adc2,
                      OPAMP_HandleTypeDef *opamp1,
                      OPAMP_HandleTypeDef *opamp2)
{
    s_adc1 = adc1;
    s_adc2 = adc2;
    s_cal_sum_a = 0.0f;
    s_cal_sum_b = 0.0f;
    s_cal_count = 0U;
    s_cal_done = false;
    s_oc_debounce_count = 0U;
    s_cont_i_amp_sq = 0.0f;
    s_regular_pending = 0U;
    s_bus_last_tick = 0U;
    s_missing_pair_count = 0U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    if (HAL_OPAMP_Start(opamp1) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
    if (HAL_OPAMP_Start(opamp2) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }

    if (HAL_ADCEx_Calibration_Start(adc1, ADC_SINGLE_ENDED) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
    if (HAL_ADCEx_Calibration_Start(adc2, ADC_SINGLE_ENDED) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
}

void hw_adc_start_calibration(void)
{
    esc_debug.adc_a_offset = ESC_CS_OFFSET_COUNTS_A;
    esc_debug.adc_b_offset = ESC_CS_OFFSET_COUNTS_B;
    s_cal_done = false;

    ADC1->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
    ADC2->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;

    if (HAL_ADCEx_InjectedStart_IT(s_adc1) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }
    if (HAL_ADCEx_InjectedStart_IT(s_adc2) != HAL_OK) {
        fault_latch(ESC_FAULT_DETECTION);
    }

    ADC1->IER = ADC_IER_JEOSIE;
    ADC2->IER = 0U;
}

void hw_adc_isr(void)
{
    if (((ADC1->ISR & ADC_ISR_JEOS) == 0U) ||
        ((ADC2->ISR & ADC_ISR_JEOS) == 0U)) {
        s_missing_pair_count++;
        if (s_missing_pair_count > 5U) {
            fault_latch(ESC_FAULT_DETECTION);
        }
        ADC1->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
        ADC2->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
        return;
    }
    s_missing_pair_count = 0U;
    uint16_t raw_a = (uint16_t)ADC1->JDR1;
    uint16_t raw_b = (uint16_t)ADC2->JDR1;
    ADC1->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
    ADC2->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC;
    esc_debug.adc_a_raw = raw_a;
    esc_debug.adc_b_raw = raw_b;
    esc_debug.adc_injection_count++;

    s_cal_count++;
    if (!s_cal_done) {
        if (s_cal_count > ESC_CS_CAL_SETTLE_SAMPLES) {
            s_cal_sum_a += (float)raw_a;
            s_cal_sum_b += (float)raw_b;
        }

        if (s_cal_count == (ESC_CS_CAL_SETTLE_SAMPLES + ESC_CS_CAL_SAMPLES)) {
            float offset_a = s_cal_sum_a / (float)ESC_CS_CAL_SAMPLES;
            float offset_b = s_cal_sum_b / (float)ESC_CS_CAL_SAMPLES;
            if ((offset_a < ESC_CS_OFFSET_MIN_COUNTS) ||
                (offset_a > ESC_CS_OFFSET_MAX_COUNTS) ||
                (offset_b < ESC_CS_OFFSET_MIN_COUNTS) ||
                (offset_b > ESC_CS_OFFSET_MAX_COUNTS)) {
                fault_latch(ESC_FAULT_DETECTION);
                return;
            }
            esc_debug.adc_a_offset = offset_a;
            esc_debug.adc_b_offset = offset_b;
            s_cal_done = true;
        }
        return;
    }

    float count_a = (float)raw_a - esc_debug.adc_a_offset;
    float count_b = (float)raw_b - esc_debug.adc_b_offset;
    float volts_a = count_a * ESC_ADC_VREF / ESC_ADC_RESOLUTION_COUNTS;
    float volts_b = count_b * ESC_ADC_VREF / ESC_ADC_RESOLUTION_COUNTS;
    float ia = ESC_CURRENT_SIGN_A * volts_a / ESC_CS_V_PER_A;
    float ib = ESC_CURRENT_SIGN_B * volts_b / ESC_CS_V_PER_A;
    float ic = -(ia + ib);

    esc_debug.ia_a = ia;
    esc_debug.ib_a = ib;
    esc_debug.ic_a = ic;

    if ((ia > ESC_PHASE_PEAK_CURRENT_A) || (ia < -ESC_PHASE_PEAK_CURRENT_A) ||
        (ib > ESC_PHASE_PEAK_CURRENT_A) || (ib < -ESC_PHASE_PEAK_CURRENT_A) ||
        (ic > ESC_PHASE_PEAK_CURRENT_A) || (ic < -ESC_PHASE_PEAK_CURRENT_A)) {
        s_oc_debounce_count++;
        if (s_oc_debounce_count >= ESC_OC_DEBOUNCE_SAMPLES) {
            fault_latch(ESC_FAULT_OVERCURRENT);
            return;
        }
    } else {
        s_oc_debounce_count = 0U;
    }

    float i_amp_sq = (2.0f / 3.0f) * (ia * ia + ib * ib + ic * ic);
    s_cont_i_amp_sq += ESC_CONT_FILTER_ALPHA * (i_amp_sq - s_cont_i_amp_sq);
    if (s_cont_i_amp_sq > ESC_PHASE_CONT_CURRENT_A_SQUARED) {
        fault_latch(ESC_FAULT_OVERCURRENT);
    }
}

bool hw_adc_cal_done(void)
{
    return s_cal_done;
}

void hw_adc_regular_task(void)
{
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - s_bus_last_tick) < ESC_BUS_TASK_MS) {
        return;
    }
    s_bus_last_tick = now;

    if (s_regular_pending == 0U) {
        ADC1->ISR = ADC_ISR_EOC | ADC_ISR_EOS;
        if ((ADC1->CR & ADC_CR_ADSTART) == 0U) {
            ADC1->CR |= ADC_CR_ADSTART;
            s_regular_pending = 1U;
        }
        return;
    }

    if ((ADC1->ISR & ADC_ISR_EOS) != 0U) {
        uint16_t raw = (uint16_t)ADC1->DR;
        ADC1->ISR = ADC_ISR_EOC | ADC_ISR_EOS;
        float vbus = ((float)raw * ESC_ADC_VREF / ESC_ADC_RESOLUTION_COUNTS) / ESC_BUS_DIVIDER_RATIO;
        if (esc_debug.vbus_v <= 0.0f) {
            esc_debug.vbus_v = vbus;
        } else {
            esc_debug.vbus_v = esc_debug.vbus_v +
                               ESC_BUS_FILTER_K * (vbus - esc_debug.vbus_v);
        }
        s_regular_pending = 0U;

        if (vbus > (ESC_BUS_OVERVOLTAGE_V + 1.0f)) {
            fault_latch(ESC_FAULT_OVERVOLTAGE);
        }
        if (esc_debug.vbus_v < ESC_BUS_UNDERVOLTAGE_V) {
            fault_latch(ESC_FAULT_UNDERVOLTAGE);
        } else if (esc_debug.vbus_v > ESC_BUS_OVERVOLTAGE_V) {
            fault_latch(ESC_FAULT_OVERVOLTAGE);
        }
    }
}
