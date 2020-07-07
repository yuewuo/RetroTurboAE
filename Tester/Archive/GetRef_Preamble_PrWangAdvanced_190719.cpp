// assume the effected bit is at most 8bit, that means
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

	if (argc < 4) {
		printf("usage: <preamble_ref_id> <collection> <id(12 byte hex = 24 char)> ...");
		return -1;
	}

	const char* preamble_ref_id_str = argv[1];
	const char* collection_str = argv[2];
	vector<const char*> record_id_strs;
	for (int i=3; i<argc; ++i) {
		record_id_strs.push_back(argv[i]);
	} printf("there're %d documents for the reference capture\n", (int)record_id_strs.size());

	// define the requested pattern from prwang
	struct Pattern {
		int idx;
		Pattern(int idx_): idx(idx_) {}
		int bit_length() {
			switch (idx) {
				case 0: return 1;
				case 1: case 2: case 3: return 2;
				case 4: case 5: return 3;
				default: assert(0 && "invalid index");
			}
		}
		bool has_pattern(int pattern) {
			switch (idx) {
				case 0: return pattern == 0b000111 || pattern == 0b100111;
				case 1:	return pattern == 0b110011;
				case 2: return pattern == 0b110001;
				case 3: return pattern == 0b011001 || pattern == 0b111001;
				case 4: return pattern == 0b100001;
				case 5: return pattern == 0b100011;
				default: assert(0 && "invalid index");
			}
		}
	};
	#define PATTERN_LENGTH 6
	#define PATTERNS_CNT 6
	Pattern patterns[PATTERNS_CNT] = { Pattern(0), Pattern(1), Pattern(2), Pattern(3), Pattern(4), Pattern(5) };

	// first take frequency and compute basic parameters
	BsonOp record0 = mongodat.get_bsonop(collection_str, record_id_strs[0]);
	assert(record0["length"].existed() && record0["length"].type() == BSON_TYPE_INT32);
	int ori_length = record0["length"].value<int32_t>();
	assert(record0["frequency"].existed() && record0["frequency"].type() == BSON_TYPE_DOUBLE);
	double frequency = record0["frequency"].value<double>();
	double throughput = frequency / 2;
	printf("frequency: %f, throughtput: %f\n", frequency, throughput);
	double sample_rate = 80000;
	int bit_ref_length = sample_rate / throughput;
	int reference_bit_cnt = 0;
	for (int i=0; i<PATTERNS_CNT; ++i) reference_bit_cnt += patterns[i].bit_length();
	printf("reference_bit_cnt: %d\n", reference_bit_cnt);
	int all_ref_samples = bit_ref_length * reference_bit_cnt;
	printf("bit_ref_length: %d, all reference has %d samples (%d bytes)\n", bit_ref_length, all_ref_samples, (int)(all_ref_samples * sizeof(float)));

	bson_oid_t preamble_ref_id = MongoDat::parseOID(preamble_ref_id_str);
	vector<char> preamble_ref_binary = mongodat.get_binary_file(preamble_ref_id);
	assert(preamble_ref_binary.size() % sizeof(complex<float>) == 0 && "preamble_ref alignment error");
	vector<complex<float>> preamble_ref; preamble_ref.resize(preamble_ref_binary.size() / sizeof(complex<float>));
	memcpy(preamble_ref.data(), preamble_ref_binary.data(), preamble_ref_binary.size());

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

	// get statistics information of pattern: how many patterns are found
	vector<vector<pair<int, int> > > index;  index.resize(PATTERNS_CNT);
	const int preamble_cnt = 32 * frequency / 2000;
	for (int i=0; i<(int)record_id_strs.size(); ++i) {
		for (int j=0; j < ori_length - PATTERN_LENGTH/2; ++j) {
			int idx_start = preamble_cnt + 2 + 2*j;
			int idx_end = idx_start + PATTERN_LENGTH;
			int pattern = 0;
			for (int k=idx_start; k<idx_end; ++k) {
				pattern = (pattern << 1) | (records[i].samples[k]);
			}
			// printf("pattern: ");
			// for (int k=PATTERNS_CNT-1; k>=0; --k) {
			// 	printf("%d", (pattern >> k) & 1);
			// } printf("\n");
			for (int k=0; k<(int)PATTERNS_CNT; ++k) {
				if (patterns[k].has_pattern(pattern)) {
					// printf("found pattern\n");
					index[k].push_back(make_pair(i, idx_start));  // should record for length 2
				}
			}
		}
	}
	for (int i=0; i<PATTERNS_CNT; ++i) {
		printf("pattern %d: %d\n", i, (int)index[i].size());
		assert(index[i].size() != 0 && "cannot find this pattern, try add more documents");
	}

	// then get raw data and analyze
	for (int i=0; i<(int)record_id_strs.size(); ++i) {
		bson_oid_t record_id = MongoDat::parseOID(records[i].record_id.c_str());
		vector<char> binary = mongodat.get_binary_file(record_id);
#define EXTRA_TIME_S 5e-3
		records[i].union_curve = union_curve_parse(binary, (records[i].samples.size() / frequency + EXTRA_TIME_S) * sample_rate, preamble_ref);
		// save union curve (optional for debug)
		mongodat.upload_record("union curve", (float*)records[i].union_curve.data(), 1, records[i].union_curve.size()
			, NULL, "simple union of four", 1/sample_rate*1000, "time(ms)", 1, "data");
		printf("upload union curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
	}

	// get actual reference
	vector<float> all_refs;
	for (int i=0; i<PATTERNS_CNT; ++i) {
		int ref_length = patterns[i].bit_length() * 2 / frequency * sample_rate;
		vector<float> refs; refs.resize(ref_length);
		for (int j=0; j<(int)index[i].size(); ++j) {
			auto a = index[i][j];
			int records_i = a.first;
			int record_start = a.second;
			const vector<float>& union_curve = records[records_i].union_curve;
			// printf("record_start: %d (%f ms)\n", (int)(record_start / frequency * sample_rate), record_start / frequency * 1000);
			int uc_start = (record_start + PATTERN_LENGTH) / frequency * sample_rate - ref_length;
			int uc_end = uc_start + ref_length;
			printf("take sample from %d (%f ms) to %d (%f ms)\n", uc_start, uc_start / sample_rate * 1000, uc_end, uc_end / sample_rate * 1000);
			for (int k=0; k<ref_length; ++k) {
				refs[k] += union_curve[uc_start + k];
			}
		}
		// then compute the average of those and output to all_refs
		for (int j=0; j<ref_length; ++j) {
			refs[j] /= index[i].size();
		}
		all_refs.insert(all_refs.end(), refs.begin(), refs.end());
	}
	assert(all_ref_samples == (int)all_refs.size() && "samples not equal, strange");
	// save all_refs
	mongodat.upload_record("all_refs", (float*)all_refs.data(), 1, all_refs.size()
		, NULL, "all_refs", 1/sample_rate*1000, "time(ms)", 1, "data");
	bson_oid_t all_refs_id = mongodat.get_fileID();
	printf("upload all_refs with ID: %s\n", MongoDat::OID2str(all_refs_id).c_str());

	return 0;

}
