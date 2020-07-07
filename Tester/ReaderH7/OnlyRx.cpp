#define ReaderH7Host_DEFINATION
#define ReaderH7Host_IMPLEMENTATION
#include "reader-H7xx-ex.h"

ReaderH7Host_t reader;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: ./ReaderH7SoftIO <portname>\n");
		return -1;
	}

	reader.verbose = true;
	reader.open(argv[1]);
	reader.start_rx_receiving();
	vector<filter_out_t> ret = su_time_ret(reader.wait_rx_receiving, 10 * 56875);
	printf("receive %d samples\n", (int)ret.size());
	reader.close();
}
