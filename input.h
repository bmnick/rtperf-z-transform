#ifndef __INPUT_H
#define __INPUT_H

#include <semaphore.h>

#include "filter.h"

typedef struct{
	sem_t * write_lock;
	coefs * coefficient_struct;
	double* setpoint;
}input_thread_args;

/**
 * This reads lines in the format "<var> <action> <value>", and action's
 * var by value.
 *
 * valid vars:
 * p - proportional
 * i - integral
 * d - derivative
 * s - setpoint
 *
 * valid actions
 * + - increment
 * - - decrement
 * = - set
 *
 * v_params should be a pointer to an input_thread_params struct
 */
void * start_input_thread(void * v_params);

#endif