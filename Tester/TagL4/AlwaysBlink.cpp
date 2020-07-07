#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"

TagL4Host_t tag;

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: <portname> <frequency:Hz(=1.)>\n");
		return -1;
	}

	tag.verbose = true;
	tag.open(argv[1]);
	float frequency = argc > 2 ? atof(argv[2]) : 1.;

    float frequency_real = tag.set_tx_sample_rate(frequency * 8) / 8;  // set sample rate
    printf("frequency_real: %f Hz\n", frequency_real);

    tag.mem.NLCD = 1;  // single byte
	softio_blocking(write, tag.sio, tag.mem.NLCD);

    softio_blocking(reset_fifo, tag.sio, tag.mem.tx_data);
    fifo_enque(&tag.mem.tx_data, 1);
    fifo_enque(&tag.mem.tx_data, 1);
    fifo_enque(&tag.mem.tx_data, 1);
    fifo_enque(&tag.mem.tx_data, 1);
    fifo_enque(&tag.mem.tx_data, 0);
    fifo_enque(&tag.mem.tx_data, 0);
    // fifo_enque(&tag.mem.tx_data, 0);  // interval will consume 2 samples
    // fifo_enque(&tag.mem.tx_data, 0);
    softio_blocking(write_fifo, tag.sio, tag.mem.tx_data);

	tag.mem.repeat_count = -1;  // always repeat start
	softio_blocking(write, tag.sio, tag.mem.repeat_count);

	tag.close();
}
