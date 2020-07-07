#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"

MongoDat mongodat;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: <remote_filename>");
		return 0;
	}
	const char* remote_filename = argv[1];
	MongoDat::LibInit();
	mongodat.open();

	char buf[16];
	sprintf(buf, "hello world!");

	// see http://mongoc.org/libbson/current/creating.html for how to create your own metadata
	bson_t metadata = BSON_INITIALIZER;
	BSON_APPEND_UTF8 (&metadata, "key", "value");
	mongodat.upload_record(remote_filename, (int8_t*)buf, 1, sizeof(buf), &metadata, "Test Record");
	printf("%s\n", MongoDat::dump(metadata).c_str());
	bson_destroy(&metadata);

	bson_oid_t id = mongodat.get_fileID();
	printf("id: %s\n", MongoDat::OID2str(id).c_str());
	printf("type: %s\n", mongodat.typeof_record(id).c_str());
	size_t dimension, length;
	assert(mongodat.get_dimension_length_record(id, &dimension, &length) == 0 && "get dimension and length failed");
	printf("dimension: %d, length: %d\n", (int)dimension, (int)length);
	printf("x_label: %s\n", mongodat.get_x_label_record(id).c_str());
	printf("y_label: %s\n", mongodat.get_y_label_record(id).c_str());

	mongodat.close();
	MongoDat::LibDeinit();
}
