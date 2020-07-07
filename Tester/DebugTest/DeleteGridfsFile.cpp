#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	if (argc != 2) {
		printf("usage: <file_id>\n");
		return 0;
	}
	const char* file_id = argv[1];
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	bson_oid_t id = MongoDat::parseOID(file_id);
	printf("file_id: 0x%s\n", MongoDat::OID2str(id).c_str());

    int ret = mongodat.delete_gridfs_file(id);

	if (ret) {
		printf("error(%d): %s\n", ret, mongodat.error.message);
	} else {
		printf("delete done\n");
	}

	mongodat.close();
	MongoDat::LibDeinit();
}
