#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	if (argc != 3) {
		printf("usage: <local_filename> <remote_fileID>\n");
		return 0;
	}
	const char* local_filename = argv[1];
	const char* file_id = argv[2];
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	bson_oid_t id = MongoDat::parseOID(file_id);
	printf("file_id: 0x%s\n", MongoDat::OID2str(id).c_str());
	if (!mongodat.is_gridfs_file_existed(id)) {
		printf("file not existed\n");
		return -1;
	}

	int ret = mongodat.download_file(local_filename, id);
	if (ret) {
		printf("error(%d): %s\n", ret, mongodat.error.message);
	} else {
		printf("download done\n");
	}
	mongodat.close();
	MongoDat::LibDeinit();
}
