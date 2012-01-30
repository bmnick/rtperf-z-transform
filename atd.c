#include "atd.h"

void out8( uintptr_t port, 
           uint8_t val );
uint8_t in8( uintptr_t port );

void init_adc(uintptr_t handle) {

	out8(handle+AD_GAIN, ~(AD_GAIN_SCAN_DIS|AD_GAIN_ZERO));
	out8(handle+AD_IRC, in8(handle+AD_IRC) & (AD_IRC_AINTE_DIS) );

}

double adc_input(uintptr_t handle, filter_data* data) {
	out8(handle+AD_CMD, AD_CMD_TRIGGER_READ);
	while(in8(handle+AD_STATUS) & AD_STATUS_STS_MASK){
		//spin
	}
	signed short value = (in8(handle+AD_MSB)<< 8) + in8(handle+AD_LSB); //signed 16 bit value

	return (double)value/(32767.0)*10.0;//convert to double (voltage)
}

void adc_callback(void * param){

	adc_args* args=(adc_args*) param;
	double input= adc_input(args->handle, args->data);

	args->data->E[2]=args->data->E[1];
	args->data->E[1]=args->data->E[0];
	args->data->E[0]=*(args->setpoint)-input;

	fprintf(args->f,"%f ,", input);
	fprintf(args->f,"%f ,", args->data->E[0]);

	//free up the filter semaphore to allow the filter to run to run
	sem_post(args->filter_sem);
}

/**
 * Schedules a periodic ATD conversion
 *
 * @param interval nanosecond interval
 *
 */
int scheduleATD(int interval, uintptr_t handle, adc_args* args){
	struct sigevent event;
	timer_t timer; //out value for timer create
	struct itimerspec value;


	SIGEV_THREAD_INIT(&event, &adc_callback,(void*)args, NULL );
	timer_create(CLOCK_REALTIME, &event, &timer );

	//set initial time
	value.it_value.tv_nsec = interval;
	value.it_value.tv_sec = 0;

	//set reload time
	value.it_interval.tv_nsec=interval;
	value.it_interval.tv_sec=0;

	//set up the timer and let it run
	timer_settime(timer,0,&value,NULL);

	return 0;

}
