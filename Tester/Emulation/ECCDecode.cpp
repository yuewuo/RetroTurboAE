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

    // decoded
    assert(record["decoded"].existed() && record["decoded"].type() == BSON_TYPE_UTF8 && "decoded needed in hex format");
	vector<uint8_t> decoded = record["decoded"].get_bytes_from_hex_string();

    // primitive_data
    assert(record["primitive_data"].existed() && record["primitive_data"].type() == BSON_TYPE_UTF8 && "primitive_data needed in hex format");
	vector<uint8_t> primitive_data = record["primitive_data"].get_bytes_from_hex_string();

    // repaired_BER_output_key
    string repaired_BER_output_key = "Repaired_BER";
    string lost_output_key = "packet_lost";
    
    // repair decoded data with ECC
    RS::ReedSolomon rs(pdata_per_block, ecc_per_block);
    vector<uint8_t> repaired(block_number * pdata_per_block);
    for(int b = 0; b < block_number; b++)
    {
        rs.Decode(decoded.data() + b * (ecc_per_block + pdata_per_block), repaired.data() + b * pdata_per_block);
    }
    for(auto d : repaired)
    {
        printf("%02X", d);
    }
    printf("\n");    

    // output to database
    string repaired_string;
	for(int i = 0; i < repaired.size(); i++)
	{
		char strbuf[4];
		sprintf(strbuf, "%02X", repaired[i]);
		repaired_string += strbuf;
	}
	record["repaired"] = repaired_string;

    double repaired_BER = 0;
	for (int i=0; i<(int)primitive_data.size(); ++i) {
        for (int j=0; j<8; ++j) {
		    if ((repaired[i] ^ primitive_data[i]) & (1 << j)) ++repaired_BER;
        }
	}
	repaired_BER /= primitive_data.size() * 8;
	printf("Repaired BER: %f %%\n", repaired_BER * 100);

    record[repaired_BER_output_key.c_str()] = repaired_BER;
    if(repaired_BER < 0.00001)
        record[lost_output_key.c_str()] = 0;
    else
        record[lost_output_key.c_str()] = 1;
    

	record.save();
	mongodat.close();

	return 0;
}
