/*
 * This program is used to analyze the performance of real experiment / emulation
 *   with data pre-known, just compute the max-likelihood one (not precise enough, since only use the 001 reference)
 * the output is pretty like QAM constellation diagram
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
        printf("  scatter_data_id: the result of emulation or real experiment, to be analyzed\n");
        return -1;
    }

	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
    bool with_dc = 1;  // of course there should be DC, because we mainly focus on it
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

    // scatter_data
    assert(record["scatter_data_id"].existed() && record["scatter_data_id"].type() == BSON_TYPE_UTF8);
    string scatter_data_id_str = record["scatter_data_id"].value<string>();
    assert(MongoDat::isPossibleOID(scatter_data_id_str.c_str()) && "scatter_data_id invalid");
    bson_oid_t scatter_data_id = MongoDat::parseOID(scatter_data_id_str.c_str());
    vector<char> scatter_data_binary = mongodat.get_binary_file(scatter_data_id);
    int scatter_data_length = scatter_data_binary.size() / sizeof(complex<float>);
	const complex<float>* sd1b = (const complex<float>*)scatter_data_binary.data();
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
	for (int i=eval_Qzero_start; i<eval_Qzero_end; ++i) eval_Qzero += sd1b[i].imag();
	eval_Qzero /= (eval_Qzero_end - eval_Qzero_start); printf("eval_Qzero: %f\n", eval_Qzero);
	double eval_Izero = 0;
	for (int i=eval_Izero_start; i<eval_Izero_end; ++i) eval_Izero += sd1b[i].real();
    eval_Izero /= (eval_Izero_end - eval_Izero_start); printf("eval_Izero: %f\n", eval_Izero);
    complex<float>* scatter_data = new complex<float>[scatter_data_length];
    complex<float> eval_zero_bias(eval_Izero, eval_Qzero);
    for (int i=0; i<scatter_data_length; ++i) {
		complex<float> dat = sd1b[i] - eval_zero_bias;
		scatter_data[i] = complex<float>(dat.real(), dat.imag());
	}		 
#endif

    // compute the bias of emulation (basically preamble and channel training)
    encoder.build_preamble();
    encoder.build_channel_training();
    FastDSM_Emulator::bias += (encoder.o_preamble.size() + encoder.o_channel_training.size()) * emulator.orate / emulator.frequency;
    printf("recomputed bias: %d\n", FastDSM_Emulator::bias);
    int channel_training_data_length = encoder.o_channel_training.size() * emulator.orate / emulator.frequency;
	const complex<float>* channel_training_data = sd1b + FastDSM_Emulator::bias - channel_training_data_length;
	complex<float>* scatter_data = NULL;
    if (encoder.channel_training_type == 0) {  // get new references from purui's server
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
		transform(sd1b, sd1b + scatter_data_length,
				scatter_data = new complex<float>[scatter_data_length],
				[dc](cf x) { return x - dc; });
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
            scatter_data = (complex<float>*)sd1b;
        }
    }

    encoder.compute_o_parameters();
    vector<Tag_Single_t> singles = FastDSM_Encoder::PQAMencode(data.data(), data.size(), encoder.bit_per_symbol);
    emulator.sanity_check();  // this will check the size of reference file
    emulator.initialize();
    vector< complex<float> > scatter_output;
    int single_slot_length = cycle * emulator.orate / frequency / (NLCD/combine);
    assert(scatter_data_length >= FastDSM_Emulator::bias + (int)singles.size() * single_slot_length);

    // fix the bias doesn't really solve the problem, so I wanna explore how it can work well with scaling?
    // basically, what we assume is that the result of reference collection could be added together, but now without any reason this would failed, I would try to "fix" this issue...
    // {
    //     int L = NLCD / combine;  // there is L scaling parameters we would solve
    //     vector< vector< complex<float> > > singlewaves; singlewaves.resize(L);
    //     vector< vector< complex<float> > > try_refs; try_refs.resize(L);
    //     const vector< complex<float> > original_refs = FastDSM_Emulator::refs;
    //     int single_cycle_length = cycle * emulator.orate / frequency;
    //     int single_ref_length = single_cycle_length * ((1 << effect_length) - 1);
    //     for (int i=0; i<L; ++i) {  // try to get the emulated result by only remain the reference of those LCDs
    //         try_refs[i].resize(original_refs.size());
    //         for (int j=0; j<combine; ++j) {
    //             int lcd_idx = i + j * L; //duo-8421 LCD index
    //             int dat_idx = lcd_idx * 2 * single_ref_length;
    //             for (int k=0; k<2*single_ref_length; ++k) {
    //                 try_refs[i][dat_idx + k] = original_refs[dat_idx + k];
    //             }
    //         }
    //         // mongodat.upload_record("try_refs", (float*)try_refs[i].data(), 2, try_refs[i].size(), NULL, "try_refs", 1/80., "time(ms)", 1, "I,Q");
    //         // printf("upload try_refs file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    //         FastDSM_Emulator::refs = try_refs[i];
    //         FastDSM_Emulator tmp_emulator;
    //         tmp_emulator.initialize();
    //         singlewaves[i] = tmp_emulator.emulate(singles);
    //         // mongodat.upload_record("singlewave", (float*)singlewaves[i].data(), 2, singlewaves[i].size(), NULL, "singlewave", 1/80., "time(ms)", 1, "I,Q");
    //         // printf("upload singlewave file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    //     }
    //     FastDSM_Emulator::refs = original_refs;  // must recover !!!

    //     // next, the question becomes, given L coefficients, how to maximize the likelihood to real one?
    //     // for example, L = 2
    //     // min( | x0 * singlewaves[0](t) + x1 * singlewaves[1](t) - real(t) |^2 )
    //     vector< vector<double> > cross_coeff; cross_coeff.resize(L);
    //     for (int i=0; i<L; ++i) {  // compute singlewaves[i] X singlewaves[j]
    //         cross_coeff[i].resize(L);
    //         // printf("cross_coeff[%d]: ", i);
    //         for (int j=0; j<L; ++j) {
    //             double coeff = 0;
    //             const vector< complex<float> >& s1 = singlewaves[i];
    //             const vector< complex<float> >& s2 = singlewaves[j];
    //             for (int k=0; k<(int)s1.size(); ++k) {
    //                 coeff += s1[k].real() * s2[k].real() + s1[k].imag() * s2[k].imag();
    //             }
    //             cross_coeff[i][j] = coeff;
    //             // printf("%f, ", coeff);
    //         }
    //         // printf("\n");
    //     }
    //     vector<double> para_coeff; para_coeff.resize(L);  // compute singlewaves[i] X real
    //     for (int i=0; i<L; ++i) {
    //         double coeff = 0;
    //         const vector< complex<float> >& s = singlewaves[i];
    //         const complex<float>* real = scatter_data + FastDSM_Emulator::bias;
    //         for (int k=0; k<(int)s.size(); ++k) {
    //             coeff += s[k].real() * real[k].real() + s[k].imag() * real[k].imag();
    //         }
    //         para_coeff[i] = coeff;
    //         // printf("para_coeff[%d]: %f\n", i, coeff);
    //     }
    //     // then try to solve the equation
    //     vector<double> scales;  scales.resize(L);
    //     // for i in 0..L:   Sum( for j in 0..L,  scales[j] * cross_coeff[i][j] ) == para_coeff[i]
    //     // since scales[j] is around 1, use simply method to solve equation
    //     for (int i=0; i<L-1; ++i) {
    //         for (int j=i+1; j<L; ++j) {
    //             double ratio = cross_coeff[j][i] / cross_coeff[i][i];
    //             for (int k=i; k<L; ++k) {
    //                 cross_coeff[j][k] -= cross_coeff[i][k] * ratio;
    //             }
    //             para_coeff[j] -= ratio * para_coeff[i];
    //         }
    //     }
    //     for (int i=1; i<L; ++i) {
    //         for (int j=0; j<i; ++j) {
    //             double ratio = cross_coeff[j][i] / cross_coeff[i][i];
    //             for (int k=i; k<L; ++k) {
    //                 cross_coeff[j][k] -= cross_coeff[i][k] * ratio;
    //             }
    //             para_coeff[j] -= ratio * para_coeff[i];
    //         }
    //     }
    //     // for (int i=0; i<L; ++i) { for (int j=0; j<L; ++j) { printf("%f, ", cross_coeff[i][j]); } printf(" = %f\n", para_coeff[i]); }  // show the matrix
    //     for (int i=0; i<L; ++i) {
    //         scales[i] = para_coeff[i] / cross_coeff[i][i];
    //         printf("scales[%d]: %f\n", i, scales[i]);
    //     }
    //     if (0) {  // modify the original reference signal
    //         for (int i=0; i<(int)FastDSM_Emulator::refs.size(); ++i) {
    //             complex<float> modified;
    //             for (int k=0; k<L; ++k) {
    //                 modified += (float)scales[k] * try_refs[k][i];
    //             }
    //             FastDSM_Emulator::refs[i] = modified;
    //         }
    //         mongodat.upload_record("refs", (float*)FastDSM_Emulator::refs.data(), 2, FastDSM_Emulator::refs.size(), NULL, "refs", 1/80., "time(ms)", 1, "I,Q");
    //         printf("upload refs file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    //     }
    // }

    int start = FastDSM_Emulator::bias;
    printf("start: %d (%f ms)\n", start, start/80.);
    for (int i=0; i<(int)singles.size(); ++i) {
// for (int i=0; i<1; ++i) {
        FastDSM_Emulator tmp_emu_0 = emulator;
        FastDSM_Emulator tmp_emu_0F = emulator;
        FastDSM_Emulator tmp_emu_F0 = emulator;
        vector< complex<float> > out_segment_0 = tmp_emu_0.push(0);
        vector< complex<float> > ref_0F = tmp_emu_0F.push(bit_per_symbol == 6 ? 0x07 : 0x0F);
        for (int j=0; j<(int)out_segment_0.size(); ++j) ref_0F[j] -= out_segment_0[j];
// mongodat.upload_record("ref_0F", (float*)ref_0F.data(), 2, ref_0F.size(), NULL, "ref_0F", 1/80., "time(ms)", 1, "I,Q");
// printf("upload ref_0F file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
        vector< complex<float> > ref_F0 = tmp_emu_F0.push(bit_per_symbol == 6 ? 0x70 : 0xF0);
        for (int j=0; j<(int)out_segment_0.size(); ++j) ref_F0[j] -= out_segment_0[j];
// mongodat.upload_record("ref_F0", (float*)ref_F0.data(), 2, ref_F0.size(), NULL, "ref_F0", 1/80., "time(ms)", 1, "I,Q");
// printf("upload ref_F0 file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
        for (int j=0; j<(int)out_segment_0.size(); ++j) {  // disable the previous bit influence
            out_segment_0[j] = scatter_data[start+j] - out_segment_0[j];
        }
// mongodat.upload_record("out_segment_0", (float*)out_segment_0.data(), 2, out_segment_0.size(), NULL, "out_segment_0", 1/80., "time(ms)", 1, "I,Q");
// printf("upload out_segment_0 file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
        // now we've got the reference of 2 ways and the ultimate waveform
        // trying to solve the equation I * ref_0F + Q * ref_F0 == out_segment_0
        // or to say, min(sum(|I * ref_0F + Q * ref_F0 - out_segment_0|^2))
        // let f(x) = ref_0F, g(x) = ref_F0, y(x) = out_segment_0
        if (with_dc) {
            // I * (2*f(x)f'(x)) + Q * (f(x)g'(x)+f'(x)g(x)) == f(x)y'(x)+f'(x)y(x)
            // I * (f(x)g'(x)+f'(x)g(x)) + Q * (2*g(x)g'(x)) == g(x)y'(x)+g'(x)y(x)
            // aka:
            // I * 2A + Q * 2B == 2C
            // I * 2B + Q * 2D == 2E
            double A = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) {
                float re = ref_0F[j].real(); float im = ref_0F[j].imag(); A += re*re + im*im;
            }
            double B = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) {
                B += ref_0F[j].real() * ref_F0[j].real() + ref_0F[j].imag() * ref_F0[j].imag();
            }
            double C = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) {
                C += ref_0F[j].real() * out_segment_0[j].real() + ref_0F[j].imag() * out_segment_0[j].imag();
            }
            double D = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) {
                float re = ref_F0[j].real(); float im = ref_F0[j].imag(); D += re*re + im*im;
            }
            double E = 0; for (int j=0; j<(int)out_segment_0.size(); ++j) {
                E += ref_F0[j].real() * out_segment_0[j].real() + ref_F0[j].imag() * out_segment_0[j].imag();
            }
            double I = (E*B - C*D) / (B*B - A*D);
            double Q = (C*B - E*A) / (B*B - A*D);
            scatter_output.push_back(complex<float>(I, Q));
        } else {
            assert(0 && "not implemented");
        }
        emulator.push(singles[i]);  // add the correct data here
        start += single_slot_length;
    }
    mongodat.upload_scatter("scatter_output", (float*)scatter_output.data(), scatter_output.size(), NULL, "scatter_output", 1, "x", 1, "y");
    printf("upload scatter_output file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    record["scatter_output_id"] = MongoDat::OID2str(mongodat.get_fileID());
    
    mongodat.upload_record("ctd_refs_output", (float*)FastDSM_Emulator::refs.data(), 2, FastDSM_Emulator::refs.size(), NULL, "refs", 1/80., "time(ms)", 1, "I,Q");
    printf("upload refs file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    record["ctd_refs_output_id"] = MongoDat::OID2str(mongodat.get_fileID());

    record.save();

    mongodat.close();

    return 0;
}
