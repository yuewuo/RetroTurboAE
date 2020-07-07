/*
 * This program is to fetch data and try to demodulate it
 * 
 * The basic obervation is that, using "Tester/Emulation/ScatterPlot.cpp" we can see very nice scatter plot, 
 *     simply select the nearest point would give out a naive demodulation
 *     select multiple nearest ones would give possibly better result, with demodulation time trade-off
 * they are in the same manner so that this program requires a butter length of how many points to take, we define two parameters here:
 *     1. demod_buffer_length: the maximum number of possible decode object
 *     2. demod_nearest_count: this demodulation works as: do the same work in scatterplot, then find several nearst in the constellation diagram and compute their values
 *              note that this method is not accurate when evaluate the constellation diagram, because each sub-pixel's history is recordde individually. I assume that this wouldn't effect a lot, nonetheless later computation would fix this slight difference
 */
// use static here, simply test it and used later with demodulator
#define STATIC_EMULATOR_FOR_EASIER_COPY
#include "emulator.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include <regex>
#include <random>
#include <set>
#include <map>
#include <rpc_client.hpp>
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;
FastDSM_Encoder encoder;
FastDSM_Emulator emulator;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

    if (argc != 3 && argc != 4) {
        printf("usage: <collection> <id(12 byte hex = 24 char)> <port>\n");
        printf("\n");
        printf("the database document should have the following elements:\n");
        printf("  common: NLCD, duty, cycle, frequency, effect_length, ct_fast, ct_slow, combine, bit_per_symbol, bias\n");
        printf("  data: format as hex string\n");
        printf("  refs_id: bson id which points to a valid reference file (some metadata needed)\n");
        printf("  demod_data_id: the result of emulation or real experiment, to be analyzed\n");
        printf("you can set demod_buffer_length = 1 and demod_nearest_count = 1 for the fastest demodulation\n");
        printf("however, I suggest demod_nearest_count = 4 and demod_buffer_length >= 1 (you can adjust demod_buffer_length for deeper search depth)\n");
        return -1;
    }

    const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
	int server_port = argc == 4 ? atoi(argv[3]) : 0;
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);

	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);

    // NLCD
    assert(record["NLCD"].existed() && record["NLCD"].type() == BSON_TYPE_INT32);
    int NLCD = record["NLCD"].value<int32_t>(); printf("NLCD: %d\n", NLCD);
    assert(NLCD > 0 && NLCD <= 16 && "invalid NLCD");
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

    assert(encoder.channel_training_type == 0 && "must with ctd");

    // demod_data
    assert(record["demod_data_id"].existed() && record["demod_data_id"].type() == BSON_TYPE_UTF8);
    string demod_data_id_str = record["demod_data_id"].value<string>();
    assert(MongoDat::isPossibleOID(demod_data_id_str.c_str()) && "demod_data_id invalid");
    bson_oid_t demod_data_id = MongoDat::parseOID(demod_data_id_str.c_str());
    vector<char> demod_data_binary = mongodat.get_binary_file(demod_data_id);
    int demod_data_length = demod_data_binary.size() / sizeof(complex<float>);
    size_t target_length = demod_data_length;
    const complex<float>* sd1b = (const complex<float>*)demod_data_binary.data();

    // compute the bias of emulation (basically preamble and channel training)
    encoder.build_preamble();
    encoder.build_channel_training();
    FastDSM_Emulator::bias += (encoder.o_preamble.size() + encoder.o_channel_training.size()) * emulator.orate / emulator.frequency;
    printf("recomputed bias: %d\n", FastDSM_Emulator::bias);
    int channel_training_data_length = encoder.o_channel_training.size() * emulator.orate / emulator.frequency;
	const complex<float>* channel_training_data = sd1b + FastDSM_Emulator::bias - channel_training_data_length;

	complex<float>* demod_data = NULL;
    using cf=complex<float>;
    vector< complex<float> > new_reference;  // should be 
    rest_rpc::rpc_client c("127.0.0.1", server_port); assert(c.connect() && "connection timeout");
#define V (FastDSM_Emulator::effect_length)
#define L (encoder.o_dsm_order)
#define PW int(emulator.orate / emulator.frequency * emulator.cycle)
    vector<float> outf((const float*)channel_training_data,
            (const float*)(channel_training_data + PW * (2 * L)));
    auto [r2f, dcre, dcim] = c.call<tuple<vector<float>, float, float>>("channel_training", outf);
#define castv(T, v) vector<T>((T*)v.data(), (T*)(v.data() + v.size()))
    auto& refs = FastDSM_Emulator::refs = castv(cf, r2f);
    cf dc = cf(dcre,dcim);
    transform(sd1b, sd1b + demod_data_length,
            demod_data = new complex<float>[demod_data_length],
            [dc](cf x) { return x - dc; });

    encoder.compute_o_parameters();
    vector<Tag_Single_t> singles = FastDSM_Encoder::PQAMencode(data.data(), data.size(), encoder.bit_per_symbol);
    int singles_length = singles.size();

    // then emulate the best match signal
    emulator.initialize();
    vector< complex<float> > emulated = emulator.emulate(singles, target_length);
    if (target_length && emulated.size() > target_length) emulated.erase(emulated.begin() + target_length, emulated.end());
    mongodat.upload_record("emulated", (float*)emulated.data(), 2, emulated.size(), NULL, "emulated", 1/80., "time(ms)", 1, "I,Q");
    printf("upload emulated file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    record["emulated_id"] = MongoDat::OID2str(mongodat.get_fileID());

    record.save();

    mongodat.close();

    return 0;
}
