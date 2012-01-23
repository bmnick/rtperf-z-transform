#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/neutrino.h>
#include <sys/mman.h>
#include <hw/inout.h>

//TODO: Get ATD working periodically (thread/auto) -> dump into shared filterData
//TODO: Make thread +semaphore for filter
//TODO: Make thread +semaphore for output
//TODO: Make user input thread (Setpoint, P, I, D)
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

void dac_output(double value, uintptr_t handle) {
	int output = value / 7 * 2048 + 2048;

	out8(handle + DA_LSB, output & DA_LSB_MASK);
	out8(handle + DA_MSB, ((output & DA_MSB_MASK) >> 8) | DA_MSB_CHANNEL_0);
}

void init_adc(uintptr_t handle) {

	out8(handle+AD_GAIN, ~(AD_GAIN_SCAN_DIS|AD_GAIN_ZERO));

	out8(handle+AD_IRC, in8(handle+AD_IRC) & (AD_AINTE_DIS) );
}

void adc_input(uintptr_t handle, filterData* data) {
	out8(handle+DA_CMD, AD_TRIGGER_READ);
	while(in8(handle+AD_STATUS) & AD_STATUS_STS_MASK){
		//spin
	}
	int value = in8(handle+AD_MSB)<< 8 | in8(handle+AD_LSB);

}


typedef struct{ //0 is current time
	double E[3]; //error (input to filter
	double U[3]; //output
}filterData;

typedef struct{
	double p,i,d;
	double setpoint;
}coefs;

/**
 * Applies a filter to the specified data
 *
 */
int filter(filterData in, coefs c){
	in.U[0]=in.U[1]+
			c.p*(in.E[0]-in.E[1])
			+ c.i*in.E[0]
			+ c.d*(in.E[0]-(in.E[1]/2)+in.E[2]);

	return in.U[0];
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

