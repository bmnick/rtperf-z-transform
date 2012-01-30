#ifndef __ATD_H
#define __ATD_H

#include <stdint.h>
#include <time.h>

#include <sys/siginfo.h>
#include <hw/inout.h>

#include "filter.h"

#define AD_GAIN 3

#define AD_GAIN_SCAN_DIS (0b00000100) //disable scan
#define AD_GAIN_ZERO (0b00000011) //set gain to 0


#define AD_STATUS 3

#define AD_STATUS_STS_MASK (0x80)


#define AD_IRC 4
#define AD_IRC_AINTE_DIS ~(0b1) //disable analog inputs

#define AD_LSB 0
#define AD_MSB 1

#define AD_CMD 0
#define AD_CMD_TRIGGER_READ (0x80)

typedef struct adc_args {
	uintptr_t handle;

	filter_data* data;

	sem_t* filter_sem;

	sem_t* DAC_sem;

	double* setpoint;

	FILE * f;
} adc_args;

void init_adc(uintptr_t handle);

double adc_input(uintptr_t handle, filter_data* data);

/**
 * ADC callback. Function called periodically to scan the ADC and trigger a run of the filter
 *
 */
void adc_callback(void * param);

/**
 * Schedules a periodic ATD conversion
 *
 * @param interval nanosecond interval
 *
 */
int scheduleATD(int interval, uintptr_t handle, adc_args * args);

#endif