#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"

TagL4Host_t tag;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: <portname>\n");
		return -1;
	}

	tag.verbose = true;
	tag.open(argv[1]);

	// use this for continuous streaming
	// tag.tx_send_samples_start(4e3);
	// this_thread::sleep_for(chrono::seconds(3));
	// tag.tx_send_samples_stop();

	// use this for burst transmit
	vector<Tag_Sample_t> samples;
	Tag_Sample_t one, zero;
	tag.set_tx_default_sample(zero);  // set default sample

	memset(&one.s, 0xFF, sizeof(Tag_Sample_t));
	memset(&zero.s, 0x00, sizeof(Tag_Sample_t));
	for (int i=0; i<2; ++i) {
		for (int j=0; j<8000; ++j) samples.push_back(one);
		for (int j=0; j<8000; ++j) samples.push_back(zero);
	}
	tag.tx_send_samples(8e3, samples);

	tag.close();
}
