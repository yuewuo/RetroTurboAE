#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	if (argc != 3) {
		printf("usage: <remote_filename> <local_filename>");
		return 0;
	}
	const char* remote_filename = argv[1];
	const char* local_filename = argv[2];
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);
	int ret = mongodat.upload_file(remote_filename, local_filename);
	if (ret) {
		printf("error(%d): %s\n", ret, mongodat.error.message);
	}
	bson_oid_t file_id = mongodat.get_fileID();
	printf("file_id: 0x");
	for (size_t i=0; i<12; ++i) printf("%02X", file_id.bytes[i]);
	printf("\n");
	mongodat.close();
	MongoDat::LibDeinit();
}
