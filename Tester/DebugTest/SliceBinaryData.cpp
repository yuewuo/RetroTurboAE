#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	if (argc != 4) {
		printf("usage: <remote_fileID> <bias/points> <max_length/points>\n");
        printf("    set max_length to -1 for infinite max length\n");
		return 0;
	}
	const char* file_id = argv[1];
    int bias = atoi(argv[2]);
    int max_length = atoi(argv[3]);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	bson_oid_t id = MongoDat::parseOID(file_id);
	printf("file_id: 0x%s\n", MongoDat::OID2str(id).c_str());
	if (!mongodat.is_gridfs_file_existed(id)) {
		printf("file not existed\n");
		return -1;
	}

    const bson_t* metadata = mongodat.metadata_record(id);
    int dimension = getint32frombson(metadata, "dimension", -1);
    string type = getstringfrombson(metadata, "type", "");
    if (type == "" || dimension == -1) assert(0 && "record not valid, loss type or dimension");

    int type_size = 0;
    if (type == "int8_t") type_size = sizeof(int8_t);
    else if (type == "uint8_t") type_size = sizeof(uint8_t);
    else if (type == "int16_t") type_size = sizeof(int16_t);
    else if (type == "uint16_t") type_size = sizeof(uint16_t);
    else if (type == "int32_t") type_size = sizeof(int32_t);
    else if (type == "uint32_t") type_size = sizeof(uint32_t);
    else if (type == "float") type_size = sizeof(float);
    else if (type == "double") type_size = sizeof(double);
    else assert(0 && "data type not recognized");

    int idx_multi = type_size * dimension;
    vector<char> buf = mongodat.get_binary_file_bias_length(id, bias * idx_multi, max_length * idx_multi);
    mongodat.upload_binary("sliced", (void*)buf.data(), buf.size(), metadata);

    printf("upload sliced data with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	mongodat.close();
	MongoDat::LibDeinit();

    return 0;
}
