/*
 * This is handler after run "WebGUI/lua/190808_reference_collect_renew.lua"
 * 
 * The output file is consist of NLCD * 2 * (m_sequence length) * (cycle/frequency*80k) * complex<float>
 * the information of effect_length, cycle, duty, frequency are recorded in the metadata field
 */

#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include <complex>
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
	
    // NLCD
    assert(record["NLCD"].existed() && record["NLCD"].type() == BSON_TYPE_INT32);
    int NLCD = record["NLCD"].value<int32_t>(); printf("NLCD: %d\n", NLCD);
    assert(NLCD > 0 && NLCD <= 16 && "invalid NLCD");

    // frequency
    assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
    double frequency = record["frequency"].value<double>(); printf("frequency: %f\n", frequency);

    // repeat_cnt
    assert(record["repeat_cnt"].existed() && record["repeat_cnt"].type() == BSON_TYPE_INT32);
    int repeat_cnt = record["repeat_cnt"].value<int32_t>(); printf("repeat_cnt: %d\n", repeat_cnt);
    assert(repeat_cnt > 0 && repeat_cnt <= 10000 && "should it exceed 10000? modify code if REALLY necessary");
	
    // cycle
    assert(record["cycle"].existed() && record["cycle"].type() == BSON_TYPE_INT32);
    int cycle = record["cycle"].value<int32_t>();
    printf("cycle: %d\n", cycle);

    // duty
    assert(record["duty"].existed() && record["duty"].type() == BSON_TYPE_INT32);
    int duty = record["duty"].value<int32_t>();
    printf("duty: %d\n", duty);
	
    // effect_length
    assert(record["effect_length"].existed() && record["effect_length"].type() == BSON_TYPE_INT32);
    int effect_length = record["effect_length"].value<int32_t>();
    printf("effect_length: %d\n", effect_length);
	assert(effect_length == 3 && "only support effect_length=3, otherwise please check the function. since non-zero value is observed as bias, it should not occur in the reference file. we need some information to evaluate this bias, which is the second bit of m sequence 0<0>10111");
	
	char o_data_id_key[32];
	vector< complex<float> > refs;
	for (int i=0; i<2*NLCD; ++i) {
		sprintf(o_data_id_key, "o_data_id_%d", i);
		assert(record[o_data_id_key].existed() && record[o_data_id_key].type() == BSON_TYPE_UTF8 && "data_id needed");
		string binary_id_str = record[o_data_id_key].value<string>();
		assert(MongoDat::isPossibleOID(binary_id_str.c_str()) && "data_id invalid");
		bson_oid_t data_id = MongoDat::parseOID(binary_id_str.c_str());
		vector<char> binary = mongodat.get_binary_file(data_id);
		complex<float> *data = (complex<float>*)binary.data();
		int data_length = binary.size() / sizeof(complex<float>);
		int data_cycle_length = cycle * 80e3 / frequency;
		int m_sequence_length = (1 << effect_length) - 1;
		int data_length_should_be = m_sequence_length * data_cycle_length;
		// printf("data_length: %d, data_length_should_be: %d\n", data_length, data_length_should_be);
		assert(data_length == data_length_should_be);
		// next evaluate the value of zero value
		complex<float> zero_bias;
		for (int j=data_cycle_length; j<2*data_cycle_length; ++j) {
			zero_bias += data[j];
		}
		zero_bias /= data_cycle_length;
		// then minus the data by the bias
		for (int j=0; j<data_length; ++j) {
			data[j] -= zero_bias;
		}
		refs.insert(refs.end(), data, data + data_length);
	}

	// finially output the refs
	bson_t metadata = BSON_INITIALIZER;
	BSON_APPEND_INT32(&metadata, "NLCD", NLCD);
	BSON_APPEND_INT32(&metadata, "duty", duty);
	BSON_APPEND_INT32(&metadata, "cycle", cycle);
	BSON_APPEND_DOUBLE(&metadata, "frequency", frequency);
	BSON_APPEND_INT32(&metadata, "effect_length", effect_length);
    mongodat.upload_record("refs", (float*)refs.data(), 2, refs.size(), &metadata, "reference file", 1/80., "time(ms)", 1, "I,Q");
	bson_destroy(&metadata);
    printf("upload reference file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    record["o_refs_id"] = MongoDat::OID2str(mongodat.get_fileID());

	record.save();

	mongodat.close();

	return 0;
}
