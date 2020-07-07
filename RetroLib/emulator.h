/*
 * This file is to emulate the behavior of multiple LCDs
 */

// define this to make most parameters static, could be used to improve speed when copy objects, however, cannot use multiple different parameters in one process
// do this very carefully !!!
// TODO: this feature is not fully supported yet, leave this later if a DP decoder is needed
#ifdef STATIC_EMULATOR_FOR_EASIER_COPY
#endif

#include "modulator.h"
#include "m_sequence.h"
#include <vector>
#include <complex>
#include <random>
using namespace std;

class FastDSM_Emulator {
public:
#ifdef STATIC_EMULATOR_FOR_EASIER_COPY
    static int NLCD;
    static int duty;
    static int cycle;
    static int effect_length;
    static int combine;
    static int seg_out_length;
    static double frequency;
    static int bias;  // add how many zeros before the data part, for consistency with real experiment
    static vector< complex<float> > refs;  // the length of this vector will be checked before running the emulation
    static vector<int> m_seq_map;  // for example, find the 00<1> inside 0010111 would give the result: 2, which is the latest sample located
    static double noise;  // only used inside function "emulate", but not "push" !!!
#else
    int NLCD;
    int duty;
    int cycle;
    int effect_length;
    int combine;
    int seg_out_length;
    double frequency;
    int bias;  // add how many zeros before the data part, for consistency with real experiment
    vector< complex<float> > refs;  // the length of this vector will be checked before running the emulation
    vector<int> m_seq_map;  // for example, find the 00<1> inside 0010111 would give the result: 2, which is the latest sample located
    double noise = 0;  // only used inside function "emulate", but not "push" !!!
#endif

    // this is the essential of emulator which enables segment input & output
    // history vector, whose longest size is ( effect_length * (NLCD / combine) )
    // this variable should be copied when object is copied, thus never static
    int single_idx;
    vector<Tag_Single_t> history;
    vector< complex<float> > out_segment;

    const double orate = 80000;
    void sanity_check();
    void internal_push(Tag_Single_t value);  // segment input and segment output API
    vector< complex<float> > push(Tag_Single_t value);
    void initialize();  // this will initialize the history, etc, for later use "push" API
    vector< complex<float> > emulate(const vector<Tag_Single_t>& singles, size_t minimum_length=0);  // this should be the output of FastDSM_Encoder::PQAMencode
    vector<Tag_Sample_t> uncompressed_vector(const vector<string>& compressed);
    static vector<uint8_t> get_bytes_from_hex_string(string hex);
#ifdef STATIC_EMULATOR_FOR_EASIER_COPY
    FastDSM_Emulator& operator=(const FastDSM_Emulator&);  // implement this only when static, otherwise the copy is too expensive
    FastDSM_Emulator(const FastDSM_Emulator&);
    FastDSM_Emulator();
#else
    FastDSM_Emulator& operator=(const FastDSM_Emulator&) = delete;
#endif
};

#ifdef STATIC_EMULATOR_FOR_EASIER_COPY
int FastDSM_Emulator::NLCD;
int FastDSM_Emulator::duty;
int FastDSM_Emulator::cycle;
int FastDSM_Emulator::effect_length;
int FastDSM_Emulator::combine;
int FastDSM_Emulator::seg_out_length;
double FastDSM_Emulator::frequency;
int FastDSM_Emulator::bias;  // add how many zeros before the data part, for consistency with real experiment
vector< complex<float> > FastDSM_Emulator::refs;  // the length of this vector will be checked before running the emulation
vector<int> FastDSM_Emulator::m_seq_map;  // for example, find the 00<1> inside 0010111 would give the result: 2, which is the latest sample located
double FastDSM_Emulator::noise = 0;  // only used inside function "emulate", but not "push" !!!
#endif

#define EMULATOR_MAX_NLCD 32

void FastDSM_Emulator::sanity_check() {
    assert(NLCD > 0 && NLCD <= EMULATOR_MAX_NLCD);
    assert(cycle > 0);
    assert(duty > 0 && duty <= cycle);
    assert(effect_length > 0 && effect_length <= 16);
    int data_cycle_length = cycle * orate / frequency;
    int m_sequence_length = (1 << effect_length) - 1;
    int data_length_should_be = m_sequence_length * data_cycle_length;
    // printf("data_length_should_be: %d, refs.size(): %d\n", data_length_should_be, (int)refs.size());
    assert(data_length_should_be * NLCD * 2 == (int)refs.size());
}

vector<uint8_t> FastDSM_Emulator::get_bytes_from_hex_string(string hex) {
	vector<uint8_t> bs;
	assert(hex.size() % 2 == 0 && "invalid hex string");
	for (size_t i=0; i<hex.size(); i += 2) {
		char H = hex[i], L = hex[i+1];
#define ASSERT_HEX_VALID_CHAR_TMP(x) assert(((x>='0'&&x<='9')||(x>='a'&&x<='z')||(x>='A'&&x<='Z')) && "invalid char");
		ASSERT_HEX_VALID_CHAR_TMP(H) ASSERT_HEX_VALID_CHAR_TMP(L)
#undef ASSERT_HEX_VALID_CHAR_TMP
#define c2bs(x) (x>='0'&&x<='9'?(x-'0'):((x>='a'&&x<='f')?(x-'a'+10):((x>='A'&&x<='F')?(x-'A'+10):(0))))
		char b = (c2bs(H) << 4) | c2bs(L);
#undef c2bs
		bs.push_back(b);
	}
	return bs;
}

vector< complex<float> > FastDSM_Emulator::push(Tag_Single_t value) {
    internal_push(value);
    return out_segment;
}

void FastDSM_Emulator::internal_push(Tag_Single_t value) {
    int L = NLCD / combine;  // DSM order
    // printf("seg_out_length: %d\n", seg_out_length);
    out_segment.resize(seg_out_length); for (int i=0; i<seg_out_length; ++i) out_segment[i] = complex<float>(0,0);
    // locate the reference and add them together here
    history.insert(history.begin(), value);
    int history_size = effect_length * L;
    history.resize(history_size, 0);
    for (int i=0; i<L; ++i) {  // i is the DSM slot index
        for (int l=0; l<combine; ++l) {
            int lcd_idx = ((single_idx + L - i) % L) + l * L; //duo-8421 LCD index
            for (int j=0; j<8; ++j) {  // j is the small pixel
                int his = 0;
                for (int k=0; k<effect_length; ++k) {
                    his |= ((history[i + k*L] >> j) & 0x01) << k;
                }
                int m_seq_idx = m_seq_map[his];
                int tim_delta = i * cycle * orate / frequency / L;  // this is the starting point to sample
                // int refs_idx = (NLCD - 1 - lcd_idx) * 2 + (j>=4);  // because refs script is reversed...
                int refs_idx = lcd_idx * 2 + (j>=4);
                int single_cycle_length = cycle * orate / frequency;
                int single_ref_length = single_cycle_length * ((1 << effect_length) - 1);
                const complex<float>* refs_part = refs.data() + refs_idx * single_ref_length + m_seq_idx * single_cycle_length;
                float ratio = (1<<(3-(j%4))) / 15.; // note: _ _ _ _, left is smaller, right is larger, means (j%4=0)->8/15, (j%4=3)->1/15 
                for (int k=0; k<seg_out_length; ++k) {
                    out_segment[k] += refs_part[tim_delta+k] * ratio;
                }
            }
        }
    }
    single_idx += 1;
}

void FastDSM_Emulator::initialize() {
    single_idx = 0;
    history.clear();
    history.resize(effect_length * (NLCD / combine), 0);
    int L = NLCD / combine;  // DSM order
    seg_out_length = cycle * orate / frequency / L;
    assert(effect_length == 3 && "if you want others, simply add the map vector here");
    if (effect_length == 3) {
        m_seq_map.resize(8);
        // 0010111
        // 0123456
        m_seq_map[0] = 1;  // simply use the second 0, same as 10<0>
        m_seq_map[1] = 2;  // 00<1>
        m_seq_map[2] = 3;  // 01<0>
        m_seq_map[3] = 5;  // 01<1>
        m_seq_map[4] = 1;  // 10<0>
        m_seq_map[5] = 4;  // 10<1>
        m_seq_map[6] = 0;  // 11<0>
        m_seq_map[7] = 6;  // 11<1>
    }
}

vector< complex<float> > FastDSM_Emulator::emulate(const vector<Tag_Single_t>& singles, size_t minimum_length) {
    assert(bias >= 0 && "bias error, data part will be missing");
    vector< complex<float> > emulated;
    emulated.insert(emulated.end(), bias, complex<float>(0, 0));
    // then add data to here
    initialize();
    for (int i=0; i<(int)singles.size(); ++i) {
        internal_push(singles[i]);
        emulated.insert(emulated.end(), out_segment.begin(), out_segment.end());
    }
    int extra_singles_cnt = NLCD / combine;
    for (int i=0; i<extra_singles_cnt; ++i) {
        internal_push(0);
        emulated.insert(emulated.end(), out_segment.begin(), out_segment.end());
    }
    while (emulated.size() < minimum_length) {
        internal_push(0);
        emulated.insert(emulated.end(), out_segment.begin(), out_segment.end());
    }
    // add noise
    if (noise > 0) {
        auto dist = bind(std::normal_distribution<double>{0, noise}, mt19937(random_device{}()));
        for (int i=0; i<(int)emulated.size(); ++i) {
            emulated[i] += complex<float>(dist(), dist());
        }
    }
    return emulated;
}

#ifdef STATIC_EMULATOR_FOR_EASIER_COPY
FastDSM_Emulator& FastDSM_Emulator::operator=(const FastDSM_Emulator& obj) {
    single_idx = obj.single_idx;
    history = obj.history;
#ifdef EMULATOR_KEEP_SEGMENT_WHEN_COPY
    out_segment = obj.out_segment;
#endif
    return *this;
}
FastDSM_Emulator::FastDSM_Emulator(const FastDSM_Emulator& obj) {
    single_idx = obj.single_idx;
    history = obj.history;
#ifdef EMULATOR_KEEP_SEGMENT_WHEN_COPY
    out_segment = obj.out_segment;
#endif
}
FastDSM_Emulator::FastDSM_Emulator() {
    single_idx = 0;
}
#endif
