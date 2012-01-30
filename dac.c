#include "dac.h"

//#include "hw/inout.h"

/**
 * DAC Output
 */
void dac_output(double value, uintptr_t handle, FILE * f) {
	if (value > 6.5){
		value=6.5;
	}
	if (value < -6.5){
		value=-6.5;
	}

	int output = value / 7 * 2048 + 2048;

	fprintf(f,"%lf \r\n",value);

	out8(handle + DA_LSB, output & DA_LSB_MASK);
	out8(handle + DA_MSB, ((output & DA_MSB_MASK) >> 8) | DA_MSB_CHANNEL_0);
}

void init_da(uintptr_t ctrlHandle) {
	//reset DA converter
	out8(ctrlHandle + DA_CMD, DA_CMD_RSTDA);
}

/**
 * DAC thread
 */
void* DAC_thread(void* param) {
	dac_args* args= (dac_args*) param;

	while(1==1){
		sem_wait(args->DAC_sem);
		dac_output(args->data->U[0], args->handle, args->f);

	}

	return 0;
}