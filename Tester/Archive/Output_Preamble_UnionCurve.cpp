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

	if (argc < 4) {
		printf("usage: <preamble_ref_id> <collection> <id(12 byte hex = 24 char)> [filename_prefix] [filename_suffix]");
		return -1;
	}

	const char* preamble_ref_id_str = argv[1];
	const char* collection_str = argv[2];
	const char* record_id_str = argv[3];
	string filename_prefix = argc > 4 ? argv[4] : "";
	string filename_suffix = argc > 5 ? argv[5] : "";

	// first take frequency and compute basic parameters
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
	assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
	double frequency = record["frequency"].value<double>();
	double throughput = frequency / 2;
	printf("frequency: %f, throughtput: %f\n", frequency, throughput);
	double sample_rate = 80000;
	assert(record["record_id"].existed() && record["record_id"].type() == BSON_TYPE_UTF8);
	string record_id = record["record_id"].value<string>();
	assert(record["data"].existed() && record["data"].type() == BSON_TYPE_UTF8);
	string data_str = record["data"].value<string>();
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

	bson_oid_t preamble_ref_id = MongoDat::parseOID(preamble_ref_id_str);
	vector<char> preamble_ref_binary = mongodat.get_binary_file(preamble_ref_id);
	assert(preamble_ref_binary.size() % sizeof(complex<float>) == 0 && "preamble_ref alignment error");
	vector<complex<float>> preamble_ref; preamble_ref.resize(preamble_ref_binary.size() / sizeof(complex<float>));
	memcpy(preamble_ref.data(), preamble_ref_binary.data(), preamble_ref_binary.size());
	
	bson_oid_t data_id = MongoDat::parseOID(record_id.c_str());
	vector<char> binary = mongodat.get_binary_file(data_id);
#define EXTRA_TIME_S 5e-3
	vector<float> union_curve = union_curve_parse(binary, (samples.size() / frequency + EXTRA_TIME_S) * sample_rate, preamble_ref);

	// output to database
	// mongodat.upload_record("union curve", (float*)union_curve.data(), 1, union_curve.size()
	// 	, NULL, "simple union of four", 1/80., "time(ms)", 1, "data");
	// printf("upload union curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// or output to file
	string filename = filename_prefix + data_str + filename_suffix;
	printf("output to file %s\n", filename.c_str());
	ofstream output(filename, ios::binary);
	output.write((const char*)union_curve.data(), union_curve.size() * sizeof(float));
	output.close();

	mongodat.close();

	return 0;
}
