#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "MillerModulator.h"


const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;
MillerModulator encoder;

int main(int argc, char** argv) {



    /*
     * Database management
     */

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
	NEED_RECORD_INT32(length)
	NEED_RECORD_INT32(reverse)
#undef NEED_RECORD_INT32
	assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE); \
	printf("frequency: %f\n", (float)(encoder.frequency = record["frequency"].value<double>()));


    /*
     * Insert packet to database
     */
    
    encoder.Generate();
    record["packet"].remove();
    record["packet"].build_array();
    record["packet"].append(encoder.packet);
    
	// save file to database
	printf("saved id: %s\n", MongoDat::OID2str(*record.save()).c_str());

	mongodat.close();
	record.remove();

	return 0;
}
