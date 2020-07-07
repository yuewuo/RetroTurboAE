#define ReaderH7Host_DEFINATION
#define ReaderH7Host_IMPLEMENTATION
#define PREAMBLE_IMPLEMENTATION
#include "reader-H7xx-ex.h"

ReaderH7Host_t reader;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: ./ReaderH7SoftIO <portname>\n");
		return -1;
	}

	reader.load_refpreamble("ref.refraw");
	reader.open(argv[1]);
	// reader.gain_ctrl(0.2);
	printf("AGC done\n");
	reader.start_preamble_receiving(100, 10.0);
	printf("reader is listening to preamble\n");
	int ret = reader.wait_preamble_for(5);  // wait for 5s
	reader.stop_preamble_receiving();
	printf("reader complete with snr = %f, ret = %d\n", reader.snr_preamble, ret);
	reader.close();

}
