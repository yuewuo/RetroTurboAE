#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"

TagL4Host_t tag;

int main(int argc, char** argv) {
	if (argc != 4) {
		printf("usage: <portname> <0|1(EN9)> <0|1(PWSEL)>\n");
		return -1;
	}
	assert(argv[2][0] == '0' || argv[2][0] == '1');
	assert(argv[3][0] == '0' || argv[3][0] == '1');

	tag.verbose = true;
	tag.open(argv[1]);

	tag.mem.PIN_EN9 = argv[2][0] == '0' ? 0 : 1;
	tag.mem.PIN_PWSEL = argv[3][0] == '0' ? 0 : 1;
	softio_blocking(write, tag.sio, tag.mem.PIN_EN9);
	softio_blocking(write, tag.sio, tag.mem.PIN_PWSEL);

	tag.close();
}
