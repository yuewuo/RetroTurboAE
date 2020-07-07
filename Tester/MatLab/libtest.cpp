#include "rtm.h"
#include <Eigen/Dense>
#include <stdio.h>
#include <cstring>
#include <thread>
#include <vector>
#include <chrono>
#include <fstream>
#include <complex>
using namespace std;
#define LOGM(...) fprintf(stderr, __VA_ARGS__)
using namespace std::chrono_literals;
#include <iostream>
#include <chrono>
#include <fstream>

#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <cassert>
using namespace std;

int main() {
	mod_init(mod_option{.p = 2, .pw = 320, .l = 4, .q = 1}, "refs16x2_8000_12.bin");
	int len = 2;
	uint8_t data[16]{0x74,0xa8}, decoded[16]{};
	//l * p * q
	printf("%d\n", simerr(data, decoded, len, 253, 0, 4));
	for (int i = 0; i < len; ++i) printf("%02x", decoded[i]);
	puts("");
}
int mani(int argc, char** argv) {
	auto a1 = chrono::high_resolution_clock::now();
	enque_calc();
	auto a2 = chrono::high_resolution_clock::now();
	deque_calc();
	auto a3 = chrono::high_resolution_clock::now();
	printf("%f %f\n", chrono::duration<double>(a2 - a1).count(),
				 chrono::duration<double>(a3 - a1).count());
	car_move(0, -100, 0);
	car_adjust_once();
	car_adjust_once();
}
int mian3() {
	int preamble_4psk[] = {0, 1, 3,   2, 2, 2, 2,   1, 1, 1, 1,   2, 2, 2, 2,  0, 0, 0};
	int null[] = {0, 1, 2, 3};
	tag_samp_append(2, 18, preamble_4psk, 32, 32);
#if 1
	init("COM26",  "refn4.ref", nullptr);
	printf("reader gain set to %f\n", gain_reset());
	int a16[] = {0,1,3,2,4,5,7,6,12,13,15,14,8,9,11,10};
	int a256[] = {0,1,3,2,7,6,4,5,15,14,12,13,8,9,11,10,16,17,19,18,23,22,20,21,31,30,28,29,24,25,27,26,48,49,51,50,55,54,52,53,63,62,60,61,56,57,59,58,32,33,35,34,39,38,36,37,47,46,44,45,40,41,43,42,112,113,115,114,119,118,116,117,127,126,124,125,120,121,123,122,96,97,99,98,103,102,100,101,111,110,108,109,104,105,107,106,64,65,67,66,71,70,68,69,79,78,76,77,72,73,75,74,80,81,83,82,87,86,84,85,95,94,92,93,88,89,91,90,240,241,243,242,247,246,244,245,255,254,252,253,248,249,251,250,224,225,227,226,231,230,228,229,239,238,236,237,232,233,235,234,192,193,195,194,199,198,196,197,207,206,204,205,200,201,203,202,208,209,211,210,215,214,212,213,223,222,220,221,216,217,219,218,128,129,131,130,135,134,132,133,143,142,140,141,136,137,139,138,144,145,147,146,151,150,148,149,159,158,156,157,152,153,155,154,176,177,179,178,183,182,180,181,191,190,188,189,184,185,187,186,160,161,163,162,167,166,164,165,175,174,172,173,168,169,171,170
	};
	for (int i = 0; i < 2; ++i) {
		tag_samp_append(4, 16, a16, 800, 800);
	}
	tag_samp_append(2, 18, preamble_4psk, 32, 32);
	vector<complex<float>> rxs(rx_samp2recv());
	printf("reader preamble snr is %f\n", channel(3.15, (float*)rxs.data()));
	auto y = ofstream("rxdata.raw", ofstream::binary);
	y.write((const char*)rxs.data(), rxs.size() * sizeof(complex<float>));
#else
	test_tag();
#endif
	return 0;
}

unsigned rdrand1() {
	unsigned ans;
	asm("rdrand %0":"=r"(ans));
	return ans;
}
