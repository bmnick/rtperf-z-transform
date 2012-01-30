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

//TODO: get rid of this
FILE * f;

int main(int argc, char *argv[]) {

	char buf[20];
	char file[10];
	printf("log file?\r\n");
	scanf("%s", file);
	sprintf(buf,"/tmp/%s.csv",file);

	f= fopen(buf, "w+");

	uintptr_t ctrlHandle;

	filter_data data={{0,0,0},{0,0,0}};
	coefs coefficients={2,.3,.1};
	double setpoint=0;

	printf("p:\r\n");
	scanf("%lf", &coefficients.p);
	printf("i:\r\n");
	scanf("%lf", &coefficients.i);
	printf("d:\r\n");
	scanf("%lf", &coefficients.d);
	printf("setpoint:\r\n");
	scanf("%lf", &setpoint);


	sem_t filter_sem;
	sem_t DAC_sem;
	sem_t write_lock;

	pthread_t filter_pthread;
	pthread_t DAC_pthread;
	pthread_t input_pthread;

	sem_init(&filter_sem,0,0);
	sem_init(&DAC_sem,0,0);
	sem_init(&write_lock,0,0);

	if (ThreadCtl(_NTO_TCTL_IO, NULL) == -1){
		perror("Failed to get I/O access permission");
		return 1;
	}

	ctrlHandle = mmap_device_io(IO_PORT_SIZE, CTRL_ADDRESS);
	if (ctrlHandle == MAP_DEVICE_FAILED) {
		perror("Failed to map control register");
		return 2;
	}

	//set up args for our ADC callback thread
	adc_args adcargs={ctrlHandle, &data, &filter_sem, &DAC_sem, &setpoint};
	filter_args filtargs={&data, &coefficients, &filter_sem, &write_lock, &DAC_sem};
	dac_args dacargs={&data,ctrlHandle,&DAC_sem,f};
	input_thread_args inputargs={&write_lock, &coefficients,&setpoint};

	init_da(ctrlHandle);

	printf("Sample freq?\r\n");
	int freq;
	scanf("%d",&freq);

	scheduleATD(1000000000/freq,ctrlHandle,&adcargs);

	pthread_create(&filter_pthread,NULL,&filter_thread,(void*)&filtargs);
	pthread_create(&DAC_pthread,NULL,&DAC_thread,(void*)&dacargs);
	pthread_create(&input_pthread,NULL, &start_input_thread, (void*)&inputargs);

	pthread_join(filter_pthread,NULL);
	pthread_join(DAC_pthread,NULL);
	pthread_join(input_pthread,NULL);

	return EXIT_SUCCESS;
}

