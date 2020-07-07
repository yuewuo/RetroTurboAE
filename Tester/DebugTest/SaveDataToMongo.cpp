#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	if (argc != 5) {
		printf("due to mongoc driver bug, cannot assign the ID of file\n");
		printf("alternatively, give the document and will modify the ID in it\n");
		printf("sorry for the inconvenience...\n");
		printf("usage: <local_filename> <document_collection> <document_id> <document_key>\n");
		return 0;
	}
	const char* local_filename = argv[1];
	const char* document_collection = argv[2];
	const char* document_id = argv[3];
	const char* document_key = argv[4];
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	printf("remote_id: 0x%s\n", document_id);
	int ret = mongodat.upload_file("uploaded", local_filename);
	if (ret) {
		printf("error(%d): %s\n", ret, mongodat.error.message);
		return ret;
	}
	// then get document to write in
	BsonOp record = mongodat.get_bsonop(document_collection, document_id);
	bson_oid_t file_id = mongodat.get_fileID();
	printf("file_id: 0x");
	for (size_t i=0; i<12; ++i) printf("%02X", file_id.bytes[i]);
	printf("\n");
	record[document_key] = MongoDat::OID2str(file_id);  // modify the file_id in document
	record.save();
	record.remove();

	mongodat.close();
	MongoDat::LibDeinit();
}
