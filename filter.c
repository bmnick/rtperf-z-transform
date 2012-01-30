#include "filter.h"

/**
 * Applies a filter to the specified data
 *
 */
double filter(filter_data* in, coefs* c){

	in->U[2]=in->U[1];
	in->U[1]=in->U[0];
	in->U[0]=in->U[1]+
			c->p*(in->E[0]-in->E[1])
			+ c->i*in->E[0]
			+ c->d*(in->E[0]-(in->E[1]/2)+in->E[2]);

	return in->U[0];
}

/**
 * Filter thread
 *
 */
void* filter_thread(void* param){

	filter_args* args = (filter_args*) param;
	while(1==1){
		sem_wait(args->filter_sem);//wait until the ADC is done
		sem_wait(args->write_lock);


		filter(args->data, args->coeficients);
		fprintf(args->f, " %f ,", args->data->U[0]);
		sem_post(args->write_lock);
		sem_post(args->DAC_sem);
	}

	return 0;
}