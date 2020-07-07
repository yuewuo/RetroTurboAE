#define ReaderH7Host_DEFINATION
#define ReaderH7Host_IMPLEMENTATION
#include "reader-H7xx-ex.h"

ReaderH7Host_t reader;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: ./RawTx <portname>\n");
		return -1;
	}

	reader.open(argv[1]);

	// setup frequency
	pair<float, float> actual;
	actual = reader.set_lptim1(455e3, 0.5);
	printf("lptim1 set to %f kHz, %f%% duty cycle\n", actual.first/1000, actual.second*100);
	actual = reader.set_lptim2(10e3, 0.5);
	printf("lptim2 set to %f kHz, %f%% duty cycle\n", actual.first/1000, actual.second*100);
	actual = reader.set_tim4(1100e3, 0.5);
	printf("timer4 set to %f kHz, %f%% duty cycle\n", actual.first/1000, actual.second*100);

	// setup output sample
	Tx_Sample_t default_sample, zero, one;
	default_sample.use_tim4_not_lptim1 = 0;
	default_sample.select_lptim1tim4_not_lptim2 = 1;  // default: lptim1
	reader.set_tx_default_sample(default_sample);
	zero.select_lptim1tim4_not_lptim2 = 0;  // zero: lptim2
	one.use_tim4_not_lptim1 = 1;
	one.select_lptim1tim4_not_lptim2 = 1;  // one: tim4

	// set sample rate
	float sample_rate = reader.set_tx_sample_rate(9600);
	printf("sample_rate is %f Hz\n", sample_rate);

	// build up samples
	vector<Tx_Sample_t> samples;
	for (int i=0; i<1000; ++i) {
		samples.push_back(zero);
		samples.push_back(one);
	}

	// blocking API to send samples
	su_time(reader.tx_send_samples, samples);
	printf("blocking send done\n");

	// non-blocking API to send
	su_time(reader.tx_send_samples_async, samples);
	printf("async send done\n");
	// reader.tx_send_samples_wait();  // close will automatically call this

	reader.close();
}
