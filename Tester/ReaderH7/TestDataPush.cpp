#define ReaderH7Host_DEFINATION
#define ReaderH7Host_IMPLEMENTATION
#define TEST_MCU_STREAM_VERBOSE_SPEED  // to enable throughput printing every 1s
#include "reader-H7xx-ex.h"

ReaderH7Host_t reader;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: ./ReaderH7SoftIO <portname>\n");
		return -1;
	}

	reader.verbose = true;
	reader.open(argv[1]);
	printf("len: %d\n", fifo_count(&reader.mem.adc_data));
	reader.start_rx_receiving();
	// reader.dump(0);  // dump only basic
    this_thread::sleep_for(5000ms);  // 5s
	printf("call stop\n");
    reader.stop_rx_receiving();
	printf("len: %d\n", fifo_count(&reader.mem.adc_data));
	reader.close();
}
