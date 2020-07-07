// assume the effected bit is at most 8bit, that means
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "Demodulate_WY_190715.h"
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

	if (argc < 4) {
		printf("usage: <effect_length:int> <collection> <id(12 byte hex = 24 char)> ...");
		return -1;
	}
	
	int effect_length = atoi(argv[1]);
	assert(effect_length <= 16 && "cannot process too large number");
	int reference_cnt = 4 * (1 << effect_length);  // actually it could be 2* (see Find_Valid_Miller_WY_190715), improve that later
	printf("will capture %d references\n", reference_cnt);
	const char* collection_str = argv[2];
	vector<const char*> record_id_strs;
	for (int i=3; i<argc; ++i) {
		record_id_strs.push_back(argv[i]);
	} printf("there're %d documents for the reference capture\n", (int)record_id_strs.size());

	// first take frequency and compute basic parameters
	BsonOp record0 = mongodat.get_bsonop(collection_str, record_id_strs[0]);
	assert(record0["length"].existed() && record0["length"].type() == BSON_TYPE_INT32);
	int ori_length = record0["length"].value<int32_t>();
	assert(record0["frequency"].existed() && record0["frequency"].type() == BSON_TYPE_DOUBLE);
	double frequency = record0["frequency"].value<double>();
	double throughput = frequency / 2;
	printf("frequency: %f, throughtput: %f\n", frequency, throughput);
	double sample_rate = 56875;
	int ref_length = sample_rate / throughput;
	int all_ref_samples = ref_length * reference_cnt;
	printf("ref_length: %d, all reference has %d samples (%d bytes)\n", ref_length, all_ref_samples, (int)(all_ref_samples * sizeof(float)));

	// get necessary information from documents
	struct Record {
		string record_id;
		string data;
		vector<uint8_t> samples;
		vector<float> union_curve;
		bool operator[] (int i) {
			int idx = i/4;
			idx ^= 0x01;
			assert(idx >=0 && idx < (int)data.size() && "index out of bound");
			char c = data[idx];
#define c2bs(x) (x>='0'&&x<='9'?(x-'0'):((x>='a'&&x<='f')?(x-'a'+10):((x>='A'&&x<='F')?(x-'A'+10):(0))))
			char b = c2bs(c);
#undef c2bs
			return (b >> (i%4)) & 0x01;
		}
	};
	vector<Record> records; records.resize(record_id_strs.size());
	for (int i=0; i<(int)record_id_strs.size(); ++i) {
		BsonOp record = mongodat.get_bsonop(collection_str, record_id_strs[i]);
		assert(record["record_id"].existed() && record["record_id"].type() == BSON_TYPE_UTF8);
		records[i].record_id = record["record_id"].value<string>();
		assert(record["data"].existed() && record["data"].type() == BSON_TYPE_UTF8);
		records[i].data = record["data"].value<string>();
		BsonOp arr = record["packet"];
		assert(arr.existed() && arr.type() == BSON_TYPE_ARRAY);
		vector<string> compressed; int arr_length = arr.count();
		for (int j=0; j<arr_length; ++j) {
			compressed.push_back(arr[j].value<string>());
		}
		for (size_t j=0; j<compressed.size(); ++j) {
			string line = compressed[j];
			int cnt = 1;
			if (line.find(':') != string::npos) {
				cnt = atoi(line.c_str() + line.find(':') + 1);
			}
			records[i].samples.insert(records[i].samples.end(), cnt, line[0] == 'F');  // the sample is inverted so that shorter string could also work well
		}  // this part tested
		int sample_cnt = (int)records[i].samples.size();
		printf("%s: %s, samples length: %d\n", records[i].record_id.c_str(), records[i].data.c_str(), sample_cnt);
	}

	// generate all patterns
	vector<int> patterns = generate_all_patterns(effect_length, true);
	printf("patterns has %d elements\n", (int)patterns.size());

	// get statistics information of pattern: how many patterns are found
	vector<vector<pair<int, int> > > index;  index.resize(reference_cnt);
	const int preamble_cnt = 32;
	for (int i=0; i<(int)record_id_strs.size(); ++i) {
		for (int j=0; j < ori_length - effect_length; ++j) {
			int idx_start = preamble_cnt + 2 + 2*j;
			int idx_end = idx_start + 2*effect_length;
			int pattern = 0;
			for (int k=idx_start; k<idx_end; ++k) {
				pattern = (pattern << 1) | (records[i].samples[k]);
			}
			// printf("pattern: ");
			// for (int k=2*effect_length-1; k>=0; --k) {
			// 	printf("%d", (pattern >> k) & 1);
			// } printf("\n");
			int found_cnt = 0;
			for (int k=0; k<(int)patterns.size(); ++k) {
				if (patterns[k] == pattern) {
					++found_cnt;
					index[k].push_back(make_pair(i, idx_start + 2*(effect_length-1)));  // should record for length 2
				}
			}
			assert(found_cnt == 2 && "strange, found invalid pattern");
		}
	}
	for (int i=0; i<reference_cnt; ++i) {
		printf("0x%08X: %d\n", i, (int)index[i].size());
		assert(index[i].size() != 0 && "cannot find this pattern, try add more documents");
	}

	// then get raw data and analyze
	for (int i=0; i<(int)record_id_strs.size(); ++i) {
		bson_oid_t record_id = MongoDat::parseOID(records[i].record_id.c_str());
		vector<char> binary = mongodat.get_binary_file(record_id);
#define EXTRA_TIME_S 4e-3
		records[i].union_curve = union_curve_parse(binary, sample_rate, frequency, (records[i].samples.size() / frequency + EXTRA_TIME_S) * sample_rate);
		// save union curve (optional for debug)
		// mongodat.upload_record("union curve", (float*)records[i].union_curve.data(), 1, records[i].union_curve.size()
		// 	, NULL, "simple union of four", 1/56.875, "time(ms)", 1, "data");
		// printf("upload union curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
	}

	// get actual reference
	vector<float> all_refs; all_refs.resize(all_ref_samples);
	for (int i=0; i<reference_cnt; ++i) {
		vector<float> refs; refs.resize(ref_length);
		for (int j=0; j<(int)index[i].size(); ++j) {
			auto a = index[i][j];
			int records_i = a.first;
			int record_start = a.second;
			const vector<float>& union_curve = records[records_i].union_curve;
			int uc_start = record_start / frequency * sample_rate;
			int uc_end = uc_start + ref_length;
			printf("take sample from %d (%f ms) to %d (%f ms)\n", uc_start, uc_start / sample_rate * 1000, uc_end, uc_end / sample_rate * 1000);
			for (int k=0; k<ref_length; ++k) {
				refs[k] += union_curve[uc_start + k];
			}
		}
		// then compute the average of those and output to all_refs
		for (int j=0; j<ref_length; ++j) {
			all_refs[i * ref_length + j] = refs[j] / index[i].size();
		}
	}
	// save all_refs
	mongodat.upload_record("all_refs", (float*)all_refs.data(), 1, all_refs.size()
		, NULL, "all_refs", 1/56.875, "time(ms)", 1, "data");
	bson_oid_t all_refs_id = mongodat.get_fileID();
	printf("upload all_refs with ID: %s\n", MongoDat::OID2str(all_refs_id).c_str());

	// use this to debug reference collection, by reconstruct the curve using this
	bool debug_reconstruct_curve0 = true;
if (debug_reconstruct_curve0) {
	vector<char> binary = mongodat.get_binary_file(all_refs_id);
	map<int, vector<float>> refs = get_reference(binary, effect_length, ref_length);
	printf("have %d different patterns\n", (int)refs.size());
	// then reconstruct data using these patterns
	int mask = (1 << (2 * effect_length)) - 1;
	printf("using mask for pattern: 0x%04X\n", mask);
	int pattern = 0b01100 & mask;  // only this satisfy miller... otherwise must record extra ref for start few bits
	int previous_logic_bit = 0;
	int signal = 0;
	Record& record = records[0];
	int data_start = union_curve_data_begin(sample_rate, frequency);
	vector<float> reconstructed; reconstructed.resize(data_start);
	for (int i=0; i<ori_length; ++i) {
		int bit = record[i];
		if (bit == 0) {
			if (previous_logic_bit == 0) {
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
		previous_logic_bit = bit;
		pattern &= mask;
		vector<float>& ref = refs[pattern];
		int target_start = data_start + (i * 2) * sample_rate / frequency;
		while ((int)reconstructed.size() < target_start) reconstructed.push_back(*reconstructed.rbegin());
		printf("use pattern 0x%04X, length: %d\n", pattern, (int)ref.size());
		assert(ref.size() && "cannot find pattern");
		reconstructed.insert(reconstructed.end(), ref.begin(), ref.end());
	}
	reconstructed.resize(reconstructed.size() + EXTRA_TIME_S * sample_rate);
	mongodat.upload_record("reconstructed", (float*)reconstructed.data(), 1, reconstructed.size()
		, NULL, "reconstructed", 1/56.875, "time(ms)", 1, "data");
	printf("upload reconstructed with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
}

	return 0;

}
