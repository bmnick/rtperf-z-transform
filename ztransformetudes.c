#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <semaphore.h>

#include <sys/neutrino.h>
#include <sys/mman.h>
#include <hw/inout.h>

//TODO: Get ATD working periodically (thread/auto) -> dump into shared filter_data
//TODO: Write input and output values to CSV
//TODO: Get CSPS running with step, etc.
//TODO:


#define IO_PORT_SIZE 0xF
#define CTRL_ADDRESS 0x280

#define DA_CMD 0
#define DA_LSB 6
#define DA_MSB 7

#define AD_GAIN 3
#define AD_STATUS 3
#define AD_IRC 4
#define AD_LSB 0
#define AD_MSB 1

#define AD_GAIN_SCAN_DIS (0b00000100) //disable scan
#define AD_GAIN_ZERO (0b00000011) //set gain to 0

#define AD_AINTE_DIS ~(0b1) //disable analog inputs
#define AD_TRIGGER_READ (0x80)
#define AD_STATUS_STS_MASK (0x80)

#define DA_CMD_RSTDA (0x20)

#define DA_LSB_MASK (0x00FF)
#define DA_MSB_MASK (0x0F00)

#define DA_MSB_CHANNEL_0 (0x0000)


/**
 * Ongoing Filter Data struct
 */
typedef struct{ //0 is current time
	double E[3]; //error (input to filter
	double U[3]; //output
} filter_data;

/**
 * Coefficient struct
 */
typedef struct{
	double p,i,d;
	double setpoint;
} coefs; 

/**
 * Data struct required by input thread
 */
typedef struct{
	sem_t * write_lock;
	coefs * coefficient_struct
} input_thread_params;

/**
 * Data struct required by filter thread
 */
typedef struct{
	sem_t * write_lock;
	coefs * coefficient_struct;
	filter_data * data_struct;
	sem_t * input_lock;
	sem_t * output_lock;
} filter_thread_params;

/**
 * Data struct required by output thread
 */
typedef struct {
	sem_t * output_lock;
	filter_data * data_struct;
	uintptr_t * handle;
} output_thread_params;


void dac_output(double value, uintptr_t handle) {
	int output = value / 7 * 2048 + 2048;

	out8(handle + DA_LSB, output & DA_LSB_MASK);
	out8(handle + DA_MSB, ((output & DA_MSB_MASK) >> 8) | DA_MSB_CHANNEL_0);
}

void init_adc(uintptr_t handle) {

	out8(handle+AD_GAIN, ~(AD_GAIN_SCAN_DIS|AD_GAIN_ZERO));

	out8(handle+AD_IRC, in8(handle+AD_IRC) & (AD_AINTE_DIS) );
}

void adc_input(uintptr_t handle, filter_data* data) {
	out8(handle+DA_CMD, AD_TRIGGER_READ);
	while(in8(handle+AD_STATUS) & AD_STATUS_STS_MASK){
		//spin
	}
	int value = in8(handle+AD_MSB)<< 8 | in8(handle+AD_LSB);

}

/**
 * Applies a filter to the specified data
 *
 */
int filter(filter_data * in, coefs * c){
	in->U[0] = in->U[1] 
			 + c->p * ( in->E[0] - in->E[1] )
			 + c->i * in->E[0]
			 + c->d * ( in->E[0] - ( in->E[1] / 2 ) + in->E[2] );

	return in->U[0];
}

/**
 * This reads lines in the format "<var> <action> <value>", and action's
 * var by value.
 * 
 * valid vars:
 * 	p - proportional
 * 	i - integral
 *	d - derivative
 *	s - setpoint
 *
 * valid actions
 *	+ - increment
 *	- - decrement
 * 	= - set
 *
 * v_params should be a pointer to an input_thread_params struct
 */
void * start_input_thread(void * v_params) {
	input_thread_params * params = (input_thread_params *) v_params;

	char var;
	char action;
	double value;

	double * var_to_change;

	while (true) {
		sem_post(params->write_lock);

		scanf("%c %c %f", &var, &action, &value);

		sem_wait(params->write_lock);

		// Get out the variable
		switch(var){
			case 'p': 
				var_to_change = &(params->coefficient_struct->p); 
				break;
			case 'i': 
				var_to_change = &(params->coefficient_struct->i); 
				break;
			case 'd': 
				var_to_change = &(params->coefficient_struct->d); 
				break;
			case 's': 
				var_to_change = &(params->coefficient_struct->setpoint); 
				break;
			default:
				printf("Invalid variable\n");
				continue;
		}

		// Perform the action
		switch(action) {
			case '+':
				*var_to_change += value;
				break;
			case '-':
				*var_to_change -= value;
				break;
			case '=':
				*var_to_change = value;
				break;
			default:
				printf("Invalid action\n");
				continue;
		}
	}
}

/**
 * Thread wrapper function for filter thread, just runs filter with 
 * given parameters
 *
 * v_params should be a pointer to a filter_thread_params struct
 */
void * start_filter_thread(void * v_params) {
	filter_thread_params * params = (filter_thread_params *) v_params;

	while(true) {
		sem_wait(params->input_lock);

		sem_wait(params->write_lock);

		filter(params->data_struct, params->coefficient_struct);

		sem_post(params->write_lock);

		sem_post(params->output_lock);
	}
}

/**
 * Thread wrapper function for output thread, just runs output when
 * semaphore charged.
 *
 * v_params should be a pointer to a output_thread_params struct
 */
void * start_output_thread(void * v_params) {
	output_thread_params * params = (output_thread_params *) v_params;

	while(true) {
		sem_wait(params->output_lock);

		dac_output(params->data_struct->U[0], params->handle);
	}
}


int main(int argc, char *argv[]) {
	FILE * f = fopen("/tmp/data.csv", "w+");

	uintptr_t ctrlHandle;

	double y_next = 0, y_cur = 0, y_prev = 0;
	double u_cur = 5, u_prev = 0;
	int i;

	if (ThreadCtl(_NTO_TCTL_IO, NULL) == -1){
		perror("Failed to get I/O access permission");
		return 1;
	}

	ctrlHandle = mmap_device_io(IO_PORT_SIZE, CTRL_ADDRESS);
	if (ctrlHandle == MAP_DEVICE_FAILED) {
		perror("Failed to map control register");
		return 2;
	}

	out8(ctrlHandle + DA_CMD, DA_CMD_RSTDA);

	for (i = 0; i < 1000; i++) {
		y_next = y_cur - .632 * y_prev + .368 * u_cur + .264 * u_prev;

		fprintf(f, "%f, %f, %f, %f, %f\n", y_next, y_cur, y_prev, u_cur, u_prev);
		printf("%f, %f, %f, %f, %f\n", y_next, y_cur, y_prev, u_cur, u_prev);

		y_prev = y_cur;
		y_cur = y_next;
		u_prev= u_cur;
		u_cur = 5;

		dac_output(y_next, ctrlHandle);

		usleep(100000);
	}

	return EXIT_SUCCESS;
}

