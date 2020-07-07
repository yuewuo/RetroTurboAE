#include <vector>
#include <map>
#include "stdlib.h"
#include "stdio.h"
#include "assert.h"
#include "math.h"
#include "Archive_Preamble.h"
using namespace std;

ofstream db("db.raw", ios::binary);
static float preamble_offline(int out_len, float* out, float snrthres,
																 int ref_len, const float* ref/*2x*/,
																 int input_len, const int16_t *input/*4x*/)
{
	constexpr int max_data_once = 2048;
	float input_buf[max_data_once * 4];
	promise<void> start_barrier; int input_cursor = 0;
	auto f = start_barrier.get_future();

	for (int i = 0; i < max_data_once * 4; ++i) {
		input_buf[i] = (short)rand() % 2;
	}
	return preamble_rx(out_len, out, snrthres, ref_len, ref, move(start_barrier), [&](size_t requested_4float) -> const float* {
		size_t n = requested_4float * 4;  // float count
		if (f.wait_for(0s) == future_status::ready) { //do not feed meaningful data before internal state sets up
			short* rsv = (short*)&input_buf[n] - n;  // about the middle
			for (int i = 0; i < (int)n; ++i) {
				rsv[i] = input_cursor < input_len * 4 ? input[input_cursor] : (short)rand() % 2;
				input_cursor++;
			}
			for (size_t i = 0; i < n; ++i) input_buf[i] = float(rsv[i]);
		}
		db.write((char*)input_buf, n * sizeof(float));
		db.flush();
		return input_buf;
	}, max_data_once, input_len * 8 / 5 / ref_len + 200);
}

vector<float> union_curve_parse(const vector<char>& binary, int target_length, vector<complex<float>>& preamble_ref) {
	typedef struct { int16_t s[4]; } uni_out_t;
	const uni_out_t* buffer2 = (const uni_out_t*)binary.data();
	int length = binary.size() / sizeof(uni_out_t);
	// use preamble to find start

	unique_ptr<float[]> output_data(new float[2 * target_length]);
	printf("preamble detection returns with snr = %f\n", preamble_offline(target_length, &output_data[0], 3.3, preamble_ref.size(), (const float*)preamble_ref.data(),
		length, (const int16_t*)buffer2));

	vector<float> union_curve; union_curve.resize(target_length);
	for (int i=0; i<target_length; ++i) union_curve[i] = output_data[2*i];
	
	return union_curve;
}

int union_curve_data_begin(double sample_rate, double frequency) {
	int preamble_cnt = 32 * frequency / 2000;
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
