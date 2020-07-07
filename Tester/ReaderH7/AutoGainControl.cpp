#define ReaderH7Host_DEFINATION
#define ReaderH7Host_IMPLEMENTATION
// #define PREAMBLE_IMPLEMENTATION
#include "reader-H7xx-ex.h"

ReaderH7Host_t reader;

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("usage: <portname> <voltage level (0~2.0) V>\n");
		return -1;
	}

	reader.open(argv[1]);
	float volt = atof(argv[2]);
	float real_volt = reader.gain_ctrl(volt);
	printf("AGC done end with %f\n", real_volt);
	reader.close();

}
