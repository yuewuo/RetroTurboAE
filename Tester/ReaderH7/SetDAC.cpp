#define ReaderH7Host_DEFINATION
#define ReaderH7Host_IMPLEMENTATION
#include "reader-H7xx-ex.h"

ReaderH7Host_t reader;

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("usage: ./ReaderH7SoftIO <portname> <dac_set:uint16>\n");
		return -1;
	}

	reader.verbose = true;
	int val = atoi(argv[2]);
	reader.open(argv[1]);
	reader.set_dac_delay(val);
	printf("dac value set: %d\n", val);
	reader.close();
}
