#ifndef __FILTER_H
#define __FILTER_H

#include <stdio.h>

#include <semaphore.h>

typedef struct filter_data { //0 is current time
	double E[3]; //error (input to filter
	double U[3]; //output
} filter_data; 

typedef struct coefs {
	double p,i,d;
} coefs;

typedef struct filter_args {
	filter_data* data;
	coefs* coeficients;
	sem_t* filter_sem;
	sem_t* write_lock;
	sem_t* DAC_sem;
	FILE * f;
} filter_args;

double filter(filter_data * in, coefs * c);

void * filter_thread(void * param);

#endif