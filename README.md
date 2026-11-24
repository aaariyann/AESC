# AESC: a regenerative, sensorless BLDC ESC

AESC is an electronic speed controller for three-phase brushless motors, built around an STM32G431. The end goal is sensorless field-oriented control with regenerative braking that pushes energy back into the battery, the same basic idea an EV traction inverter uses, just small enough to hand-solder and understand from the schematic to the last line of firmware.

It behaves like a normal RC ESC from the outside: standard 1000-2000 µs throttle input, startup beeps through the motor, throttle endpoint calibration, and an arming sequence.

## Where the project stands

| Part | Status |
|---|---|
| Hardware (schematic, 4-layer 100 x 100 mm board) | Designed |
| Firmware stage A: sensing, protection, throttle, calibration, arming, beeps | Written, builds with the feature flags off and on |
| Stage B: rotor alignment and open-loop spin | Not started |
| Stage C: sensorless FOC (observer, current loops, handover) | Not started |
| Stage D: regenerative braking with bus-voltage limiting | Not started |
| Bench results | To be added |

The motor does not spin yet. Stage A is the bring-up layer: it measures, protects and listens, and keeps every output off until it is safe to do otherwise.

## Hardware

Everything is chosen to be buyable from LCSC and solderable by hand.

| Block | Part |
|---|---|
| MCU | STM32G431CBT6 (LQFP-48, internal 16 MHz oscillator, no crystal) |
| Gate driver | DRV8300DPW (TSSOP-20, integrated bootstrap diodes, fixed 200 ns dead time) |
| Power stage | 6 x IRFR7546 (60 V, DPAK) |
| Current sensing | 10 mΩ low-side shunts, read through the G431's internal op-amps |
| 12 V rail | L78M12CDT (gate drive) |
| 3.3 V rail | AMS1117-3.3, with a ferrite bead feeding VDDA |
| Input protection | SMBJ28A TVS |
| Power connector | XT60 |

**Design target:** 4S battery, about 10 A per phase continuous. The firmware limits are set for 4S (details below). Anything higher needs the part ratings and thresholds reviewed first.

**How current is measured.** Each of the three phases has a shunt, but only phases A and B go through an op-amp (OPAMP1 on PA1, OPAMP2 on PA7). Phase C is computed from the other two. The op-amps run in PGA mode at gain 4 with a small offset network so the output rests near mid-rail and negative current can be read. That works out to roughly 23 mA per ADC count. Because the shunts are on the low side, a sample is only valid while the low-side FET is on, so the ADC is triggered at the peak of the center-aligned PWM and the duty cycle is capped at 90%.

**Pin map**

| Pin | Function |
|---|---|
| PA8, PA9, PA10 | TIM1 CH1-3, high-side inputs to the driver (INHA/B/C) |
| PB13, PB14, PB15 | TIM1 CH1N-3N, low-side inputs to the driver (INLA/B/C) |
| PA0 | Bus voltage (33 kΩ / 4.7 kΩ divider, about 26 V full scale) |
| PA1 | Phase A current (OPAMP1) |
| PA7 | Phase B current (OPAMP2) |
| PA15 | Throttle input (TIM2 CH1, PWM input mode) |
| PA11 | Status LED |
| PA13, PA14 | SWDIO, SWCLK |

## Firmware (stage A)

Built with STM32CubeIDE 1.18 and the STM32 HAL.

**Timing.** TIM1 runs center-aligned at 20 kHz with 400 ns of dead time on top of the driver's own 200 ns. The ADC conversions are triggered by TIM1 channel 4, and the ADC interrupt (20 kHz, highest priority) is where the control loop will live.

**What stage A does**

- Calibrates the current-sense offsets at startup with all outputs off, and rejects offsets that look wrong.
- Reads phase currents and bus voltage continuously.
- Latches a fault on overcurrent (instantaneous peak and a filtered continuous limit), undervoltage and overvoltage.
- Runs a 50 ms independent watchdog. Hard faults and NMI turn the outputs off, and a CPU lockup is wired to the TIM1 break input.
- Charges the bootstrap capacitors before the first run, and again after long idle periods.
- Measures the throttle pulse in hardware (period and pulse width, 1 µs resolution) and treats the signal as lost if no new pulse arrives within the timeout. A missing signal keeps the ESC disarmed. It is not treated as a fault.
- Calibrates throttle endpoints like a normal ESC and stores them in the last flash page: hold full throttle shortly after power-up, then move to minimum, then neutral.
- Arms after the throttle sits at neutral long enough.
- Plays startup, arming and calibration tones through the motor windings at low duty, and aborts immediately on overcurrent or stick movement.

**Fault codes.** When a fault latches, the status LED blinks the code, then pauses and repeats. Faults stay latched until the next power cycle.

| Blinks | Fault |
|---|---|
| 1 | Overcurrent |
| 2 | Undervoltage |
| 3 | Overvoltage |
| 4 | Throttle loss |
| 5 | Observer lost (reserved for stage C) |
| 6 | Detection or internal error |

**Feature flags.** `esc_config.h` has three switches, all defaulting to 0, so the board can be brought up in steps:

| Flag | Enables |
|---|---|
| `ESC_FEATURE_THROTTLE` | TIM2 capture, throttle reading, calibration |
| `ESC_FEATURE_BEEP` | tones through the windings |
| `ESC_FEATURE_MOTOR_STATE` | precharge, calibration entry, arming state machine |

With all three at 0 the firmware only senses and protects, and the power stage never turns on.

**Source layout**

| File | Role |
|---|---|
| `esc_config.h` | Every tunable limit and constant |
| `esc_types.h` | Fault codes, states, shared debug struct |
| `hw_pwm.c` | TIM1 setup, output enable and force-off, bootstrap precharge |
| `hw_adc.c` | Current and voltage sampling, offset calibration, protection |
| `fault.c` | Fault latching, watchdog, LED codes |
| `throttle.c` | Throttle capture, validation, calibration, flash storage |
| `beep.c` | Tone generation |
| `motor_state.c` | Arming state machine |
| `console.c` | Empty placeholder for a future UART console |

## Building and first power-up

1. Open the project in STM32CubeIDE and build the Debug configuration.
2. Flash over SWD.
3. First power-up: bench supply at 13-16 V, current limit around 0.5 A, no propeller.
4. Add `esc_debug` to Live Expressions and check:
   - `fault` stays 0
   - `adc_injection_count` climbs by about 20000 per second
   - `adc_a_offset` and `adc_b_offset` sit near 2095
   - `vbus_v` matches a multimeter
   - `ia_a` and `ib_a` read close to zero
   - `isr_cycles_max` stays well under 4000
5. Set `ESC_FEATURE_THROTTLE` to 1 and confirm `throttle_us` follows a servo tester and drops to 0 when the signal is unplugged.
6. Then enable the beep and motor-state flags.

Before any test with a real motor, remove the `__HAL_DBGMCU_FREEZE_IWDG()` line in `main.c`. It stops the watchdog at breakpoints, which is useful while debugging and wrong for anything that moves.

## Roadmap

- **Stage B:** rotor alignment and an open-loop spin-up.
- **Stage C:** sensorless FOC with Clarke and Park transforms, PI current loops, SVPWM, a back-EMF observer, and handover from open loop. Throttle above neutral commands forward torque.
- **Stage D:** regenerative braking as negative torque below neutral, scaled back as the bus approaches its limit and cut off entirely above it. Motor parameters (pole pairs, resistance, inductance) will be set in `esc_config.h`.

## Known limitations

- Overcurrent protection is software only, checked every 50 µs. There is no hardware comparator trip yet, so use a current-limited supply for early tests.
- The current trip is set at 12 A with a short debounce. That may trip during motor startup and is flagged to revisit before spinning anything.
- The throttle calibration page still needs to be reserved in the linker script. Do this before enabling the throttle feature on a firmware that has grown.
- Voltage thresholds are 12 V undervoltage, 18 V overvoltage and 17.5 V regen limit, which assume 4S. The regen limit is defined but not used yet.
- The watchdog timeout is short compared with a flash page erase, so calibration saving needs the watchdog refreshed around it (the code does this).

## Repository layout

```
hardware/   KiCad project (schematic and PCB)
firmware/   STM32CubeIDE project "AESC" (Core/Src, Core/Inc)
docs/       notes and test results
```

## Author

Built by Ariyan Siddique ([github.com/aaariyann](https://github.com/aaariyann)).
