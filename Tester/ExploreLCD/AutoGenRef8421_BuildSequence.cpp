#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
#include "modulator.h"
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

vector< vector<string> > base_sequences;
vector< vector<string> > build_reference_collection_sequence(int NLCD, double frequency, int cycle, int duty, int effect_length, int repeat_cnt, int& o_sample2send, int& o_repeat_cnt);

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 3) {
		printf("usage: <collection> <id(12 byte hex = 24 char)>\n");
        printf("the document should have the following values:\n");
        printf("  NLCD: at most 16, which has 16x8=128 independent pixels\n");
        printf("  frequency: the frequency to drive each LCD pixel, this should not be more than 250Hz when voltage is 8V, experimentally\n");
        printf("  effect_length: if frequency is under 125Hz, effect_length can be 1, otherwise consider add more.\n");
        printf("  repeat_cnt: for every reference, repeat it for multiple times to average\n\n");
        printf("the reference collecting process is described below:\n");
#define MAX_PACKET_TIME_S 2.0
        printf("  first, due to clock accuracy (10ppm), each packet should not exceed 2s (80kS/s)\n");
        printf("  in each packet, LCD send 0x0F, 0xF0\n");
		return -1;
	}
    
	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
    
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

    // generate and store in database
    int o_sample2send = 0;
    int o_repeat_cnt = 0;
    vector< vector<string> > reference_collection_sequence = build_reference_collection_sequence(NLCD, frequency, cycle, duty, effect_length, repeat_cnt, o_sample2send, o_repeat_cnt);
    assert((int)reference_collection_sequence.size() == NLCD && "invalid behavior");
    for (int i=0; i<NLCD; ++i) {
        char key_name[32];
        sprintf(key_name, "ref8421seq_%d", i);
        record[key_name].remove();
        record[key_name].build_array();
        record[key_name].append(reference_collection_sequence[i]);
    }
    assert((int)base_sequences.size() == 2 * NLCD && "base_sequences strange");
    for (int i=0; i<2*NLCD; ++i) {
        char key_name[32];
        sprintf(key_name, "base_%d", i);
        record[key_name].remove();
        record[key_name].build_array();
        record[key_name].append(base_sequences[i]);
    }

    record["o_sample2send"] = o_sample2send;
    record["o_sample2send_each"] = o_sample2send / NLCD;
    record["o_repeat_cnt"] = o_repeat_cnt;
    record["o_estimated_time"] = o_sample2send / frequency;
    record["o_estimated_time_each"] = o_sample2send / frequency / NLCD;
    record.save();
    printf("record saved\n");

	mongodat.close();
    printf("database closed\n");

    return 0;
}

// return NLCD elements
vector< vector<string> > build_reference_collection_sequence(int NLCD, double frequency, int cycle, int duty, int effect_length, int repeat_cnt, int& o_sample2send, int& o_repeat_cnt) {
    // first compute how many packets are needed to do this reference collection
    vector<bool> m_sequence = generate_m_sequence(effect_length);
    printf("m_sequence size: %d\n", (int)m_sequence.size());
    int m_sequence_cnt_each_packet = MAX_PACKET_TIME_S * frequency / cycle / m_sequence.size();
    printf("m_sequence_cnt_each_packet: %d\n", m_sequence_cnt_each_packet);
    int each_packet_repeat = (repeat_cnt + m_sequence_cnt_each_packet - 1) / m_sequence_cnt_each_packet;
    printf("each_packet_repeat: %d\n", each_packet_repeat);
    int packet_cnt = NLCD * 2 * each_packet_repeat;
    printf("have %d packets totally\n", packet_cnt);

    // build basic_sequence which is single pixel evaluation in ONE packet, including (m_sequence_cnt_each_packet + 1) ref
    vector<bool> basic_sequence;
    for (int i=0; i <= m_sequence_cnt_each_packet; ++i) {  // loop for (m_sequence_cnt_each_packet + 1) times
        basic_sequence.insert(basic_sequence.end(), m_sequence.begin(), m_sequence.end());
    }
    printf("each packet has %d samples\n", (int)basic_sequence.size());

    // map the basic_sequence to each LCD, and repeat them for several times
    FastDSM_Encoder encoder;
    encoder.NLCD = NLCD;
    encoder.frequency = frequency;
    encoder.build_preamble();
    Tag_Sample_t A1; memset(A1.s, 0xFF, NLCD);
    Tag_Sample_t B0; memset(B0.s, 0x00, NLCD);
    vector< vector<string> > ret;
    o_sample2send = 0;
    for (int i=0; i<NLCD; ++i) {
        vector<Tag_Sample_t> seqs;
        for (int j=0; j<2; ++j) {
            Tag_Sample_t B1 = B0;
            if (j == 0) B1.le(i) = 0x0F;
            else B1.le(i) = 0xF0;
            vector<Tag_Sample_t> seq;
            for (int k=0; k<(int)basic_sequence.size(); ++k) {
                if (basic_sequence[k]) {
                    seq.insert(seq.end(), duty, B1);
                    seq.insert(seq.end(), cycle-duty, B0);
                } else {
                    seq.insert(seq.end(), cycle, B0);
                }
            }
            for (int k=0; k<each_packet_repeat; ++k) {
                seqs.insert(seqs.end(), encoder.o_preamble.begin(), encoder.o_preamble.end());
                seqs.insert(seqs.end(), seq.begin(), seq.end());
            }
            vector<Tag_Sample_t> single_seq;
            for (int k=0; k<(int)m_sequence.size(); ++k) {
                if (m_sequence[k]) {
                    single_seq.insert(single_seq.end(), duty, B1);
                    single_seq.insert(single_seq.end(), cycle-duty, B0);
                } else {
                    single_seq.insert(single_seq.end(), cycle, B0);
                }
            } base_sequences.push_back(encoder.compressed_vector(single_seq));
        }
        seqs.insert(seqs.end(), 1, B0);
        o_sample2send += seqs.size();
        ret.push_back(encoder.compressed_vector(seqs));
    }

    o_repeat_cnt = m_sequence_cnt_each_packet * each_packet_repeat;
    printf("actual repeat_cnt: %d\n", o_repeat_cnt);
    printf("sample2send: %d\n", o_sample2send);
    return ret;
}
