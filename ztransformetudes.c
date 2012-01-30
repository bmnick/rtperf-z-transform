#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <pthread.h>
#include <semaphore.h>

#include <sys/neutrino.h>
#include <sys/mman.h>

#include "atd.h"
#include "dac.h"
#include "filter.h"
#include "input.h"

#define IO_PORT_SIZE 0xF
#define CTRL_ADDRESS 0x280

int main(int argc, char *argv[]) {
	int freq;
	double setpoint;
	uintptr_t ctrlHandle;
	char buf[20], file[10];
	coefs coefficients;
	filter_data data={
		{0,0,0},
		{0,0,0}
	};

	// Read initial configuration from the user
	printf("log file? (9 chars max)\r\n");
	scanf("%s", file);
	printf("p:\r\n");
	scanf("%lf", &coefficients.p);
	printf("i:\r\n");
	scanf("%lf", &coefficients.i);
	printf("d:\r\n");
	scanf("%lf", &coefficients.d);
	printf("setpoint:\r\n");
	scanf("%lf", &setpoint);
	printf("Sample freq?\r\n");
	scanf("%d",&freq);

	// Open the log file
	sprintf(buf,"/tmp/%s.csv",file);

	FILE * f = fopen(buf, "w+");

	// Increase permissions for access to hardware
	if (ThreadCtl(_NTO_TCTL_IO, NULL) == -1){
		perror("Failed to get I/O access permission");
		return 1;
	}

	// Get a handle to write to hardware ATD and DAC
	ctrlHandle = mmap_device_io(IO_PORT_SIZE, CTRL_ADDRESS);
	if (ctrlHandle == MAP_DEVICE_FAILED) {
		perror("Failed to map control register");
		return 2;
	}

	// Create and initialize semaphores
	sem_t filter_sem, DAC_sem, write_lock;

	sem_init(&filter_sem,0,0);
	sem_init(&DAC_sem,0,0);
	sem_init(&write_lock,0,0);

	//set up args for our threads
	adc_args adcargs = {
		ctrlHandle, 	// uintptr_t handle
		&data, 			// filter_data * data
		&filter_sem, 	// sem_t * filter_sem
		&DAC_sem, 		// sem_t * DAC_sem
		&setpoint, 		// double * setpoint
		f 				// FILE * f
	};
	
	filter_args filtargs = {
		&data, 			// filter_data * data
		&coefficients, 	// coefs * coefficients
		&filter_sem, 	// sem_t * filter_sem
		&write_lock, 	// sem_t * write_lock
		&DAC_sem, 		// sem_t * DAC_sem
		f 				// FILE * f
	};
	
	dac_args dacargs = {
		&data, 			// filter_data * data
		ctrlHandle, 	// uintptr_t handle
		&DAC_sem, 		// sem_t DAC_sem
		f 				// FILE * f
	};

	input_thread_args inputargs = {
		&write_lock, 	// sem_t * write_lock
		&coefficients, 	// coefs * coefficient_struct
		&setpoint 		// double * setpoint
	};

	// Initialize hardware
	init_da(ctrlHandle);

	// Start the system
	scheduleATD(1000000000/freq,ctrlHandle,&adcargs);

	pthread_t filter_pthread, DAC_pthread, input_pthread;

	pthread_create(&filter_pthread,NULL,&filter_thread,(void*)&filtargs);
	pthread_create(&DAC_pthread,NULL,&DAC_thread,(void*)&dacargs);
	pthread_create(&input_pthread,NULL, &start_input_thread, (void*)&inputargs);

	// Wait for the threads to finish (won't happen, run forever)
	pthread_join(filter_pthread,NULL);
	pthread_join(DAC_pthread,NULL);
	pthread_join(input_pthread,NULL);

	return EXIT_SUCCESS;
}

