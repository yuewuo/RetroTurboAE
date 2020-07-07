// to recover the original raw data from "packet" array
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 3) {
		printf("usage: <collection> <id(12 byte hex = 24 char)>");
		return -1;
	}
	
	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
	
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
#define NEED_RECORD_INT32(name) \
	assert(record[#name].existed() && record[#name].type() == BSON_TYPE_INT32); \
	printf(#name": %d\n", (int)(record[#name].value<int32_t>()));
	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);
	assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
	double frequency = record["frequency"].value<double>();
	NEED_RECORD_INT32(length);
	int ori_length = (int)(record["length"].value<int32_t>());

	// get compressed packet
	BsonOp arr = record["packet"];
	assert(arr.existed() && arr.type() == BSON_TYPE_ARRAY);
	vector<string> compressed; int arr_length = arr.count();
	for (int i=0; i<arr_length; ++i) {
		compressed.push_back(arr[i].value<string>());
	}
	int length = 0;
	vector<uint8_t> samples;
	for (size_t i=0; i<compressed.size(); ++i) {
		string line = compressed[i];
		int cnt = 1;
		if (line.find(':') != string::npos) {
			cnt = atoi(line.c_str() + line.find(':') + 1);
		}
		length += cnt;
		samples.insert(samples.end(), cnt, line[3] != '0');  // the sample is inverted so that shorter string could also work well
	}  // this part tested
	int sample_cnt = (int)samples.size();
	printf("samples length: %d\n", sample_cnt);

	int preamble_cnt = 32 * frequency / 2000;
	//     preamble       0   data         0
	assert(preamble_cnt + 2 + 2*ori_length + 2 == sample_cnt && "length not match");

	// then recover the data
	vector<uint8_t> data;  data.resize((ori_length + 7) / 8);
#define D(x, bit) (data[x/8] |= (!!(bit)) << (x%8))
	int previous_logic_bit = 0;  // because it first add 0 before data is encoded
	int signal = 0;
	for (int i=0; i<ori_length; ++i) {
		int idx = preamble_cnt + 2 + 2*i;
		if (samples[idx] == signal && samples[idx+1] == !signal) {
			D(i, 1);
			previous_logic_bit = 1;
			signal = samples[idx+1];
		} else if (previous_logic_bit == 0 && samples[idx] == !signal && samples[idx+1] == !signal) {
			D(i, 0);
			previous_logic_bit = 0;
			signal = samples[idx+1];
		} else if (previous_logic_bit == 1 && samples[idx] == signal && samples[idx+1] == signal) {
			D(i, 0);
			previous_logic_bit = 0;
			signal = samples[idx+1];
		} else {
			assert("failed to decode");
		}
	}
	printf("data: %s\n", MongoDat::dump(data).c_str());

	record["data"] = MongoDat::dump(data);
	
	record.save();

	mongodat.close();

}
