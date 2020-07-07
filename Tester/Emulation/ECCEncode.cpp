#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include <complex>
#include "rs.hpp" // see RetroTurbo/RetroLib/
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 3) {
		printf("usage: <collection> <id(12 byte hex = 24 char)>\n");
		return -1;
	}

	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
	
	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);
	
    // ecc_per_block
    assert(record["ecc_per_block"].existed() && record["ecc_per_block"].type() == BSON_TYPE_INT32);
    int ecc_per_block = record["ecc_per_block"].value<int32_t>();
    printf("ecc_per_block: %d\n", ecc_per_block);

    // pdata_per_block
    assert(record["pdata_per_block"].existed() && record["pdata_per_block"].type() == BSON_TYPE_INT32);
    int pdata_per_block = record["pdata_per_block"].value<int32_t>();
    printf("pdata_per_block: %d\n", pdata_per_block);

    // block_number
    assert(record["block_number"].existed() && record["block_number"].type() == BSON_TYPE_INT32);
    int block_number = record["block_number"].value<int32_t>();
    printf("block_number: %d\n", block_number);

    // primitive_data
    assert(record["primitive_data"].existed() && record["primitive_data"].type() == BSON_TYPE_UTF8 && "primitive_data needed in hex format");
	vector<uint8_t> primitive_data = record["primitive_data"].get_bytes_from_hex_string();
    
    // encode primitve data with ECC
    RS::ReedSolomon rs(pdata_per_block, ecc_per_block);
    vector<uint8_t> data(block_number * (pdata_per_block + ecc_per_block));
    for(int b = 0; b < block_number; b++)
    {
        rs.Encode(primitive_data.data() + b * pdata_per_block, data.data() + b * (ecc_per_block + pdata_per_block));
    }  

    // output to database
    string data_string;
	for(int i = 0; i < data.size(); i++)
	{
		char strbuf[4];
		sprintf(strbuf, "%02X", data[i]);
		data_string += strbuf;
	}
	record["data"] = data_string;

	record.save();
	mongodat.close();

	return 0;
}
