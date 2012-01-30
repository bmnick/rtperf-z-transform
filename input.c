#include "input.h"
#include <stdio.h>

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