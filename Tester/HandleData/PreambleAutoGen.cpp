#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "modulator.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;
FastDSM_Encoder encoder;

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
	printf(#name": %d\n", (int)(encoder.name = record[#name].value<int32_t>()));
	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);
	NEED_RECORD_INT32(NLCD)
	NEED_RECORD_INT32(preamble_repeat)  // repeat for how many times, usually 50 is good enough, which is about 3s
	assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
	printf("frequency: %f\n", (float)(encoder.frequency = record["frequency"].value<double>()));

	// generate preamble and save in record
	encoder.build_preamble();
	// printf("preamble:\n"); encoder.dump(encoder.o_preamble);
	record["o_auto_preamble_length"] = encoder.o_preamble_length;
	record["o_auto_preamble"].remove();
	record["o_auto_preamble"].build_array();
	vector<string> strs_preamble = encoder.compressed_vector(encoder.o_preamble);
	record["o_auto_preamble"].append(strs_preamble);
	record["o_preamble_length"] = encoder.o_preamble_length / encoder.preamble_repeat;

	// save file to database
	printf("saved id: %s\n", MongoDat::OID2str(*record.save()).c_str());

	mongodat.close();
	record.remove();

	return 0;
}
