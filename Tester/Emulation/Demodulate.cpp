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
#define TAG_L4XX_SAMPLE_BYTE 32  // to enable NLCD = 32
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

vector<Tag_Single_t> get_nearest_points(int bit_per_symbol, int demod_nearest_count, double I, double Q) {
    int points_per_axis = 1 << (bit_per_symbol/2);
    double interval = 1. / (points_per_axis - 1);
    map<double, Tag_Single_t> candidates;
    for (int i=0; i<points_per_axis; ++i) {
        for (int j=0; j<points_per_axis; ++j) {
            Tag_Single_t single = 0;
            switch (bit_per_symbol) {
                case 8: single = mod_map4b[i] | (mod_map4b[j] << 4); break;
                case 6: single = mod_map3b[i] | (mod_map3b[j] << 4); break;
                case 4: single = mod_map2b[i] | (mod_map2b[j] << 4); break;
                case 2: single = mod_map1b[i] | (mod_map1b[j] << 4); break;
            }
            double distance_I = I - i*interval; distance_I *= distance_I;
            double distance_Q = Q - j*interval; distance_Q *= distance_Q;
            double distance = distance_I + distance_Q;
            candidates.insert(make_pair(distance, single));
        }
    }
    vector<Tag_Single_t> ret;
    int ret_length = candidates.size();
    if (ret_length > demod_nearest_count) ret_length = demod_nearest_count;
    auto it = candidates.begin();
    for (int i=0; i<ret_length; ++i, ++it) {
        ret.push_back(it->second);
    }
    return ret;
}

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
        printf("  demod_buffer_length: the maximum number of possible decode object\n");
        printf("  demod_nearest_count: this demodulation works as: do the same work in scatterplot, then find several nearst in the constellation diagram and compute their values\n");
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
    
    // BER_output_key
    string BER_output_key = "BER";
    if (record["BER_output_key"].existed() && record["BER_output_key"].type() == BSON_TYPE_UTF8) {
        BER_output_key = record["BER_output_key"].value<string>();
    } printf("BER_output_key: %s\n", BER_output_key.c_str());

    // refs
    if (encoder.channel_training_type != 0) {
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
        assert(metadata["NLCD"].existed() && metadata["NLCD"].type() == BSON_TYPE_INT32); assert(NLCD == metadata["NLCD"].value<int32_t>());
        assert(metadata["duty"].existed() && metadata["duty"].type() == BSON_TYPE_INT32); assert(duty == metadata["duty"].value<int32_t>());
        assert(metadata["cycle"].existed() && metadata["cycle"].type() == BSON_TYPE_INT32); assert(cycle == metadata["cycle"].value<int32_t>());
        assert(metadata["effect_length"].existed() && metadata["effect_length"].type() == BSON_TYPE_INT32); 
        assert(effect_length == metadata["effect_length"].value<int32_t>());
        assert(metadata["frequency"].existed() && metadata["frequency"].type() == BSON_TYPE_DOUBLE); assert(frequency == metadata["frequency"].value<double>());   
    }

    // demod_data
    assert(record["demod_data_id"].existed() && record["demod_data_id"].type() == BSON_TYPE_UTF8);
    string demod_data_id_str = record["demod_data_id"].value<string>();
    assert(MongoDat::isPossibleOID(demod_data_id_str.c_str()) && "demod_data_id invalid");
    bson_oid_t demod_data_id = MongoDat::parseOID(demod_data_id_str.c_str());
    vector<char> demod_data_binary = mongodat.get_binary_file(demod_data_id);
    int demod_data_length = demod_data_binary.size() / sizeof(complex<float>);
    // const complex<float>* demod_data_has_bias = (const complex<float>*)demod_data_binary.data();
    const complex<float>* sd1b = (const complex<float>*)demod_data_binary.data();
    
    // demod_buffer_length
    assert(record["demod_buffer_length"].existed() && record["demod_buffer_length"].type() == BSON_TYPE_INT32);
    int demod_buffer_length = record["demod_buffer_length"].value<int32_t>(); printf("demod_buffer_length: %d\n", demod_buffer_length);
    assert(demod_buffer_length > 0 && "invalid demod_buffer_length");

    // demod_nearest_count
    assert(record["demod_nearest_count"].existed() && record["demod_nearest_count"].type() == BSON_TYPE_INT32);
    int demod_nearest_count = record["demod_nearest_count"].value<int32_t>(); printf("demod_nearest_count: %d\n", demod_nearest_count);
    assert(demod_nearest_count > 0 && "invalid demod_nearest_count");
#if 0 
    // naive fix the bias problem
    int eval_Qzero_start = FastDSM_Emulator::bias + emulator.orate * 18e-3; assert(eval_Qzero_start > 0);
    int eval_Qzero_end = eval_Qzero_start + emulator.orate * 5e-3;
    int eval_Izero_start = FastDSM_Emulator::bias + emulator.orate * 30e-3;
    int eval_Izero_end = eval_Izero_start + emulator.orate * 5e-3;
    printf("eval_Qzero_start: %d (%f ms)\n", eval_Qzero_start, eval_Qzero_start/emulator.orate*1000);
    printf("eval_Qzero_end: %d (%f ms)\n", eval_Qzero_end, eval_Qzero_end/emulator.orate*1000);
    printf("eval_Izero_start: %d (%f ms)\n", eval_Izero_start, eval_Izero_start/emulator.orate*1000);
    printf("eval_Izero_end: %d (%f ms)\n", eval_Izero_end, eval_Izero_end/emulator.orate*1000);
    double eval_Qzero = 0;
    for (int i=eval_Qzero_start; i<eval_Qzero_end; ++i) eval_Qzero += demod_data_has_bias[i].imag();
    eval_Qzero /= (eval_Qzero_end - eval_Qzero_start); printf("eval_Qzero: %f\n", eval_Qzero);
    double eval_Izero = 0;
    for (int i=eval_Izero_start; i<eval_Izero_end; ++i) eval_Izero += demod_data_has_bias[i].real();
    eval_Izero /= (eval_Izero_end - eval_Izero_start); printf("eval_Izero: %f\n", eval_Izero);
    complex<float>* demod_data = new complex<float>[demod_data_length];
    complex<float> eval_zero_bias(eval_Izero, eval_Qzero);
    for (int i=0; i<demod_data_length; ++i) demod_data[i] = demod_data_has_bias[i] - eval_zero_bias;
#endif

    // compute the bias of emulation (basically preamble and channel training)
    encoder.build_preamble();
    encoder.build_channel_training();
    printf("encoder.o_preamble.size(): %d\n", (int)encoder.o_preamble.size());
    printf("encoder.o_channel_training.size(): %d\n", (int)encoder.o_channel_training.size());
    FastDSM_Emulator::bias += (encoder.o_preamble.size() + encoder.o_channel_training.size()) * emulator.orate / emulator.frequency;
    printf("recomputed bias: %d\n", FastDSM_Emulator::bias);
    int channel_training_data_length = encoder.o_channel_training.size() * emulator.orate / emulator.frequency;
	const complex<float>* channel_training_data = sd1b + FastDSM_Emulator::bias - channel_training_data_length;
	complex<float>* demod_data = NULL;
    if (encoder.channel_training_type == 0) {  // get new references from purui's server
long channel_training_start = get_time_now();
		using cf=complex<float>;
		// mongodat.upload_record("channel_training_data", (float*)channel_training_data, 2, channel_training_data_length, NULL, "channel_training_data", 1/80., "time(ms)", 1, "I,Q");
		// printf("upload channel_training_data file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
		// TODO pass to your program
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
// #define upc(v) do { mongodat.upload_record(#v, (float*)v.data(), 2, v.size(), NULL, #v, 1/80., "time(ms)", 1, "I,Q"); printf("upload "#v" file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str()); } while (0)
// 		upc(refs);
		
	  cf dc = cf(dcre,dcim);
		transform(sd1b, sd1b + demod_data_length,
				demod_data = new complex<float>[demod_data_length],
				[dc](cf x) { return x - dc; });
long channel_training_end = get_time_now();
printf("BENCHMARK: channel training consumes %d us\n", (int)(channel_training_end-channel_training_start));
    } else if (encoder.channel_training_type == 1) {
        if (encoder.ct_fast != 0) {
            // TODO: not debug yet
            double ct_fast_duration = encoder.ct_fast / emulator.frequency;
            int ct_fast_length = ct_fast_duration * emulator.orate;
            printf("ct_fast: %d (%f ms)\n", ct_fast_length, ct_fast_duration * 1000);
    #define CHANNEL_TRAINING_FAST_EDGE_TIME 0.4e-3
            int ct_bias_length = CHANNEL_TRAINING_FAST_EDGE_TIME * emulator.orate;
            assert(ct_fast_length > ct_bias_length && "channel training not long enough for collect stable amplitude");
            vector< complex<float> > ct_amplitudes; ct_amplitudes.resize(encoder.o_dsm_order * 2);
            complex<float> base;
            for (int i=0; i<encoder.o_dsm_order; ++i) {
                int ct_collect_start_0x0F = ct_bias_length + 2*i*ct_fast_length;
                int ct_collect_end_0x0F = ct_collect_start_0x0F - ct_bias_length + ct_fast_length;
                int ct_collect_start_0xFF = ct_collect_start_0x0F + ct_fast_length;
                int ct_collect_end_0xFF = ct_collect_start_0xFF - ct_bias_length + ct_fast_length;
                complex<float> ct_amplitudes_0x0F, ct_amplitudes_0xFF;
                for (int j=ct_collect_start_0x0F; j<ct_collect_end_0x0F; ++j) ct_amplitudes_0x0F += channel_training_data[j];
                ct_amplitudes_0x0F /= (ct_collect_end_0x0F - ct_collect_start_0x0F);
                for (int j=ct_collect_start_0xFF; j<ct_collect_end_0xFF; ++j) ct_amplitudes_0xFF += channel_training_data[j];
                ct_amplitudes_0xFF /= (ct_collect_end_0xFF - ct_collect_start_0xFF);
                ct_amplitudes[2*i] = ct_amplitudes_0x0F - base;
                ct_amplitudes[2*i+1] = ct_amplitudes_0xFF - ct_amplitudes_0x0F;
                base = ct_amplitudes_0xFF;
            }
            for (int i=0; i<(int)ct_amplitudes.size(); ++i) {
                printf("%d: %f+%fi\n", i, ct_amplitudes[i].real(), ct_amplitudes[i].imag());
            }
            // then rotate each reference piece to this one, first need to evaluate the reference signal strength
            int single_cycle_length = cycle * emulator.orate / frequency;
            int single_ref_length = single_cycle_length * ((1 << effect_length) - 1);
            int collect_length = (duty * emulator.orate / frequency) - ct_bias_length;
            assert(collect_length > 0 && "duty is too small to collect stable amplitude");
            vector< complex<float> > ref_amplitudes; ref_amplitudes.resize(encoder.o_dsm_order * 2);
            vector< complex<float> > new_refs = FastDSM_Emulator::refs;  // copy the reference out
            for (int i=0; i<encoder.o_dsm_order; ++i) {
                complex<float> ref_amplitudes_0x0F, ref_amplitudes_0xF0;
                for (int j=0; j<encoder.combine; ++j) {
                    int lcd_idx = i + j * encoder.o_dsm_order;
                    complex<float> single_0x0F, single_0xF0;
                    assert(FastDSM_Emulator::effect_length == 3 && "only support this config");
                    const complex<float>* refs_part_0F = FastDSM_Emulator::refs.data() + lcd_idx * 2 * single_ref_length + 6 * single_cycle_length + ct_bias_length;
                    const complex<float>* refs_part_F0 = refs_part_0F + single_ref_length;
                    for (int k=0; k<collect_length; ++k) {
                        single_0x0F += refs_part_0F[k];
                        single_0xF0 += refs_part_F0[k];
                    }
                    ref_amplitudes_0x0F += single_0x0F / (float)collect_length;
                    ref_amplitudes_0xF0 += single_0xF0 / (float)collect_length;
                }
                ref_amplitudes[2*i] = ref_amplitudes_0x0F;
                ref_amplitudes[2*i+1] = ref_amplitudes_0xF0;
                // then recover the reference signal to its new amplitude
                complex<float> ratio_0x0F = ref_amplitudes[2*i] / ct_amplitudes[2*i];
                complex<float> ratio_0xF0 = ref_amplitudes[2*i+1] / ct_amplitudes[2*i+1];
                for (int j=0; j<encoder.combine; ++j) {
                    int lcd_idx = i + j * encoder.o_dsm_order;
                    int start_0x0F = lcd_idx * 2 * single_ref_length;
                    int start_0xF0 = start_0x0F + single_ref_length;
                    for (int k=0; k<single_ref_length; ++k) {
                        new_refs[start_0x0F+k] *= ratio_0x0F;
                        new_refs[start_0xF0+k] *= ratio_0xF0;
                    }
                }
            }
            for (int i=0; i<(int)ref_amplitudes.size(); ++i) {
                printf("%d: %f+%fi\n", i, ref_amplitudes[i].real(), ref_amplitudes[i].imag());
            }
            // mongodat.upload_record("refs", (float*)FastDSM_Emulator::refs.data(), 2, FastDSM_Emulator::refs.size(), NULL, "refs", 1/80., "time(ms)", 1, "I,Q");
            // printf("upload refs file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
            FastDSM_Emulator::refs = new_refs;  // copy back
            // mongodat.upload_record("refs", (float*)FastDSM_Emulator::refs.data(), 2, FastDSM_Emulator::refs.size(), NULL, "refs", 1/80., "time(ms)", 1, "I,Q");
            // printf("upload refs file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
        } else {
            demod_data = (complex<float>*)sd1b;
        }
    }

    if (record["evaluate_V"].existed() && record["evaluate_V"].type() == BSON_TYPE_INT32) {
        int evaluate_V = record["evaluate_V"].value<int32_t>();
        // here I want to evaluate the effect of V, by copy the reference file properly
        printf("FastDSM_Emulator::refs.size(): %d\n", (int)FastDSM_Emulator::refs.size());
        int data_cycle_length = cycle * 80000 / frequency;
        int m_sequence_length = (1 << effect_length) - 1;
        int data_length_should_be = m_sequence_length * data_cycle_length;
        assert(evaluate_V == 1 || evaluate_V == 2 || evaluate_V == 3);
        for (int i=0; i<2*NLCD; ++i) {
            int bias_i = data_length_should_be * i;
#define copy_single_piece_ref(to, from) for (int k=0; k<data_cycle_length; ++k) FastDSM_Emulator::refs[to+k] = FastDSM_Emulator::refs[from+k]
            if (evaluate_V == 2) {
                // 0010111   0123456 -> 0120266
                copy_single_piece_ref(bias_i + 3 * data_cycle_length, bias_i + 0 * data_cycle_length);
                copy_single_piece_ref(bias_i + 4 * data_cycle_length, bias_i + 2 * data_cycle_length);
                copy_single_piece_ref(bias_i + 5 * data_cycle_length, bias_i + 6 * data_cycle_length);
            } else if (evaluate_V == 1) {
                // 0010111   0123456 -> 1161666
                copy_single_piece_ref(bias_i + 0 * data_cycle_length, bias_i + 1 * data_cycle_length);
                copy_single_piece_ref(bias_i + 2 * data_cycle_length, bias_i + 6 * data_cycle_length);
                copy_single_piece_ref(bias_i + 3 * data_cycle_length, bias_i + 1 * data_cycle_length);
                copy_single_piece_ref(bias_i + 4 * data_cycle_length, bias_i + 6 * data_cycle_length);
                copy_single_piece_ref(bias_i + 5 * data_cycle_length, bias_i + 6 * data_cycle_length);
            }
        }
    }

long demodulate_start = get_time_now();
    encoder.compute_o_parameters();
    vector<Tag_Single_t> singles = FastDSM_Encoder::PQAMencode(data.data(), data.size(), encoder.bit_per_symbol);
    int singles_length = singles.size();
    // demodulation will not use data nor singles

    emulator.sanity_check();  // this will check the size of reference file
    emulator.initialize();

    int single_slot_length = cycle * emulator.orate / frequency / (NLCD/combine);
    assert(demod_data_length >= FastDSM_Emulator::bias + singles_length * single_slot_length);
    int start = FastDSM_Emulator::bias;
    printf("start: %d (%f ms)\n", start, start/80.);

    // this is the basic data sturcture used for decode
    struct DecodeBranch {
        double segma;
        vector<Tag_Single_t> dec;  // this is the history result
        FastDSM_Emulator emu;
        DecodeBranch(const FastDSM_Emulator& e) {
            segma = 0; emu = e;
        }
        // the new_value will be pushed to dec after origin
        DecodeBranch(double segma_, const FastDSM_Emulator& e, const vector<Tag_Single_t>& origin, Tag_Single_t new_value) {
            segma = segma_;
            dec = origin;
            dec.push_back(new_value);
            emu = e;
        }
        bool operator< (const DecodeBranch &obj) const {
            return segma < obj.segma;
        }
    };
    set<DecodeBranch> branches;  // this is all branches active, which has maximum length of demod_buffer_length
    branches.insert(DecodeBranch(emulator));
    for (int i=0; i<singles_length; ++i) {
    // for (int i=0; i<1; ++i) {  // for debug
        set<DecodeBranch> new_branches;
        for (auto bt = branches.begin(); bt != branches.end(); ++bt) {
            FastDSM_Emulator tmp_emu_0 = bt->emu;
            FastDSM_Emulator tmp_emu_0F = bt->emu;
            FastDSM_Emulator tmp_emu_F0 = bt->emu;
            // construct if next symbol is 0, what happens
            vector< complex<float> > out_segment_0 = tmp_emu_0.push(0);
            // construct if next symbol is 0x0F, what happens
            vector< complex<float> > ref_0F = tmp_emu_0F.push(bit_per_symbol == 6 ? 0x07 : 0x0F);
            // construct if next symbol is 0xF0, what happens
            vector< complex<float> > ref_F0 = tmp_emu_F0.push(bit_per_symbol == 6 ? 0x70 : 0xF0);
            // get the delta of 0x0F and 0x00, 0xF0 and 0x00
            for (int j=0; j<(int)out_segment_0.size(); ++j) ref_0F[j] -= out_segment_0[j];
            for (int j=0; j<(int)out_segment_0.size(); ++j) ref_F0[j] -= out_segment_0[j];
            // get the delta between real experiment data and tmp_emu_0
            for (int j=0; j<(int)out_segment_0.size(); ++j) out_segment_0[j] = demod_data[start+j] - out_segment_0[j];  // NOTE: out_segment_0 is changed here !!!
            // trying to solve the equation I * ref_0F + Q * ref_F0 == out_segment_0
            // or to say, min(sum(|I * ref_0F + Q * ref_F0 - out_segment_0|^2))
            // let f(x) = ref_0F, g(x) = ref_F0, y(x) = out_segment_0
            // is:
            // I * (2*f(x)f'(x)) + Q * (f(x)g'(x)+f'(x)g(x)) == f(x)y'(x)+f'(x)y(x)
            // I * (f(x)g'(x)+f'(x)g(x)) + Q * (2*g(x)g'(x)) == g(x)y'(x)+g'(x)y(x)
            // also:
            // I * 2A + Q * 2B == 2C
            // I * 2B + Q * 2D == 2E
            double A = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) { float re = ref_0F[j].real(); float im = ref_0F[j].imag(); A += re*re + im*im; }
            double B = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) { B += ref_0F[j].real() * ref_F0[j].real() + ref_0F[j].imag() * ref_F0[j].imag(); }
            double C = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) { C += ref_0F[j].real() * out_segment_0[j].real() + ref_0F[j].imag() * out_segment_0[j].imag(); }
            double D = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) { float re = ref_F0[j].real(); float im = ref_F0[j].imag(); D += re*re + im*im; }
            double E = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) { E += ref_F0[j].real() * out_segment_0[j].real() + ref_F0[j].imag() * out_segment_0[j].imag(); }
            double I = (E*B - C*D) / (B*B - A*D);
            double Q = (C*B - E*A) / (B*B - A*D);
            // printf("I: %f, Q: %f\n", I, Q);
            vector<Tag_Single_t> nearest_points = get_nearest_points(bit_per_symbol, demod_nearest_count, I, Q);
            // for (int j=0; j<(int)nearest_points.size(); ++j) {
            //     printf("0x%02X, ", nearest_points[j]);
            // } printf("\n");
            // then for each nearest point, compute the result
            for (int j=0; j<(int)nearest_points.size(); ++j) {
                FastDSM_Emulator tmp_emu = bt->emu;
                vector< complex<float> > out_segment = tmp_emu.push(nearest_points[j]);
                double segma = 0;
                for (int k=0; k<(int)out_segment.size(); ++k) {
                    complex<float> diff = demod_data[start+k] - out_segment[k];
                    segma += diff.real() * diff.real() + diff.imag() * diff.imag();
                }
                // push this branch to current set
                new_branches.insert(DecodeBranch(bt->segma + segma, tmp_emu, bt->dec, nearest_points[j]));
            }
        }
        branches.clear();
        int new_branch_length = new_branches.size();
        if (new_branch_length > demod_buffer_length) new_branch_length = demod_buffer_length;
        auto bt = new_branches.begin();
        for (int j=0; j<new_branch_length; ++j, ++bt) {
            branches.insert(*bt);
        }
        // finally, the branches has no more than demod_buffer_length elements
        start += single_slot_length;
    }
long demodulate_end = get_time_now();
printf("BENCHMARK: demodulate consumes %d us\n", (int)(demodulate_end-demodulate_start));

    // printf("original singles:\n");
    // for (int i=0; i<(int)singles.size(); ++i) {
    //     printf("%02X ", singles[i]);
    // } printf("\n");

    // printf("final branches:\n");
    // for (auto bt=branches.begin(); bt != branches.end(); ++bt) {
    //     printf("%f: ", bt->segma);
    //     for (int j=0; j<(int)bt->dec.size(); ++j) {
    //         printf("%02X ", bt->dec[j]);
    //     } printf("\n");
    // }
    
    int print_branches = branches.size();
    if (print_branches > 5) print_branches = 5;
    printf("top %d branches:\n", print_branches);
    auto pbt=branches.begin();
    for (int i=0; i<print_branches; ++i, ++pbt) {
        printf("%f\n", pbt->segma);
    }

    // use the first one as decoded result
    vector<Tag_Single_t> decoded_singles = branches.begin()->dec;
    for (int i=0; i<(int)decoded_singles.size(); ++i) {
        printf("%02X ", decoded_singles[i]);
    } printf("\n");
    vector<uint8_t> decoded = FastDSM_Encoder::PQAMdecode(decoded_singles, data.size(), bit_per_symbol);

    printf("origin: %s\n", MongoDat::dump(data).c_str());
    printf("decode: %s\n", MongoDat::dump(decoded).c_str());

	double BER = 0;
	for (int i=0; i<(int)data.size(); ++i) {
        for (int j=0; j<8; ++j) {
		    if ((decoded[i] ^ data[i]) & (1 << j)) ++BER;
        }
	}
	BER /= data.size() * 8;
	printf("BER: %f %%\n", BER * 100);

    record["decoded"] = MongoDat::dump(decoded);
    record[BER_output_key.c_str()] = BER;
    record.save();

    mongodat.close();

    return 0;
}
