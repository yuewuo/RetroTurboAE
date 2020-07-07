#include <vector>
#include <map>
#include "stdlib.h"
#include "stdio.h"
#include "assert.h"
#include "math.h"
using namespace std;

vector<float> union_curve_parse(const vector<char>& binary, double sample_rate, double frequency, int target_length) {
	typedef struct { int16_t s[4]; } uni_out_t;
	const uni_out_t* buffer2 = (const uni_out_t*)binary.data();
	int length = binary.size() / sizeof(uni_out_t);
	// evaluate noise ratio and all-zero level
	int zero_start = 0;  // to avoid some strange points at begining
	int zero_padding = 0.020 * sample_rate;
	int zero_end = zero_start + zero_padding;
	assert(length >= zero_padding && "too short");
	printf("evaluate noise ratio and all-zero level from %d (%f ms) to %d (%f ms)\n", zero_start, (double)zero_start / sample_rate * 1000, zero_end, zero_end / sample_rate * 1000);
	double zero_avr[4] = {0};
	for (int i=zero_start; i<zero_end; ++i) for (int j=0; j<4; ++j) zero_avr[j] += buffer2[i].s[j];
	for (int j=0; j<4; ++j) zero_avr[j] /= zero_padding;
	printf("zero_avr: %f %f %f %f\n", zero_avr[0], zero_avr[1], zero_avr[2], zero_avr[3]);
	double stddev[4] = {0};
	for (int i=zero_start; i<zero_end; ++i) for (int j=0; j<4; ++j) stddev[j] += pow(buffer2[i].s[j] - zero_avr[j], 2);
	for (int j=0; j<4; ++j) stddev[j] = sqrt(stddev[j] / zero_padding);
	printf("stddev: %f %f %f %f\n", stddev[0], stddev[1], stddev[2], stddev[3]);
	// next find the rough fast edge start point where delta is larger than 100x stddev
	int rough_start = 0;
	double dev_th = 0;
	for (int j=0; j<4; ++j) dev_th += abs(stddev[j]);
	dev_th *= 10;
	if (dev_th > 300) dev_th = 300;
	printf("dev_th: %f\n", dev_th);
	for (int i=zero_start + zero_padding; i<length; ++i) {
		double dev = 0;
		for (int j=0; j<4; ++j) dev += abs(buffer2[i].s[j] - zero_avr[j]);
		if (dev > dev_th) { rough_start = i; break; }
	}
	assert(rough_start != 0 && "cannot find rough start of first fast edge");
	printf("rough_start: %d (%f ms)\n", rough_start, rough_start / sample_rate * 1000);
	// evaluate all-one level and noise ratio
	int one_padding = 16 * sample_rate / frequency - 0.003 * sample_rate;
	int one_start = rough_start + 0.002 * sample_rate;  // about 2ms after rough_start
	assert(length >= one_start + one_padding && "too short");
	printf("evaluate all-one level from %d (%f ms) to %d (%f ms)\n", one_start, one_start / sample_rate * 1000, one_start + one_padding, (one_start + one_padding) / sample_rate * 1000);
	double one_avr[4] = {0};
	for (int i=0; i<one_padding; ++i) for (int j=0; j<4; ++j) one_avr[j] += buffer2[one_start+i].s[j];
	for (int j=0; j<4; ++j) one_avr[j] /= one_padding;
	printf("one_avr: %f %f %f %f\n", one_avr[0], one_avr[1], one_avr[2], one_avr[3]);
	// change the four curve into one curve
	vector<float> union_curve; union_curve.resize(length);
	double fenmu = sqrt(pow(one_avr[0]-zero_avr[0], 2) + pow(one_avr[1]-zero_avr[1], 2) + pow(one_avr[2]-zero_avr[2], 2) + pow(one_avr[3]-zero_avr[3], 2));
	for (int i=0; i<length; ++i) {
		double fenzi = sqrt(pow(buffer2[i].s[0]-zero_avr[0], 2) + pow(buffer2[i].s[1]-zero_avr[1], 2) + pow(buffer2[i].s[2]-zero_avr[2], 2) + pow(buffer2[i].s[3]-zero_avr[3], 2));
		union_curve[i] = fenzi / fenmu;
	}
	int middle_start = rough_start - 100;
	double last_middle_delta = 1;
	int o_middle = 0;
	for (int i=0; i<200; ++i) {
		double delta = abs(union_curve[middle_start + i] - 0.5);
		if (delta < 0.25) {  // must be in 1/4 to deprecate noise
			if (delta < last_middle_delta) {
				o_middle = middle_start + i;
				last_middle_delta = delta;
			} else break;
		}
	}
	assert(o_middle && "cannot find middle, strange");
	printf("found accurate middle at %d (%f ms)\n", o_middle, o_middle / sample_rate * 1000);
	// then slice to start point
#define UNION_CURVE_HALF_FAST 0.7
	int new_middle = UNION_CURVE_HALF_FAST * sample_rate / 1000;
	int union_curve_start = o_middle - new_middle;  // 0.7ms
	printf("new middle should at %d (%f ms)\n", new_middle, new_middle / sample_rate * 1000);
	union_curve.erase(union_curve.begin(), union_curve.begin() + union_curve_start);
	assert((int)union_curve.size() >= target_length && "curve length not enough");
	union_curve.erase(union_curve.begin() + target_length, union_curve.end());
	return union_curve;
}

int union_curve_data_begin(double sample_rate, double frequency) {
	int preamble_cnt = 32;
	return (preamble_cnt + 2) * sample_rate / frequency;
}

vector<int> generate_all_patterns(int effect_length, bool verbose=false) {
	int reference_cnt = 4 * (1 << effect_length);
	vector<int> patterns; patterns.resize(reference_cnt);
	int cnt = 1 << effect_length;
	for (int i=0; i<cnt; ++i) {
		for (int _previous_logic_bit=0; _previous_logic_bit<2; ++_previous_logic_bit) {
			for (int _signal=0; _signal<2; ++_signal) {
				int output = 0;
				int previous_logic_bit = _previous_logic_bit;
				int signal = _signal;
				int bits = (previous_logic_bit << 1) | signal;
				if (verbose) printf("(%d%d)", previous_logic_bit, signal);
				for (int k=0; k<effect_length; ++k) {
					if (verbose) printf("%d", (i >> k) & 1);
					bits = (bits << 1) | ((i >> k) & 1);
				} if (verbose) printf(":");
				for (int k=0; k<effect_length; ++k) {
					int bit = (i >> k) & 1;
					if (bit == 0) {
						if (previous_logic_bit == 0) {
							signal = 1 - signal;
							// output |= signal << (2*k);
							// output |= signal << (2*k+1);
							output = (output << 1) | signal;
							output = (output << 1) | signal;
						}
						else {
							// output |= signal << (2*k);
							// output |= signal << (2*k+1);
							output = (output << 1) | signal;
							output = (output << 1) | signal;
						}
					} else {
						output = (output << 1) | signal;
						// output |= signal << (2*k);
						signal = 1 - signal;
						output = (output << 1) | signal;
						// output |= signal << (2*k+1);
					}
					previous_logic_bit = bit;
				}
				if (verbose) for (int k=0; k<2*effect_length; ++k) {
					printf("%d", (output >> k) & 1);
				}
				patterns[bits] = output;
				if (verbose) printf("\n");
			}
		}
	}
	return patterns;
}

map<int, vector<float>> get_reference(const vector<char>& binary, int effect_length, int ref_length) {
	assert(effect_length <= 16 && "cannot process too large number");
	const float* buffer = (const float*)binary.data();
	int length = binary.size() / sizeof(float);
	int reference_cnt = 4 * (1 << effect_length);
	// printf("length: %d, reference_cnt: %d, ref_length: %d\n", length, reference_cnt, ref_length);
	assert((length == reference_cnt * ref_length) && "length not match");
	map<int, vector<float>> refs;
	vector<int> patterns = generate_all_patterns(effect_length);
	assert(reference_cnt == (int)patterns.size() && "strange not equal");
	for (int i=0; i<reference_cnt; ++i) {
		const float* buf = buffer + i * ref_length;
		vector<float> ref(buf, buf + ref_length);
		refs[patterns[i]] = ref;
	}
	assert(reference_cnt == 2 * (int)refs.size() && "strange count of different pattern");
	return refs;
}

float union_curve_add(const float* a, int length) {
	float sum = 0;
	for (int i=0; i<length; ++i) {
		sum += a[i];
	}
	return sum;
}

float union_curve_minus_2(const float* a, const float* b, int length) {
	float sum = 0;
	for (int i=0; i<length; ++i) {
		float d = a[i] - b[i];
		sum += d * d;
	}
	return sum;
}

float union_curve_match_coeff_no_DC(const float* a, const float* b, int length) {
	float add_a = union_curve_add(a, length);
	float add_b = union_curve_add(b, length);
	float minus_a_b = union_curve_minus_2(a, b, length);
	float c = add_b - add_a;
	return minus_a_b - c*c/length;
}
