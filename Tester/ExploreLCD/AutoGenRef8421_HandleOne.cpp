/* 
 * Output is a one-dimensional complex<float> array
 * which is actually complex<float> ref[idx][cycle*orate/frequency]
 * where idx is 0~2^effect_length
 */

#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
#include "modulator.h"
#include "soxr/soxr.h"
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

vector< complex<float> > get_calibrated(const vector<char> binary);

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 4) {
		printf("usage: <collection> <id(12 byte hex = 24 char)> <LCD_idx:int>\n");
        printf("the document should have the following values:\n");
        printf("  NLCD: at most 16, which has 16x8=128 independent pixels\n");
        printf("  frequency: the frequency to drive each LCD pixel, this should not be more than 250Hz when voltage is 8V, experimentally\n");
        printf("  effect_length: if frequency is under 125Hz, effect_length can be 1, otherwise consider add more.\n");
        printf("  o_repeat_cnt: for every reference, repeat it for multiple times to average\n\n");
        printf("the reference collecting process is described below:\n");
#define MAX_PACKET_TIME_S 2.0
        printf("  first, due to clock accuracy (10ppm), each packet should not exceed 2s (80kS/s)\n");
        printf("  in each packet, LCD send 0x0F, 0xF0\n");
		return -1;
	}

	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
    int LCD_idx = atoi(argv[3]);
    
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);

    // NLCD
    assert(record["NLCD"].existed() && record["NLCD"].type() == BSON_TYPE_INT32);
    int NLCD = record["NLCD"].value<int32_t>();
    printf("NLCD: %d\n", NLCD);
    assert(NLCD > 0 && NLCD <= 16 && "invalid NLCD");

    // frequency
    assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
    double frequency = record["frequency"].value<double>();
    printf("frequency: %f\n", frequency);

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
    assert(effect_length > 0 && effect_length <= 16 && "16 means 2^16 possible cases which is impossible to finish in reasonable time");
    
    // repeat_cnt
    assert(record["repeat_cnt"].existed() && record["repeat_cnt"].type() == BSON_TYPE_INT32);
    int repeat_cnt = record["repeat_cnt"].value<int32_t>();
    printf("repeat_cnt: %d\n", repeat_cnt);
    assert(repeat_cnt > 0 && repeat_cnt <= 10000 && "should it exceed 10000? modify code if REALLY necessary");

    // o_repeat_cnt
    assert(record["o_repeat_cnt"].existed() && record["o_repeat_cnt"].type() == BSON_TYPE_INT32);
    int o_repeat_cnt = record["o_repeat_cnt"].value<int32_t>();
    printf("o_repeat_cnt: %d\n", o_repeat_cnt);
    assert(o_repeat_cnt > 0 && o_repeat_cnt <= 10000 && "should it exceed 10000? modify code if REALLY necessary");

    // first resampling them from 56.875k to 80k
    char record_id_key[32];
    sprintf(record_id_key, "record_id_%d", LCD_idx);
    char ref_id_key[32];
    sprintf(ref_id_key, "ref_id_%d", LCD_idx);
    assert(record[record_id_key].existed() && record[record_id_key].type() == BSON_TYPE_UTF8 && "record_id needed");
	string binary_id_str = record[record_id_key].value<string>();
	assert(MongoDat::isPossibleOID(binary_id_str.c_str()) && "record_id invalid");
	bson_oid_t record_id = MongoDat::parseOID(binary_id_str.c_str());
	vector<char> binary = mongodat.get_binary_file(record_id);

    // get calibrated signal with time alignment
    vector< complex<float> > signal = get_calibrated(binary);
	// mongodat.upload_record("signal", (float*)signal.data(), 2, signal.size(), NULL, "signal", 1/80., "time(ms)", 1, "I,Q");
	// printf("upload signal with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

    FastDSM_Encoder encoder;
    encoder.NLCD = NLCD;
    encoder.frequency = frequency;
    encoder.build_preamble();
    vector<bool> m_sequence = generate_m_sequence(effect_length);
    printf("m_sequence size: %d\n", (int)m_sequence.size());
    int m_sequence_cnt_each_packet = MAX_PACKET_TIME_S * frequency / cycle / m_sequence.size();
    printf("m_sequence_cnt_each_packet: %d\n", m_sequence_cnt_each_packet);
    int each_packet_repeat = (repeat_cnt + m_sequence_cnt_each_packet - 1) / m_sequence_cnt_each_packet;
    printf("each_packet_repeat: %d\n", each_packet_repeat);
    assert(each_packet_repeat == 1 && "otherwise not supported");

    double orate = 80000;
    int preamble_length = encoder.o_preamble.size() * orate / frequency;
    printf("preamble_length: %d\n", preamble_length);
    int m_sequence_cycle_length = m_sequence.size() * orate * cycle / frequency;
    printf("m_sequence_cycle_length: %d (%f ms)\n", m_sequence_cycle_length, m_sequence_cycle_length/orate*1e3);
    int I_data_start = preamble_length - 32 * orate / 8000;  // copy from 
    printf("I_data_start at %d (%f ms)\n", I_data_start, I_data_start/orate*1e3);
    int Q_data_start = I_data_start + m_sequence_cycle_length * (o_repeat_cnt + 1) + preamble_length;
    printf("Q_data_start at %d (%f ms)\n", Q_data_start, Q_data_start/orate*1e3);

    // reference format
    uint32_t m_sequence_bit_rep = 0;
    for (size_t i=0; i<m_sequence.size(); ++i) {
        m_sequence_bit_rep |= m_sequence[i] << (i);
        m_sequence_bit_rep |= m_sequence[i] << (i + m_sequence.size());
    }
    vector<int> base;
    base.resize(1 << effect_length);
    for (int i=1; i<(1<<effect_length); ++i) {
        for (int j=0; j<(int)m_sequence.size(); ++j) {
            bool equal = true;
            for (int k=0; k<effect_length; ++k) {
                if ((((m_sequence_bit_rep >> j) >> k) ^ (i >> (effect_length-k-1))) & 1) equal = false;
            }
            if (equal) {
                printf("%d: %d, ", i, j+effect_length-1);
                assert(base[i] == 0 && "should not happen, otherwise is not m-sequence");
                base[i] = (j+effect_length-1) * cycle * orate / frequency;
                printf("%d (%f ms)\n", base[i], base[i]/orate*1e3);
            }
        }
    }

    vector< complex<float> > refs;
    int single_ref_length = orate * cycle / frequency;
    refs.insert(refs.end(), single_ref_length, 0);
    for (int i=1; i<(1<<effect_length); ++i) {
        int base_i = I_data_start + base[i];
        vector< complex<float> > ref;
        ref.resize(single_ref_length);
        for (int j=0; j<o_repeat_cnt; ++j) {
            int base_j = base_i + j * m_sequence_cycle_length;
            // printf("%d (%f ms)\n", base_j, base_j/orate*1e3);
            for (int k=0; k<single_ref_length; ++k) {
                ref[k] += signal[base_j + k];
            }
        }
        for (int k=0; k<single_ref_length; ++k) {
            ref[k] /= o_repeat_cnt;
        }
        refs.insert(refs.end(), ref.begin(), ref.end());
    }
    refs.insert(refs.end(), single_ref_length, 0);
    for (int i=1; i<(1<<effect_length); ++i) {
        int base_i = Q_data_start + base[i];
        vector< complex<float> > ref;
        ref.resize(single_ref_length);
        for (int j=0; j<o_repeat_cnt; ++j) {
            int base_j = base_i + j * m_sequence_cycle_length;
            // printf("%d (%f ms)\n", base_j, base_j/orate*1e3);
            for (int k=0; k<single_ref_length; ++k) {
                ref[k] += signal[base_j + k];
            }
        }
        for (int k=0; k<single_ref_length; ++k) {
            ref[k] /= o_repeat_cnt;
        }
        refs.insert(refs.end(), ref.begin(), ref.end());
    }
    
	mongodat.upload_record("refs", (float*)refs.data(), 2, refs.size(), NULL, "refs", 1/80., "time(ms)", 1, "I,Q");
	printf("upload refs with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    record[ref_id_key] = MongoDat::OID2str(mongodat.get_fileID());
    record.save();

    mongodat.close();

    return 0;

}

vector< complex<float> > get_calibrated(const vector<char> binary) {
    typedef struct { int16_t s[4]; } uni_out_t;
	const uni_out_t* data = (const uni_out_t*)binary.data();
	int length = binary.size() / sizeof(uni_out_t);
	typedef struct { float s[4]; } float_out_t;
	vector<float_out_t> float_in; float_in.resize(length);
	for (int i=0; i<length; ++i) for (int j=0; j<4; ++j) float_in[i].s[j] = data[i].s[j];

	double irate = 56875;
	double orate = 80000;
	size_t olen = (size_t)(length * orate / irate + .5);
	// size_t odone;
	vector<float_out_t> float_out; float_out.resize(olen);
	soxr_oneshot(irate, orate, 4, (const float*)float_in.data(), length, NULL, (float*)float_out.data(), olen, NULL, NULL, NULL, NULL);
	float_out.erase(float_out.begin(), float_out.begin() + 20);
	float_out.erase(float_out.end() - 20, float_out.end());
	olen -= 40;

    // output drawing for debug soxr
	// mongodat.upload_record("soxr out", (float*)float_out.data(), 4, float_out.size(), NULL, "soxr out", 1/80., "time(ms)", 1, "Ia,Qa,Ib,Qb");
	// printf("upload soxr out with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

    // measure zero-level
	int zero_padding = 0.1 * orate;  // 100ms
	double zero_avr[4] = {0};
	for (int i=0; i<zero_padding; ++i) for (int j=0; j<4; ++j) zero_avr[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) zero_avr[j] /= zero_padding;
	printf("zero_avr: %f %f %f %f\n", zero_avr[0], zero_avr[1], zero_avr[2], zero_avr[3]);

	// then set it as zero
	for (size_t i=0; i<olen; ++i) for (int j=0; j<4; ++j) float_out[i].s[j] -= zero_avr[j];
    
	// output drawing for debug soxr
	// mongodat.upload_record("soxr out", (float*)float_out.data(), 4, float_out.size(), NULL, "soxr out", 1/80., "time(ms)", 1, "Ia,Qa,Ib,Qb");
	// printf("upload soxr out with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// find the maximum change
	double max_change[4] = {0};
	for (size_t i=0; i<olen; ++i) for (int j=0; j<4; ++j) { double change = fabs(float_out[i].s[j]); max_change[j] = max(max_change[j], change);}
	double max_change_sum = 0;
	for (int j=0; j<4; ++j) max_change_sum += max_change[j];
	printf("max_change: %f %f %f %f, sum: %f\n", max_change[0], max_change[1], max_change[2], max_change[3], max_change_sum);

	// find the first that reach 1/5 change sum
	int rough_start = 0;
	for (; rough_start < (int)olen; ++rough_start) {
		double change = 0;
		for (int j=0; j<4; ++j) change += fabs(float_out[rough_start].s[j]);
		if (change > max_change_sum/5) break;
	} printf("found rough start at %d (%f ms)\n", rough_start, rough_start / orate * 1000);
    

	// fine tune rough_start, measure +1.5ms~+2.5ms
	double ones[4] = {0};
	int ones_start = rough_start + 1.5e-3*orate;
	int ones_end = rough_start + 2.5e-3*orate;
	for (int i=ones_start; i<ones_end; ++i) for (int j=0; j<4; ++j) ones[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) ones[j] /= (ones_end - ones_start);
	printf("ones: %f %f %f %f\n", ones[0], ones[1], ones[2], ones[3]);
	double middle = (fabs(ones[0]) + fabs(ones[1]) + fabs(ones[2]) + fabs(ones[3])) / 2;
	int search_start = rough_start - 2e-3*orate;
	int search_idx;
	for (search_idx=search_start; search_idx<ones_start; ++search_idx) {
		double sum = 0;
		for (int j=0; j<4; ++j) sum += fabs(float_out[search_idx].s[j]);
		if (sum > middle) break;
	}
	printf("found middle at %d (%f ms)\n", search_idx, search_idx/orate*1e3);
	rough_start = search_idx;  // use accurate instead

	// from rough_start, measure +1.5ms~+2.5ms and +25~+29ms, test B1
	double B1_avr[4] = {0};
	int B1_start_1 = rough_start + 1.5e-3*orate;
	int B1_end_1 = rough_start + 2.5e-3*orate;
	int B1_start_2 = rough_start + 25e-3*orate;
	int B1_end_2 = rough_start + 29e-3*orate;
	printf("measuring B1 from %d (%f ms) to %d (%f ms) , and %d (%f ms) to %d (%f ms)\n", B1_start_1, B1_start_1/orate*1e3, B1_end_1, B1_end_1/orate*1e3, B1_start_2, B1_start_2/orate*1e3, B1_end_2, B1_end_2/orate*1e3);
	for (int i=B1_start_1; i<B1_end_1; ++i) for (int j=0; j<4; ++j) B1_avr[j] += float_out[i].s[j];
	for (int i=B1_start_2; i<B1_end_2; ++i) for (int j=0; j<4; ++j) B1_avr[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) B1_avr[j] /= (B1_end_1 - B1_start_1 + B1_end_2 - B1_start_2);
	printf("B1_avr: %f %f %f %f\n", B1_avr[0], B1_avr[1], B1_avr[2], B1_avr[3]);

	// from rough_start, measure +14ms~+18ms, test B2
	double B2_avr[4] = {0};
	int B2_start_1 = rough_start + 14e-3*orate;
	int B2_end_1 = rough_start + 18e-3*orate;
	printf("measuring B2 from %d (%f ms) to %d (%f ms)\n", B2_start_1, B2_start_1/orate*1e3, B2_end_1, B2_end_1/orate*1e3);
	for (int i=B2_start_1; i<B2_end_1; ++i) for (int j=0; j<4; ++j) B2_avr[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) B2_avr[j] /= (B2_end_1 - B2_start_1);
	printf("B2_avr: %f %f %f %f\n", B2_avr[0], B2_avr[1], B2_avr[2], B2_avr[3]);

	// from rough_start, measure +5.5ms~+6.5ms and +34ms~+38ms, test B3
	double B3_avr[4] = {0};
	int B3_start_1 = rough_start + 5.5e-3*orate;
	int B3_end_1 = rough_start + 6.5e-3*orate;
	int B3_start_2 = rough_start + 34e-3*orate;
	int B3_end_2 = rough_start + 38e-3*orate;
	printf("measuring B3 from %d (%f ms) to %d (%f ms) , and %d (%f ms) to %d (%f ms)\n", B3_start_1, B3_start_1/orate*1e3, B3_end_1, B3_end_1/orate*1e3, B3_start_2, B3_start_2/orate*1e3, B3_end_2, B3_end_2/orate*1e3);
	for (int i=B3_start_1; i<B3_end_1; ++i) for (int j=0; j<4; ++j) B3_avr[j] += float_out[i].s[j];
	for (int i=B3_start_2; i<B3_end_2; ++i) for (int j=0; j<4; ++j) B3_avr[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) B3_avr[j] /= (B3_end_1 - B3_start_1 + B3_end_2 - B3_start_2);
	printf("B3_avr: %f %f %f %f\n", B3_avr[0], B3_avr[1], B3_avr[2], B3_avr[3]);

	// first compute IaQa, IbQb
	double A_max = fabs(B1_avr[0]) + fabs(B1_avr[1]);
	double *A_ptr = B1_avr;
	if (fabs(B2_avr[0]) + fabs(B2_avr[1]) > A_max) A_ptr = B2_avr;
	if (fabs(B3_avr[0]) + fabs(B3_avr[1]) > A_max) A_ptr = B3_avr;
	double B_max = fabs(B1_avr[2]) + fabs(B1_avr[3]);
	double *B_ptr = B1_avr;
	if (fabs(B2_avr[2]) + fabs(B2_avr[3]) > B_max) B_ptr = B2_avr;
	if (fabs(B3_avr[2]) + fabs(B3_avr[3]) > B_max) B_ptr = B3_avr;
	// then convert them
	typedef struct { float s[2]; } two_t;
	vector<two_t> two; two.resize(olen);
	for (size_t i=0; i<olen; ++i) {
		two[i].s[0] = A_ptr[0] * float_out[i].s[0] - A_ptr[1] * float_out[i].s[1];
		two[i].s[1] = B_ptr[2] * float_out[i].s[2] - B_ptr[3] * float_out[i].s[3];
	}
	// output drawing for debug soxr
	// mongodat.upload_record("two out", (float*)two.data(), 2, two.size(), NULL, "soxr out", 1/80., "time(ms)", 1, "I,Q");
	// printf("upload two out with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// recompute average, linear
	double B1_avr_two[2] = { A_ptr[0] * B1_avr[0] - A_ptr[1] * B1_avr[1], B_ptr[2] * B1_avr[2] - B_ptr[3] * B1_avr[3] };
	double B2_avr_two[2] = { A_ptr[0] * B2_avr[0] - A_ptr[1] * B2_avr[1], B_ptr[2] * B2_avr[2] - B_ptr[3] * B2_avr[3] };
	double B3_avr_two[2] = { A_ptr[0] * B3_avr[0] - A_ptr[1] * B3_avr[1], B_ptr[2] * B3_avr[2] - B_ptr[3] * B3_avr[3] };

	// solve equation a,b,c,d
	// a * B1_avr_two[0] + b * B1_avr_two[1] = 0
	// c * B1_avr_two[0] + d * B1_avr_two[1] = 1
	// a * B2_avr_two[0] + b * B2_avr_two[1] = 1
	// c * B2_avr_two[0] + d * B2_avr_two[1] = 0
	double a = B1_avr_two[1] / ( B2_avr_two[0] * B1_avr_two[1] - B1_avr_two[0] * B2_avr_two[1] );
	double b = B1_avr_two[0] / ( B1_avr_two[0] * B2_avr_two[1] - B2_avr_two[0] * B1_avr_two[1] );
	double c = B2_avr_two[1] / ( B1_avr_two[0] * B2_avr_two[1] - B2_avr_two[0] * B1_avr_two[1] );
	double d = B2_avr_two[0] / ( B2_avr_two[0] * B1_avr_two[1] - B1_avr_two[0] * B2_avr_two[1] );
	
	for (size_t i=0; i<olen; ++i) {
		float I = two[i].s[0], Q = two[i].s[1];
		two[i].s[0] = a * I + b * Q;
		two[i].s[1] = c * I + d * Q;
	}
	// output drawing for debug soxr
	// mongodat.upload_record("two out", (float*)two.data(), 2, two.size(), NULL, "soxr out", 1/80., "time(ms)", 1, "I,Q");
	// printf("upload two out with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// sanity check
	double B3I = a * B3_avr_two[0] + b * B3_avr_two[1];
	double B3Q = c * B3_avr_two[0] + d * B3_avr_two[1];
	printf("B3: %f %f\n", B3I, B3Q);

	// output reference from rough_start-1ms to 4096 points
	// mongodat.upload_record("reference", (float*)(two.data() + rough_start - (int)(1e-3*orate)), 2, 4096, NULL, "reference", 1/80., "time(ms)", 1, "I,Q");
	// printf("upload reference file: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

#define EARLY_POINTS 16  // manually assign
    two.erase(two.begin(), two.begin() + rough_start - EARLY_POINTS);
    vector< complex<float> > ret;
    ret.resize(two.size());
    for (size_t i=0; i<two.size(); ++i) {
        ret[i].real(two[i].s[0]);
        ret[i].imag(two[i].s[1]);
    }

    return ret;
}
