#define OMNIWHEELCAR_IMPLEMENTATION
#include "OmniWheelCar.h"
#include "sysutil.h"

OmniWheelCar car;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv);

	if (argc != 2) {
		printf("usage: <portname>\n");
		return -1;
	}

	car.open(argv[1]);
    car.pause();
	car.close();
}
