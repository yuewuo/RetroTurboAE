#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"
#include "signal.h"

/*
 * I observe that pixels will behave strange no matter what's the frequency, with some pixels flickering
 * Finally I realized that the enable signal is connected to LPTIM2_OUT, which is NOT the frequency of timer interrupt !!!
 * It is at fixed frequency of 32kHz, which means I have to send all data within 0.5 * (1/32kHz) = 15.6us
 * 16 * 8bit in 15.6us, that is 8.2Mbps. I set the SPI bit rate to 2.5Mbps before and I think that's why it behaves strange
 * However it is impossible to redirect LPTIM2 PWM to that port.....
 * 
 * The conclusion is, DO NOT SET SPI speed to lower than 8.2Mbps, for now I use 10Mbps.
 * Be careful next time !!!
 */

TagL4Host_t tag;

void signal_handler(int) {
    tag.tx_running = false;  // this will cause the background thread not being joined, but that's OK
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: <portname> [target_idx(=-1)] [frequency=Hz(=2.)] [multi(=1)]\n");
        printf("    target_idx is -1 by default, which means all LCDs are tested simultaneously\n");
        printf("    this tester will periodically display each pixel, to test whether a 8421x2 module is good\n");
        printf("    I observe that for very small frequency, some pixel behaves strange. Try duplicate those symbols using 'multi'\n");
        printf("    use ctrl-c to stop the program, which is safe\n");
		return -1;
	}

	tag.verbose = true;
	tag.open(argv[1]);
    int target_idx = argc > 2 ? atoi(argv[2]) : -1;
    float frequency = argc > 3 ? atof(argv[3]) : 2.;
    int multi = argc > 4 ? atoi(argv[4]) : 1;

	tag.mem.PIN_EN9 = 1;
	tag.mem.PIN_PWSEL =  1;
	softio_blocking(write, tag.sio, tag.mem.PIN_EN9);
	softio_blocking(write, tag.sio, tag.mem.PIN_PWSEL);

    Tag_Sample_t zero; memset(&zero.s, 0x00, sizeof(Tag_Sample_t));
    tag.set_tx_default_sample(zero);  // set default sample
	vector<Tag_Sample_t> samples; samples.resize(11);  // FF 00 01 02 04 08 10 20 40 80 00
    if (target_idx >= 0) {
        samples[0].le(target_idx) = 0xFF;
        samples[1].le(target_idx) = 0x00;
        samples[2].le(target_idx) = 0x01;
        samples[3].le(target_idx) = 0x02;
        samples[4].le(target_idx) = 0x04;
        samples[5].le(target_idx) = 0x08;
        samples[6].le(target_idx) = 0x10;
        samples[7].le(target_idx) = 0x20;
        samples[8].le(target_idx) = 0x40;
        samples[9].le(target_idx) = 0x80;
        samples[10].le(target_idx) = 0x00;
    } else {
	    memset(&samples[0].s, 0xFF, sizeof(Tag_Sample_t));
	    memset(&samples[1].s, 0x00, sizeof(Tag_Sample_t));
	    memset(&samples[2].s, 0x01, sizeof(Tag_Sample_t));
	    memset(&samples[3].s, 0x02, sizeof(Tag_Sample_t));
	    memset(&samples[4].s, 0x04, sizeof(Tag_Sample_t));
	    memset(&samples[5].s, 0x08, sizeof(Tag_Sample_t));
	    memset(&samples[6].s, 0x10, sizeof(Tag_Sample_t));
	    memset(&samples[7].s, 0x20, sizeof(Tag_Sample_t));
	    memset(&samples[8].s, 0x40, sizeof(Tag_Sample_t));
	    memset(&samples[9].s, 0x80, sizeof(Tag_Sample_t));
	    memset(&samples[10].s, 0x00, sizeof(Tag_Sample_t));
    }

    int idx = 0;
	float frequency_real = tag.tx_send_samples_start(frequency * multi, UINT32_MAX, [&](Tag_Sample_t* buf, size_t len) {
        for (int i=0; i<(int)len; ++i) {
            buf[i] = samples[idx / multi];
            idx = (idx + 1) % (samples.size() * multi);
        }
    });
    printf("frequency_real: %f Hz\n", frequency_real);

    signal(SIGINT, signal_handler);

    while (tag.tx_running) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    tag.lock.lock();
    tag.mem.tx_count = 0;
    softio_blocking(write, tag.sio, tag.mem.tx_count);
    tag.lock.unlock();
    tag.tx_thread.join();

	tag.close();
}
