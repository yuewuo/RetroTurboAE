#define OMNIWHEELCAR_IMPLEMENTATION
#include "OmniWheelCar.h"
#include "sysutil.h"

OmniWheelCar car;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv);

	if (argc != 5) {
		printf("usage: ./test <portname> <x/mm> <y/mm> <r/deg>\n");
		return -1;
	}

	car.open(argv[1]);
	float x = atof(argv[2]);
	float y = atof(argv[3]);
	float r = atof(argv[4]);

	car.setMaxSpeed(200);
	car.resume();
	car.move_abs(x, y, r);
	// ldscar.stop();
	car.close();
}
