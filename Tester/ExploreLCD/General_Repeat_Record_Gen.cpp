/*
 * This file is to generate packet using given sequence and repeat count
 */

#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
#include "modulator.h"
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

FastDSM_Encoder encoder;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 3 && argc != 4) {
		printf("usage: <collection> <id(12 byte hex = 24 char)> [sequence_key=\"base_sequence\"]\n");
        printf("the document should have the following values:\n");
        printf("  NLCD: at most 16\n");
        printf("  frequency: LCD Tx frequency\n");
        printf("  base_sequence: a list of string that contains the basic sequence wanna collect\n");
        printf("  repeat_cnt: repeat the base_sequence for several times\n\n");
        printf("[warning] it doesn't ensure the time alignment when the packet is too long, because the existance of clock error\n");
		return -1;
	}

	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
    const char* base_sequence_key = "base_sequence";
    if (argc == 4) base_sequence_key = argv[3];
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);

	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);

    // NLCD
    assert(record["NLCD"].existed() && record["NLCD"].type() == BSON_TYPE_INT32);
    int NLCD = record["NLCD"].value<int32_t>(); printf("NLCD: %d\n", NLCD);
    assert(NLCD > 0 && NLCD <= 16 && "invalid NLCD");
    encoder.NLCD = NLCD;
    
    // frequency
    assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
    double frequency = record["frequency"].value<double>(); printf("frequency: %f\n", frequency);
    encoder.frequency = frequency;

    // repeat_cnt
    assert(record["repeat_cnt"].existed() && record["repeat_cnt"].type() == BSON_TYPE_INT32);
    int repeat_cnt = record["repeat_cnt"].value<int32_t>(); printf("repeat_cnt: %d\n", repeat_cnt);
    assert(repeat_cnt > 0 && repeat_cnt <= 10000 && "should it exceed 10000? modify code if REALLY necessary");

    // base_sequence
    assert(record[base_sequence_key].existed() && record[base_sequence_key].type() == BSON_TYPE_ARRAY);
    int str_cnt = record[base_sequence_key].count(); printf("base_sequence string count: %d\n", str_cnt);
    assert(str_cnt > 0 && "cannot send empty base_sequence");
    vector<string> base_sequence; base_sequence.resize(str_cnt);
    printf("base_sequence:\n");
    for (int i=0; i<str_cnt; ++i) {
        base_sequence[i] = record[base_sequence_key][i].value<string>();
        printf("    %s\n", base_sequence[i].c_str());
    }

    // build preamble sequence and final sequence
    encoder.build_preamble();
    vector<string> strs_final_sequence = encoder.compressed_vector(encoder.o_preamble);
    for (int i=0; i<repeat_cnt; ++i) {
        strs_final_sequence.insert(strs_final_sequence.end(), base_sequence.begin(), base_sequence.end());
    }
    strs_final_sequence.push_back("00");  // to reset all LCD

    record["o_final_preamble_length"] = (int)encoder.o_preamble.size();
    record["o_final_sequence_length"] = (int)FastDSM_Encoder::uncompressed_vector(strs_final_sequence).size();
	record["o_final_sequence"].remove();
	record["o_final_sequence"].build_array();
	record["o_final_sequence"].append(strs_final_sequence);

    record.save();

    mongodat.close();

    return 0;
}
