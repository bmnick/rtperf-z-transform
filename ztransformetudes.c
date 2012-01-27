#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <time.h>
#include <sys/siginfo.h>
#include <pthread.h>
#include <semaphore.h>


#include <sys/neutrino.h>
#include <sys/mman.h>
#include <hw/inout.h>


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



//TODO: get rid of this
FILE * f;
int count = 0;



typedef struct{ //0 is current time
	double E[3]; //error (input to filter
	double U[3]; //output
}filter_data;

typedef struct{
	double p,i,d;
}coefs;

typedef struct{
	uintptr_t handle;
	filter_data* data;
	sem_t* filter_sem;
	sem_t* DAC_sem;
	double* setpoint;

}adc_args;

typedef struct{
	sem_t * write_lock;
	coefs * coefficient_struct;
	double* setpoint;
}input_thread_args;

typedef struct{
	filter_data* data;
	coefs* coeficients;
	sem_t* filter_sem;
	sem_t* write_lock;
	sem_t* DAC_sem;
}filter_args;

typedef struct{
	filter_data* data;
	uintptr_t handle;
	sem_t* DAC_sem;
}dac_args;


void dac_output(double value, uintptr_t handle) {
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

void init_adc(uintptr_t handle) {

	out8(handle+AD_GAIN, ~(AD_GAIN_SCAN_DIS|AD_GAIN_ZERO));
	out8(handle+AD_IRC, in8(handle+AD_IRC) & (AD_AINTE_DIS) );

}




double adc_input(uintptr_t handle, filter_data* data) {
	out8(handle+DA_CMD, AD_TRIGGER_READ);
	while(in8(handle+AD_STATUS) & AD_STATUS_STS_MASK){
		//spin
	}
	signed short value = (in8(handle+AD_MSB)<< 8) + in8(handle+AD_LSB); //signed 16 bit value

	return (double)value/(32767.0)*10.0;//convert to double (voltage)
}

/**
 * ADC callback. Function called periodically to scan the ADC and trigger a run of the filter
 *
 */
void adc_callback(param){

	adc_args* args=(adc_args*) param;
	double input= adc_input(args->handle, args->data);

	args->data->E[2]=args->data->E[1];
	args->data->E[1]=args->data->E[0];
	args->data->E[0]=*(args->setpoint)-input;

	fprintf(f,"%f ,", input);
	fprintf(f,"%f ,", args->data->E[0]);

	//free up the filter semaphore to allow the filter to run to run
	sem_post(args->filter_sem);

	count++;
	if (count==150){
		printf("done\r\n");
	}



}





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
		fprintf(f, " %f ,", args->data->U[0]);
		sem_post(args->write_lock);
		sem_post(args->DAC_sem);
	}

	return 0;
}

/**
 * DAC thread
 *
 */
void* DAC_thread(void* param){
	dac_args* args= (dac_args*) param;

	while(1==1){
		sem_wait(args->DAC_sem);
		dac_output(args->data->U[0], args->handle);

	}

	return 0;
}

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
void * start_input_thread(void * v_params) {
	input_thread_args * params = (input_thread_args *) v_params;

	char var;
	char action;
	double value;

	double * var_to_change;

	while (1) {
		sem_post(params->write_lock);

		printf(":");
		var = getchar();
		action = getchar();
		scanf("%lf", &value);
		while (getchar() != '\n'){}

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
			var_to_change = params->setpoint;
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

		printf("%c:%lf\r\n", var,*var_to_change);
	}
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
	dac_args dacargs={&data,ctrlHandle,&DAC_sem};
	input_thread_args inputargs={&write_lock, &coefficients,&setpoint};

	//reset DA converter
	out8(ctrlHandle + DA_CMD, DA_CMD_RSTDA);

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

