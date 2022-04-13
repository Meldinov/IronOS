/*
 * Setup.c
 *
 *  Created on: 29Aug.,2017
 *      Author: Ben V. Brown
 */
#include "Setup.h"
#include "BSP.h"
#include "Debug.h"
#include "FreeRTOSConfig.h"
#include "Pins.h"
extern "C" {
#include "bflb_platform.h"
#include "bl702_glb.h"
#include "bl702_i2c.h"
#include "bl702_pwm.h"
#include "bl702_timer.h"
#include "hal_clock.h"
#include "hal_pwm.h"
#include "hal_timer.h"
}
#include "IRQ.h"
#include "history.hpp"
#include <string.h>
#define ADC_NORM_SAMPLES 16
#define ADC_FILTER_LEN   4
uint16_t ADCReadings[ADC_NORM_SAMPLES]; // room for 32 lots of the pair of readings

// Functions

void setup_slow_PWM();
void hardware_init() {
  gpio_set_mode(OLED_RESET_Pin, GPIO_OUTPUT_MODE);
  // gpio_set_mode(KEY_A_Pin, GPIO_INPUT_PD_MODE);
  // gpio_set_mode(KEY_B_Pin, GPIO_INPUT_PD_MODE);
  setup_slow_PWM();
}

struct device *timer0;

volatile uint32_t cnt = 0;

void setup_slow_PWM() {

  timer_register(TIMER0_INDEX, "timer0");

  timer0 = device_find("timer0");

  if (timer0) {

    device_open(timer0, DEVICE_OFLAG_INT_TX); /* 1s,2s,3s timing*/
    // Set interrupt handler
    device_set_callback(timer0, timer0_irq_callback);
    // Enable both interrupts (0 and 1)
    device_control(timer0, DEVICE_CTRL_SET_INT, (void *)(TIMER_COMP0_IT | TIMER_COMP1_IT | TIMER_COMP2_IT));

    TIMER_SetCompValue(TIMER_CH0, TIMER_COMP_ID_0, 255 + 10 - 2); // Channel 0 is used to trigger the ADC
    TIMER_SetCompValue(TIMER_CH0, TIMER_COMP_ID_1, 128 - 2);
    TIMER_SetCompValue(TIMER_CH0, TIMER_COMP_ID_2, (255 + 10 + 10) - 2); // We are using compare 2 to set the max duration of the timer
    TIMER_SetPreloadValue(TIMER_CH0, 0);
    TIMER_SetCountMode(TIMER_CH0, TIMER_COUNT_PRELOAD);
  } else {
    MSG((char *)"timer device open failed! \n");
  }
}

/*
 * ADC
 * ADC is a bit of a mess as we dont have injected mode sampling to use with timers on this device
 * So instead we do this:
 *
 * Main timer0 runs at 5/10hz and schedules everything
 * Its running PWM in the 0-255 range for tip control + sample time at the end of the ADC
 * It triggers the ADC at the end of the PWM cycle
 *
 *
 */
uint16_t getADCHandleTemp(uint8_t sample) {
  static history<uint16_t, ADC_FILTER_LEN> filter = {{0}, 0, 0};
  if (sample) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_NORM_SAMPLES; i++) {
      sum += ADCReadings[i];
    }
    filter.update(sum);
  }
  return filter.average() >> 1;
}

uint16_t getADCVin(uint8_t sample) {
  static history<uint16_t, ADC_FILTER_LEN> filter = {{0}, 0, 0};
  if (sample) {
    uint16_t latestADC = 0;

    // latestADC += adc_inserted_data_read(ADC1, 0);
    // latestADC += adc_inserted_data_read(ADC1, 1);
    // latestADC += adc_inserted_data_read(ADC1, 2);
    // latestADC += adc_inserted_data_read(ADC1, 3);
    latestADC <<= 1;
    filter.update(latestADC);
  }
  return filter.average();
}

// Returns either average or instant value. When sample is set the samples from the injected ADC are copied to the filter and then the raw reading is returned
uint16_t getTipRawTemp(uint8_t sample) {
  static history<uint16_t, ADC_FILTER_LEN> filter = {{0}, 0, 0};
  if (sample) {
    uint16_t latestADC = 0;

    // latestADC += adc_inserted_data_read(ADC0, 0);
    // latestADC += adc_inserted_data_read(ADC0, 1);
    // latestADC += adc_inserted_data_read(ADC0, 2);
    // latestADC += adc_inserted_data_read(ADC0, 3);
    latestADC <<= 1;
    filter.update(latestADC);
    return latestADC;
  }
  return filter.average();
}

void setupFUSBIRQ() {
  // #TODO
}

void vAssertCalled(void) {
  MSG((char *)"vAssertCalled\r\n");

  while (1)
    ;
}

void vPortEnterCritical(void) {}
void vPortExitCritical(void) {}
