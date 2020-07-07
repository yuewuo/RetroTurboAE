#include <cstdint>
#include <cassert>
#include <fstream>
#include <future>
using  namespace std;
#include <preamble.h>
//
// Created by prwang on 7/17/2019.
//
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
	}, max_data_once, input_len * 8 / 5 / ref_len);
}
template<class T> vector<T> read_all(const char* filename) {
	ifstream ifs(filename, ios::binary);
	ifs.seekg(0, ios_base::end);
	int sz = ifs.tellg();
	assert(sz % sizeof(T) == 0 && "File size should be a multiple of type size");
	vector<T> ret; ret.resize(sz / sizeof(T));
	ifs.seekg(0, ios_base::beg);
	ifs.read((char*)ret.data(), sz);
	return ret;
}
int main(int argc, char** argv) {
	if (argc != 5) {
		puts(R"(Usage: tool <input_file> <ref_file> <output_length> <output_file>
If this program is to be ported to master branch for some reason, preamble.cpp/preamble.h
in this branch is required as well. It is better to have a separate copy; otherwise your
 reader-H7-ex.h will not compile with this changed interface.)");
		exit(3);
	}
  auto input = read_all<array<int16_t, 4>>(argv[1]);
  auto ref = read_all<array<float, 2>>(argv[2]);
  int out_len = atoi(argv[3]);
  unique_ptr<float[]> output_data(new float[2 * out_len]);
  printf("preamble detection returns with snr = %f\n", preamble_offline(out_len, &output_data[0], 3.3, ref.size(), (const float*)ref.data(),
  				input.size(), (const int16_t*)input.data()));
  ofstream of(argv[4], ios::binary);
  of.write((const char*)&output_data[0], 2 * out_len * sizeof(float));
}
