#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"

TagL4Host_t tag;

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: <portname> <duration:s(=5.)> <frequency:Hz(=1.)>\n");
		return -1;
	}

	tag.verbose = true;
	tag.open(argv[1]);
	float duration = argc > 2 ? atof(argv[2]) : 5.;
	float frequency = argc > 3 ? atof(argv[3]) : 1.;
	int count = duration * frequency;
	if (count < 1) count = 1;

	vector<Tag_Sample_t> samples;
	Tag_Sample_t one, zero;
	tag.set_tx_default_sample(zero);  // set default sample
	memset(&one.s, 0xFF, sizeof(Tag_Sample_t));
	memset(&zero.s, 0x00, sizeof(Tag_Sample_t));

	for (int i=0; i<count; ++i) {
		samples.push_back(one);
		samples.push_back(zero);
	}
	tag.tx_send_samples(frequency * 2, samples);  // sample rate is twice frequency

	tag.close();
}
