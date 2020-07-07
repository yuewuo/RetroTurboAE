// a really naive demodulate program
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "Demodulate_Preamble_WY_190718.h"
#include <vector>
#include <string>
#include <map>
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 6) {
		printf("usage: <preamble_ref_id> <all_refs_id> <effect_length:int> <collection> <id(12 byte hex = 24 char)>");
		return -1;
	}

	const char* preamble_ref_id_str = argv[1];
	const char* all_refs_id_str = argv[2];
	int effect_length = atoi(argv[3]);
	assert(effect_length <= 16 && "cannot process too large number");
	int reference_cnt = 4 * (1 << effect_length);  // actually it could be 2* (see Find_Valid_Miller_WY_190715), improve that later
	printf("will capture %d references\n", reference_cnt);
	const char* collection_str = argv[4];
	const char* record_id_str = argv[5];

	// first take frequency and compute basic parameters
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
	assert(record["length"].existed() && record["length"].type() == BSON_TYPE_INT32);
	int ori_length = record["length"].value<int32_t>();
	assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
	double frequency = record["frequency"].value<double>();
	double throughput = frequency / 2;
	printf("frequency: %f, throughtput: %f\n", frequency, throughput);
	double sample_rate = 80000;
	int ref_length = sample_rate / throughput;
	assert(record["record_id"].existed() && record["record_id"].type() == BSON_TYPE_UTF8);
	string record_id = record["record_id"].value<string>();
	assert(record["data"].existed() && record["data"].type() == BSON_TYPE_UTF8);
	vector<uint8_t> data = record["data"].get_bytes_from_hex_string();
	BsonOp arr = record["packet"];
	assert(arr.existed() && arr.type() == BSON_TYPE_ARRAY);
	vector<string> compressed; int arr_length = arr.count();
	for (int j=0; j<arr_length; ++j) {
		compressed.push_back(arr[j].value<string>());
	}
	vector<uint8_t> samples;
	for (size_t j=0; j<compressed.size(); ++j) {
		string line = compressed[j];
		int cnt = 1;
		if (line.find(':') != string::npos) {
			cnt = atoi(line.c_str() + line.find(':') + 1);
		}
		samples.insert(samples.end(), cnt, line[0] == 'F');  // the sample is inverted so that shorter string could also work well
	}  // this part tested

	bson_oid_t all_refs_id = MongoDat::parseOID(all_refs_id_str);
	vector<char> refs_binary = mongodat.get_binary_file(all_refs_id);
	map<int, vector<float>> refs = get_reference(refs_binary, effect_length, ref_length);

	bson_oid_t preamble_ref_id = MongoDat::parseOID(preamble_ref_id_str);
	vector<char> preamble_ref_binary = mongodat.get_binary_file(preamble_ref_id);
	assert(preamble_ref_binary.size() % sizeof(complex<float>) == 0 && "preamble_ref alignment error");
	vector<complex<float>> preamble_ref; preamble_ref.resize(preamble_ref_binary.size() / sizeof(complex<float>));
	memcpy(preamble_ref.data(), preamble_ref_binary.data(), preamble_ref_binary.size());
	
	bson_oid_t data_id = MongoDat::parseOID(record_id.c_str());
	vector<char> binary = mongodat.get_binary_file(data_id);
#define EXTRA_TIME_S 5e-3
	vector<float> union_curve = union_curve_parse(binary, (samples.size() / frequency + EXTRA_TIME_S) * sample_rate, preamble_ref);
	
	// mongodat.upload_record("union curve", (float*)union_curve.data(), 1, union_curve.size()
	// 	, NULL, "simple union of four", 1/80., "time(ms)", 1, "data");
	// printf("upload union curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// demodulate begin
	vector<uint8_t> decoded; decoded.resize(data.size());
	int data_start = union_curve_data_begin(sample_rate, frequency);
	printf("data_start: %d (%f ms)\n", data_start, data_start / sample_rate * 1000);
	int mask = (1 << (2 * effect_length)) - 1;
	printf("using mask for pattern: 0x%04X\n", mask);
	int g_pattern = 0b01100 & mask;  // only this satisfy miller... otherwise must record extra ref for start few bits
	int g_previous_logic_bit = 0;
	int g_signal = 0;
	for (int i=0; i<ori_length; ++i) {
		float coeff[2];
		for (int bit=0; bit<2; ++bit) {  // try 0 and 1 respectively
			int pattern = g_pattern;
			int signal = g_signal;
			if (bit == 0) {
				if (g_previous_logic_bit == 0) {
					signal = 1 - signal;
					pattern = (pattern << 1) | signal;
					pattern = (pattern << 1) | signal;
				}
				else {
					pattern = (pattern << 1) | signal;
					pattern = (pattern << 1) | signal;
				}
			} else {
				pattern = (pattern << 1) | signal;
				signal = 1 - signal;
				pattern = (pattern << 1) | signal;
			}
			pattern &= mask;
			int target_start = data_start + (i * 2) * sample_rate / frequency;
			vector<float>& ref = refs[pattern];
			// printf("use pattern 0x%04X, length: %d\n", pattern, (int)ref.size());
			assert((int)ref.size() == ref_length && "cannot find pattern or pattern incorrect");
			// then compute the match coefficient
			coeff[bit] = union_curve_match_coeff_no_DC(union_curve.data() + target_start, ref.data(), ref_length);
		}
		// printf("coeff %f v.s. %f\n", coeff[0], coeff[1]);  // debug
		int bit = (coeff[0] < coeff[1]) ? 0 : 1;  // choose the small one
		decoded[i/8] |= bit << (i%8);
		if (bit == 0) {
			if (g_previous_logic_bit == 0) {
				g_signal = 1 - g_signal;
				g_pattern = (g_pattern << 1) | g_signal;
				g_pattern = (g_pattern << 1) | g_signal;
			}
			else {
				g_pattern = (g_pattern << 1) | g_signal;
				g_pattern = (g_pattern << 1) | g_signal;
			}
		} else {
			g_pattern = (g_pattern << 1) | g_signal;
			g_signal = 1 - g_signal;
			g_pattern = (g_pattern << 1) | g_signal;
		}
		g_previous_logic_bit = bit;
		g_pattern &= mask;
	}
	
	printf("origin: %s\n", MongoDat::dump(data).c_str());
	printf("decode: %s\n", MongoDat::dump(decoded).c_str());

	double BER = 0;
	for (int i=0; i<ori_length; ++i) {
		if ((decoded[i/8] ^ data[i/8]) & (1 << (i%8))) ++BER;
	}
	BER /= ori_length;
	printf("BER: %f %%", BER * 100);

	record["BER"] = BER;
	record.save();

	mongodat.close();
	record.remove();

	return 0;
}
