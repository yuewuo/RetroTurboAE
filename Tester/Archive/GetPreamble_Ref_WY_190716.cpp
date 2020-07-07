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

	if (argc != 3) {
		printf("usage: <collection> <id(12 byte hex = 24 char)>");
		return -1;
	}
	
	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];

	// first take frequency and compute basic parameters
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
	assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
	double frequency = record["frequency"].value<double>();
	double throughput = frequency / 2;
	printf("frequency: %f, throughtput: %f\n", frequency, throughput);
	double sample_rate = 56875;
	assert(record["record_id"].existed() && record["record_id"].type() == BSON_TYPE_UTF8);
	string data_record_id_str = record["record_id"].value<string>();

	bson_oid_t record_id = MongoDat::parseOID(data_record_id_str.c_str());
	vector<char> binary = mongodat.get_binary_file(record_id);
#define PREAMBLE_TIME_S 15e-3
	vector<float> union_curve = union_curve_parse(binary, sample_rate, frequency, PREAMBLE_TIME_S * sample_rate);
	// save union curve (optional for debug)
	mongodat.upload_record("union curve", (float*)union_curve.data(), 1, union_curve.size()
		, NULL, "simple union of four", 1/56.875, "time(ms)", 1, "data");
	printf("upload union curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	return 0;

}
