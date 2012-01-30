#ifndef __DAC_H
#define __DAC_H

#include <stdint.h>
#include <semaphore.h>

#include "filter.h"

#define DA_CMD 0

#define DA_CMD_RSTDA (0x20)

#define DA_LSB 6

#define DA_LSB_MASK (0x00FF)

#define DA_MSB 7

#define DA_MSB_MASK (0x0F00)
#define DA_MSB_CHANNEL_0 (0x0000)

/**
 * DAC Arguments struct
 *
 * Contains all arguments needed by the DAC output thread on launch
 */
typedef struct{
	filter_data * data;

	uintptr_t handle;

	sem_t * DAC_sem;

	FILE * f;
} dac_args;

/**
 * DAC Init
 */
void init_da(uintptr_t ctrlHandle);

/**
 * DAC Output
 * 
 * Helper method for writing a double value to DAC.
 */
void dac_output(double value, uintptr_t handle,  FILE * f);

/**
 * DAC Thread
 *
 * Driver method for output thread to the DAC.
 */
void * DAC_thread (void * param);

#endif