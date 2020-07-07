/*
 * This file is to emulate the behavior of a single LCD based of 17th-order m sequence
 */

#include "modulator.h"
#include "m_sequence.h"
#include <vector>
#include <complex>
#include <random>
using namespace std;

/* Emulate the behavior of a single LCD */
class MS_Emulator_Single {
public:
    static void set_ref(int effect_length, int mseq_freq, int ref_samplerate, int tx_samplerate, vector< complex<float> > refs);  // e.g. 2000 80000 8000
    static int o_emulated_segment_size;  // computed after set_ref, see `emulate_acc` for more
    static int o_mseq_ref_mask;  // computed after set_ref, = (1 << effect_length) - 1

    uint32_t history;  // history bits, history&0x1 for current bit, and history&o_mseq_ref_mask for reference index
    complex<double> amplitude;  // the complex amplitude of this single LCD pixel, default to 1+0j
    MS_Emulator_Single(complex<double> amplitude = complex<double>(1,0));

    // `emulated_segment` should be at least of length `o_emulated_segment_size`
    // acc means the result is accumulated rather than set directly, remember to clear the previous results
    inline void emulate_acc(vector< complex<double> > &emulated_segment, bool bit);
    static vector< complex<double> > create_segment();

private:
    static int effect_length;  // should be no more than 32 (you'll definitely NOT have a 2^32 long reference file...)
    static int mseq_freq;
    static int ref_samplerate;
    static int tx_samplerate;
    static vector< complex<float> > refs;  // the length of this vector will be checked before running the emulation
    static int o_ref_seg_len;  // = ref_samplerate / mseq_freq
    static int o_gcd;  // the greatest common divisor of (ref_samplerate / mseq_freq) and (ref_samplerate / tx_samplerate)
    static int o_mseq_gcd_m;  // (ref_samplerate / mseq_freq) / o_gcd
    static int o_tx_gcd_m;  // (ref_samplerate / tx_samplerate) / o_gcd

    // because tx_samplerate may be greater than mseq_freq, this is for the internal state of current reference file
    bool triggered;  // initialized as `false`
    int idx;
};

#ifndef MS_EMULATE_NO_STATIC
int MS_Emulator_Single::effect_length;
vector< complex<float> > MS_Emulator_Single::refs;
int MS_Emulator_Single::mseq_freq;
int MS_Emulator_Single::ref_samplerate;
int MS_Emulator_Single::tx_samplerate;
int MS_Emulator_Single::o_emulated_segment_size;
int MS_Emulator_Single::o_mseq_ref_mask;
int MS_Emulator_Single::o_ref_seg_len;
int MS_Emulator_Single::o_gcd;
int MS_Emulator_Single::o_mseq_gcd_m;
int MS_Emulator_Single::o_tx_gcd_m;
#endif

void MS_Emulator_Single::set_ref(int effect_length, int mseq_freq, int ref_samplerate, int tx_samplerate, vector< complex<float> > refs) {
    assert(ref_samplerate % mseq_freq == 0);
    assert(ref_samplerate % tx_samplerate == 0);
    assert(effect_length > 0 && effect_length <= 32);
    assert(refs.size() == (1ULL << effect_length) * (ref_samplerate / mseq_freq));
    MS_Emulator_Single::mseq_freq = mseq_freq;
    MS_Emulator_Single::ref_samplerate = ref_samplerate;
    MS_Emulator_Single::tx_samplerate = tx_samplerate;
    MS_Emulator_Single::refs = refs;
    MS_Emulator_Single::o_emulated_segment_size = ref_samplerate / tx_samplerate;
    MS_Emulator_Single::o_mseq_ref_mask = (1 << effect_length) - 1;
    MS_Emulator_Single::o_ref_seg_len = ref_samplerate / mseq_freq;
    function<int(int, int)> gcd = [&](int a, int b) -> int {
        if (a < b) swap(a, b);
        if (a % b == 0) return b;
        else return gcd(b, a%b);
    };
    MS_Emulator_Single::o_gcd = gcd(ref_samplerate / mseq_freq, ref_samplerate / tx_samplerate);

    MS_Emulator_Single::o_mseq_gcd_m = ref_samplerate / mseq_freq / o_gcd;
    MS_Emulator_Single::o_tx_gcd_m = ref_samplerate / tx_samplerate / o_gcd;
}

MS_Emulator_Single::MS_Emulator_Single(complex<double> amplitude_): history(0), amplitude(amplitude_), triggered(false) {

}

vector< complex<double> > MS_Emulator_Single::create_segment() {
    assert(refs.size() && "should first set reference");
    vector< complex<double> > ret; ret.resize(o_emulated_segment_size);
    return ret;
}

void MS_Emulator_Single::emulate_acc(vector< complex<double> > &emulated_segment, bool bit) {
    assert((int)emulated_segment.size() >= o_emulated_segment_size && "buffer not long enough");
    if (!triggered) { // waiting for 1 // waiting for 1
        if (bit) {  // trigger
            triggered = true;
            history = 0;
            idx = -1;
        } else {  // output 0 values
            complex<double> zero_val = (complex<double>)refs[o_ref_seg_len-1] * amplitude;  // take the last sample of ref 000...000
            for (int i=0; i<o_emulated_segment_size; ++i) emulated_segment[i] += zero_val;
        }
    }
    if (triggered) {
        for (int i=0; i<o_tx_gcd_m; ++i) {
            idx = (idx + 1) % o_mseq_gcd_m;
            if (idx == 0) {  // need to update history
                history = (history << 1) | bit;
            }
            complex<float>* refptr = refs.data() + o_ref_seg_len * (o_mseq_ref_mask & history) + idx * o_gcd;
            complex<double>* wptr = emulated_segment.data() + i * o_gcd;
            for (int j=0; j<o_gcd; ++j) {
                wptr[j] += (complex<double>)refptr[j] * amplitude;
            }
        }
        // back to not triggered state after effect_length * 0.5ms. for effect_length = 17, that's 8.5ms
        // this feature is useful when user want to emulate multiple packets, where the interval between them might be any value
        if (history & o_mseq_ref_mask == 0) triggered = false;
    }
}

class MS_Emulator_NLCDs {
public:
    vector<MS_Emulator_Single> singles;  // NLCD * 8 elements
    MS_Emulator_NLCDs(int NLCD, complex<double> amplitude);

    // `emulated_segment` should be at least of length `o_emulated_segment_size`
    // acc means the result is accumulated rather than set directly, remember to clear the previous results
    inline void emulate_acc(vector< complex<double> > &emulated_segment, Tag_Sample_t symbol);

private:
    int NLCD;
    complex<double> amplitude;
};

MS_Emulator_NLCDs::MS_Emulator_NLCDs(int NLCD_, complex<double> amplitude_): NLCD(NLCD_), amplitude(amplitude_) {
    assert(NLCD > 0 && NLCD < TAG_L4XX_SAMPLE_BYTE && "cannot emulate");
    // set amplitude
    double min_amp = ((double)1) / NLCD / (1 + 2 + 4 + 8);  // the smallest pixel
    for (int i=0; i<NLCD; ++i) {
        // to maintain the same order as real hardware
        // 1:2:4:8 => 0x8, 0x4, 0x2, 0x1
        // the lower 4 bits is for Q (0+j), higher 4 bits for I (1+0j)
        // the reference file is I (1+0j)
        for (int j=0; j<4; ++j) {
            singles.push_back(MS_Emulator_Single(amplitude * complex<double>(0, min_amp * (8 >> j))));
        }
        for (int j=0; j<4; ++j) {
            singles.push_back(MS_Emulator_Single(amplitude * complex<double>(min_amp * (8 >> j), 0)));
        }
    }
    assert((int)singles.size() == NLCD * 8 && "sanity check");
}

void MS_Emulator_NLCDs::emulate_acc(vector< complex<double> > &emulated_segment, Tag_Sample_t symbol) {
    for (int i=0; i<NLCD; ++i) {
        for (int j=0; j<8; ++j) {
            singles[i * 8 + j].emulate_acc(emulated_segment, (symbol.le(i) >> j) & 1);
        }
    }
}
