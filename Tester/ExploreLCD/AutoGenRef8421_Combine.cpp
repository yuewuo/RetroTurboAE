/*
 * This program is simply a combiner of multiple binary and output a single binary
 */

#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc < 5) {
		printf("usage: <collection> <id(12 byte hex = 24 char)> <output_key_name> <input_key_name_1> <input_key_name_2> ... [M<scale>]\n");
        printf("    scale: a double value that is multiplied to each sample\n");
		return -1;
	}
    
	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
    const char* output_key_name = argv[3];
    vector<const char*> input_key_names;
    double scale = 1; bool is_scaled = false;
    for (int i=4; i<argc; ++i) {
        if (argv[i][0] == 'M') {
            is_scaled = true;
            scale = atof(argv[i]+1);
        } else {
            input_key_names.push_back(argv[i]);
        }
    }

    printf("combining %d binaries into \"%s\"\n", (int)input_key_names.size(), output_key_name);
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
    vector<char> output_binary;
    for (int i=0; i<(int)input_key_names.size(); ++i) {
        assert(record[input_key_names[i]].existed() && record[input_key_names[i]].type() == BSON_TYPE_UTF8);
        string binary_id_str = record[input_key_names[i]].value<string>();
        assert(MongoDat::isPossibleOID(binary_id_str.c_str()) && "data_id invalid");
        bson_oid_t record_id = MongoDat::parseOID(binary_id_str.c_str());
        vector<char> binary = mongodat.get_binary_file(record_id);
        output_binary.insert(output_binary.end(), binary.begin(), binary.end());
    }

    bson_t* metadata_ = mongodat.get_metadata_record(MongoDat::parseOID(record[input_key_names[0]].value<string>().c_str()));
    BsonOp metadata(&metadata_);
    assert(metadata["type"].existed() && metadata["type"].type() == BSON_TYPE_UTF8); string type = metadata["type"].value<string>();
    assert(metadata["x_unit"].existed() && metadata["x_unit"].type() == BSON_TYPE_DOUBLE); double x_unit = metadata["x_unit"].value<double>();
    assert(metadata["x_label"].existed() && metadata["x_label"].type() == BSON_TYPE_UTF8); string x_label = metadata["x_label"].value<string>();
    assert(metadata["y_unit"].existed() && metadata["y_unit"].type() == BSON_TYPE_DOUBLE); double y_unit = metadata["y_unit"].value<double>();
    assert(metadata["y_label"].existed() && metadata["y_label"].type() == BSON_TYPE_UTF8); string y_label = metadata["y_label"].value<string>();
    assert(metadata["dimension"].existed() && metadata["dimension"].type() == BSON_TYPE_INT32); int dimension = metadata["dimension"].value<int32_t>();
    assert(metadata["typesize"].existed() && metadata["typesize"].type() == BSON_TYPE_INT32); int typesize = metadata["typesize"].value<int32_t>();
    int output_size_1 = output_binary.size() / typesize;
    int output_size = output_size_1 / dimension;
#define UPLOAD_TYPE(t) {\
    t* d = (t*)output_binary.data();  \
    if (is_scaled) for (int i=0; i<output_size_1; ++i) d[i] *= scale;  \
    mongodat.upload_record("combined", (t*)output_binary.data(), dimension, output_size, NULL, "Combined binary", x_unit, x_label.c_str(), y_unit, y_label.c_str());\
}
    if (type == "uint8_t") UPLOAD_TYPE(uint8_t)
    else if (type == "int16_t") UPLOAD_TYPE(int16_t)
    else if (type == "uint16_t") UPLOAD_TYPE(uint16_t)
    else if (type == "int32_t") UPLOAD_TYPE(int32_t)
    else if (type == "uint32_t") UPLOAD_TYPE(uint32_t)
    else if (type == "float") UPLOAD_TYPE(float)
    else if (type == "double") UPLOAD_TYPE(double)
    else UPLOAD_TYPE(int8_t)

    printf("upload combined binary with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    record[output_key_name] = MongoDat::OID2str(mongodat.get_fileID());
    record.save();

    mongodat.close();

    return 0;
}
