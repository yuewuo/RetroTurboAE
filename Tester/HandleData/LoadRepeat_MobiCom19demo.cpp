/*
 * Loading specific sequence to LCD and let it repeat for N times
 */

#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
    HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	if (argc != 4) {
		printf("usage: <collection> <id(12 byte hex = 24 char)> <port>\n");
        printf("  this requires the document containing the following key-values:\n");
        printf("  1. repeat_arr: the array of compress tag samples\n");
        printf("  2. repeat_NLCD: set the value of NLCD (mainly used to save space when you have <128 pixels)\n");
        printf("  3. repeat_count: the count of repetition\n");
        printf("  4. repeat_interval: the interval between packets, counting in frequency\n");
        printf("  5. repeat_frequency: frequency to send (double value)\n");
		return -1;
	}

	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
	const char* port = argv[3];

    TagL4Host_t tag;
	tag.verbose = true;
	tag.open(port);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

    BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
	assert(record.existed());
#define NEED_RECORD_INT32(name) \
	assert(record[#name].existed() && record[#name].type() == BSON_TYPE_INT32); \
    int name = record[#name].value<int32_t>();\
	printf(#name": %d\n", name);
	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);
    NEED_RECORD_INT32(repeat_NLCD)
    NEED_RECORD_INT32(repeat_count)
    NEED_RECORD_INT32(repeat_interval)
    assert(record["repeat_frequency"].existed() && record["repeat_frequency"].type() == BSON_TYPE_DOUBLE);
    double repeat_frequency = record["repeat_frequency"].value<double>();
	printf("repeat_frequency: %f\n", repeat_frequency);

    BsonOp repeat_arr = record["repeat_arr"];
	assert(repeat_arr.existed() && repeat_arr.type() == BSON_TYPE_ARRAY);
	vector<string> compressed; int repeat_arr_length = repeat_arr.count();
	for (int i=0; i<repeat_arr_length; ++i) {
		compressed.push_back(repeat_arr[i].value<string>());
		// printf("%s\n", compressed.back().c_str());
	}

    // enable 9V
    tag.mem.PIN_EN9 = 1;
	softio_blocking(write, tag.sio, tag.mem.PIN_EN9);
	tag.mem.PIN_PWSEL = 1;
	softio_blocking(write, tag.sio, tag.mem.PIN_PWSEL);

    int count;
	float real_frequency = tag.tx_repeat_compressed(repeat_frequency, compressed, repeat_NLCD, repeat_count, repeat_interval, count);
    printf("real_frequency: %f\n", real_frequency);

	mongodat.close();
	record.remove();

	tag.close();

    return 0;
}
