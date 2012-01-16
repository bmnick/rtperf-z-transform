#include <stdlib.h>
#include <stdio.h>

#include <sys/neutrino.h>

#define IO_PORT_SIZE 0xF
#define CTRL_ADDRESS 0x280

#define DA_LSB 6
#define DA_MSB 7

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

	for (i = 0; i < 1000; i++) {
		y_next = y_cur - .632 * y_prev + .368 * u_cur + .264 * u_prev;

		fprintf(f, "%f, %f, %f, %f, %f\n", y_next, y_cur, y_prev, u_cur, u_prev);
		printf("%f, %f, %f, %f, %f\n", y_next, y_cur, y_prev, u_cur, u_prev);

		y_prev = y_cur;
		y_cur = y_next;
		u_prev= u_cur;
		u_cur = 5;

		usleep(100000);
	}

	return EXIT_SUCCESS;
}

void dac_output(double value, uintptr_t handle) {
	int output = value / 7 * 2048 + 2048;


}

