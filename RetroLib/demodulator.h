/*
 * Demodulator Program
 */
#include <vector>
#include <complex>
using namespace std;

class Naive_Demodulator_WY_190621 {
public:
	int NLCD;  // the amount of LCD (8bit 8-4-2-1 dual-pixel)
	double frequency;
	int preamble_length;
	int ct_fast;
	int ct_slow;
	int combine;
	int cycle;
	int duty;
	int bit_per_symbol;
	double sample_rate;
	vector<unsigned char> decode(const complex<float>* buffer, int length, int bytes);
};

vector<unsigned char> Naive_Demodulator_WY_190621::decode(const complex<float>* buffer, int length, int bytes) {
	vector<unsigned char> data;
	data.resize(bytes);
}
