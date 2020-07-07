#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"

MongoDat mongodat;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: <collection name>");
		return 0;
	}
	const char* collection_str = argv[1];
	MongoDat::LibInit();
	mongodat.open();
	
	vector<bson_oid_t> ids = mongodat.get_all_id(collection_str);
	printf("has %d documents\n", (int)ids.size());
	for (size_t i=0; i<ids.size() && i < 5; ++i) {
		printf("%d: %s\n", (int)i, MongoDat::OID2str(ids[i]).c_str());
	} if (ids.size() > 5) printf("...\n");

	mongodat.close();
	MongoDat::LibDeinit();
}
