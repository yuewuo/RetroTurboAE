// use static here, simply test it and used later with demodulator
#define TAG_L4XX_SAMPLE_BYTE 32  // to enable NLCD = 32
#define STATIC_EMULATOR_FOR_EASIER_COPY
#include "emulator.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include <regex>
#include <random>
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;
FastDSM_Encoder encoder;
FastDSM_Emulator emulator;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

    if (argc != 3 && argc != 4 && argc != 5) {
        printf("usage: <collection> <id(12 byte hex = 24 char)> [ASSERT_METADATA:1] [EQUAL_LEN:0]\n");
        printf("\n");
        printf("the database document should have the following elements:\n");
        printf("  common: NLCD, duty, cycle, frequency, effect_length, ct_fast, ct_slow, combine, bit_per_symbol, bias\n");
        printf("  data: format as hex string\n");
        printf("  noise: [optional] given by standard deviation, Guassian distribution\n");
        printf("  refs_id: bson id which points to a valid reference file (some metadata needed)\n");
        return -1;
    }

	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
    bool ASSERT_METADATA = argc >= 4 ? atoi(argv[3]) : 1;
    bool EQUAL_LEN = argc >= 5 ? atoi(argv[4]) : 0;

	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);

    // NLCD
    assert(record["NLCD"].existed() && record["NLCD"].type() == BSON_TYPE_INT32);
    int NLCD = record["NLCD"].value<int32_t>(); printf("NLCD: %d\n", NLCD);
    assert(NLCD > 0 && NLCD <= EMULATOR_MAX_NLCD && "invalid NLCD");
    FastDSM_Emulator::NLCD = encoder.NLCD = NLCD;
    
    // cycle
    assert(record["cycle"].existed() && record["cycle"].type() == BSON_TYPE_INT32);
    int cycle = record["cycle"].value<int32_t>(); printf("cycle: %d\n", cycle);
    assert(cycle > 0 && "invalid cycle");
    FastDSM_Emulator::cycle = encoder.cycle = cycle;
    
    // duty
    assert(record["duty"].existed() && record["duty"].type() == BSON_TYPE_INT32);
    int duty = record["duty"].value<int32_t>(); printf("duty: %d\n", duty);
    assert(duty > 0 && duty <= duty && "invalid duty");
    FastDSM_Emulator::duty = encoder.duty = duty;

    // frequency
    assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
    double frequency = record["frequency"].value<double>(); printf("frequency: %f\n", frequency);
    FastDSM_Emulator::frequency = encoder.frequency = frequency;
    
    // effect_length
    assert(record["effect_length"].existed() && record["effect_length"].type() == BSON_TYPE_INT32);
    int effect_length = record["effect_length"].value<int32_t>(); printf("effect_length: %d\n", effect_length);
    assert(effect_length > 0 && effect_length <= 16 && "invalid effect_length");
    FastDSM_Emulator::effect_length = effect_length;
    
    // ct_fast
    assert(record["ct_fast"].existed() && record["ct_fast"].type() == BSON_TYPE_INT32);
    int ct_fast = record["ct_fast"].value<int32_t>(); printf("ct_fast: %d\n", ct_fast);
    assert(ct_fast >= 0 && "invalid ct_fast");
    encoder.ct_fast = ct_fast;
    
    // ct_slow
    assert(record["ct_slow"].existed() && record["ct_slow"].type() == BSON_TYPE_INT32);
    int ct_slow = record["ct_slow"].value<int32_t>(); printf("ct_slow: %d\n", ct_slow);
    assert(ct_slow >= 0 && "invalid ct_slow");
    encoder.ct_slow = ct_slow;
    
    // combine
    assert(record["combine"].existed() && record["combine"].type() == BSON_TYPE_INT32);
    int combine = record["combine"].value<int32_t>(); printf("combine: %d\n", combine);
    assert(combine > 0 && combine <= NLCD && "invalid combine");
    FastDSM_Emulator::combine = encoder.combine = combine;
    
    // bit_per_symbol
    assert(record["bit_per_symbol"].existed() && record["bit_per_symbol"].type() == BSON_TYPE_INT32);
    int bit_per_symbol = record["bit_per_symbol"].value<int32_t>(); printf("bit_per_symbol: %d\n", bit_per_symbol);
    assert(bit_per_symbol >= 2 && bit_per_symbol <= 8 && bit_per_symbol%2 == 0 && "invalid bit_per_symbol");
    encoder.bit_per_symbol = bit_per_symbol;
    
    // bias
    assert(record["bias"].existed() && record["bias"].type() == BSON_TYPE_INT32);
    FastDSM_Emulator::bias = record["bias"].value<int32_t>(); printf("bias: %d\n", FastDSM_Emulator::bias);

    // data
	assert(record["data"].existed() && record["data"].type() == BSON_TYPE_UTF8 && "data needed in hex format");
	vector<uint8_t> data = record["data"].get_bytes_from_hex_string();

    // channel_training_type
    if (record["channel_training_type"].existed() && record["channel_training_type"].type() == BSON_TYPE_INT32) {
        encoder.channel_training_type = record["channel_training_type"].value<int32_t>();
    } printf("channel_training_type: %d\n", encoder.channel_training_type);

    // noise
    double noise = 0;
    if (record["noise"].existed() && record["noise"].type() == BSON_TYPE_DOUBLE) {
        noise = record["noise"].value<double>();
    } printf("noise: %f\n", noise);
    FastDSM_Emulator::noise = noise;

    size_t target_length = 0;  // final result will not be less than this
    if (EQUAL_LEN) {
        assert(record["data_id"].existed() && record["data_id"].type() == BSON_TYPE_UTF8);
        string data_id_str = record["data_id"].value<string>();
        assert(MongoDat::isPossibleOID(data_id_str.c_str()) && "data_id invalid");
        bson_oid_t data_id = MongoDat::parseOID(data_id_str.c_str());
        vector<char> data_binary = mongodat.get_binary_file(data_id);
        target_length = data_binary.size() / sizeof(complex<float>);
    }

    // refs
    assert(record["refs_id"].existed() && record["refs_id"].type() == BSON_TYPE_UTF8);
    string refs_id_str = record["refs_id"].value<string>();
    assert(MongoDat::isPossibleOID(refs_id_str.c_str()) && "data_id invalid");
    bson_oid_t data_id = MongoDat::parseOID(refs_id_str.c_str());
    vector<char> binary = mongodat.get_binary_file(data_id);
    int data_length = binary.size() / sizeof(complex<float>);
    FastDSM_Emulator::refs.resize(data_length);
    memcpy((void*)FastDSM_Emulator::refs.data(), binary.data(), binary.size());  // save the reference in the emulator
    bson_t* metadata_ = mongodat.get_metadata_record(data_id);
    BsonOp metadata(&metadata_);
#define EMU_ASSERT(x) if (ASSERT_METADATA) { assert(x); } else do { if (!(x)) printf("ASSERT FAILED! "#x"\n"); } while(0)
    EMU_ASSERT(metadata["NLCD"].existed() && metadata["NLCD"].type() == BSON_TYPE_INT32 && NLCD == metadata["NLCD"].value<int32_t>());
    EMU_ASSERT(metadata["duty"].existed() && metadata["duty"].type() == BSON_TYPE_INT32 && duty == metadata["duty"].value<int32_t>());
    EMU_ASSERT(metadata["cycle"].existed() && metadata["cycle"].type() == BSON_TYPE_INT32 && cycle == metadata["cycle"].value<int32_t>());
    EMU_ASSERT(metadata["effect_length"].existed() && metadata["effect_length"].type() == BSON_TYPE_INT32 && effect_length == metadata["effect_length"].value<int32_t>());
    EMU_ASSERT(metadata["frequency"].existed() && metadata["frequency"].type() == BSON_TYPE_DOUBLE && frequency == metadata["frequency"].value<double>());

    // compute the bias of emulation (basically preamble and channel training)
    encoder.build_preamble();
    encoder.build_channel_training();
    printf("encoder.channel_training_type: %d\n", encoder.channel_training_type);
    printf("encoder.o_preamble.size(): %d\n", (int)encoder.o_preamble.size());
    printf("encoder.o_channel_training.size(): %d\n", (int)encoder.o_channel_training.size());
    FastDSM_Emulator::bias += (encoder.o_preamble.size() + encoder.o_channel_training.size()) * emulator.orate / emulator.frequency;
    printf("recomputed bias: %d\n", FastDSM_Emulator::bias);

    encoder.compute_o_parameters();
    vector<Tag_Single_t> singles = FastDSM_Encoder::PQAMencode(data.data(), data.size(), encoder.bit_per_symbol);
    emulator.sanity_check();  // this will check the size of reference file
    vector< complex<float> > emulated = emulator.emulate(singles, target_length);
    if (target_length && emulated.size() > target_length) emulated.erase(emulated.begin() + target_length, emulated.end());
    mongodat.upload_record("emulated", (float*)emulated.data(), 2, emulated.size(), NULL, "emulated", 1/80., "time(ms)", 1, "I,Q");
    printf("upload emulated file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

    record["emulated_id"] = MongoDat::OID2str(mongodat.get_fileID());
    record["o_throughput"] = encoder.o_throughput;
    record.save();

    mongodat.close();

    return 0;
}
