#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <set>
#include "float.h"
using namespace std;

#if defined(_WIN32)
#define ENABLE_CPUTIME
#include "windows.h"
inline uint64_t filetime_to_int(const FILETIME* ftime) {
    LARGE_INTEGER li;
    li.LowPart = ftime->dwLowDateTime;
    li.HighPart = ftime->dwHighDateTime;
    return li.QuadPart;
}
inline uint64_t get_cputime() {  // return the time in 100ns
    FILETIME createTime, exitTime, kernelTime, userTime;
    assert( GetProcessTimes( GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime ) != -1);
    return filetime_to_int(&kernelTime) + filetime_to_int(&userTime);
} 
uint64_t cputime_start = get_cputime();
#define CONSUMED_CPUTIME ((double)(get_cputime()-cputime_start) / 1e7)
#else
#include <csignal>
#endif

#define DEBUG

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

/*
You can download reference file at https://github.com/wuyuepku/RetroTurbo/releases/tag/LCDmodel

It is impossible to test all the cases for data rate larger than 1kbps, and we need to analyze 4kbps, 8kbps and 12kbps cases,
    thus we use random algorithm for solution searching

I found that for those symmetric edges, the result is very strange, since our previous assumptions in 200121_MinDistancePQAMDSM.cpp does not hold any more
An alternative way is just to demodulate those data, using DFE with multiple branches, and evaluate the solution that is not 0 to find the smallest variance
Later I found that use demod_buffer_length only will leads to no result at all, since smaller however incomplete sequences will occupy the buffer, 
    leaving no space for those valid complete sequences. To handle this, I add each_step_length to limit the most branches that can be added to a specific new symbol
This still fails (no result), I try to evaluate those members with random suffix, which might help

Found that there is accumulated error, which causes the minimum variance in each round is increasing. To solve this, we need to start new program each time

*/

const int MASK17 = (1 << 17) - 1;

struct MinDistance {
    vector<uint32_t> origin;
    vector<uint32_t> changed;
    double var;
};

int NLCD;  // each LCD is composed of a pair of 0° and 45° PQAM pieces.
bool PQAM;  // if enable PQAM
int bit_per_symbol_channel;  // bit per symbol per channel
#define BIT_PER_SYMBOL (PQAM ? 2*bit_per_symbol_channel : bit_per_symbol_channel)
int fire_interval;  // it is allowed to have fire interval at 0.025ms accuracy, and thus fire interval is actually 0.025ms * fire_interval
int duty;  // duty is at 0.5ms accuracy, duty is actually 0.5ms * duty
#define DATA_RATE (BIT_PER_SYMBOL * 40e3 / fire_interval)
#define CYCLE_FLOOR (fire_interval * NLCD / 20)  // unit = 0.5ms
// our real-world experiment use NLCD=8(=16/2), bit_per_symbol=2/4/6, PQAM=true, fire_interval=20(0.5ms), duty=2(1ms)
int demod_buffer_length;  // see the comments above
int each_step_length;  // see the comments above
int min_time;
int total_round = 0;
int prefix_suffix_symbol_count = -1;
atomic<bool> running = true;

complex<float>* reference_base = NULL;

struct EmulatorState {
    vector<uint32_t> history;  // size of NLCD
    vector<uint32_t> sequence;  // size of NLCD * bit_per_symbol
    vector< complex<float>* > ref_base;  // size of NLCD * bit_per_symbol
    int bit_per_symbol;
    double smallest_amp;
    uint32_t idx;
    EmulatorState() {
        assert(reference_base != NULL);
        bit_per_symbol = BIT_PER_SYMBOL;
        history.insert(history.end(), NLCD, 0);
        sequence.insert(sequence.end(), NLCD * bit_per_symbol, 0);
        ref_base.insert(ref_base.end(), NLCD * bit_per_symbol, reference_base);  // initialize reference base as all zero sequences
        idx = 0;
        smallest_amp = 1. / NLCD / ((PQAM ? (2 * (1 << bit_per_symbol_channel)) : (1 << bit_per_symbol)) - 1);
    }
#define EMULATE_SIZE fire_interval
    static vector< complex<double> > create_emulated_segment() {
        vector< complex<double> > emulated_segment;
        emulated_segment.resize(EMULATE_SIZE);
        return emulated_segment;
    }
    inline void PQAM_DSM_emulate(complex<double>* emulated_segment, uint32_t symbol, int interested_NLCD = NLCD) {
        // insert data
        history[idx] = symbol;
        for (int i=0; i<EMULATE_SIZE; ++i) emulated_segment[i] = complex<double>(0,0);  // flush 0
        for (int j=0; j<NLCD && j<interested_NLCD; ++j) {
            int point_distance_base = ((NLCD + idx - j) % NLCD) * fire_interval;
            for (int i=0; i<EMULATE_SIZE; ++i) {
                int point_distance = point_distance_base + i;
                int ref_idx = point_distance % 20;
                int k_base = j * bit_per_symbol;
                if (ref_idx == 0) {  // should load a new reference pointer
                    // judge by duty, whether push 0 or 1
                    bool in_duty = point_distance/20 < duty;
                    int j_history = history[j];
                    for (int k=0; k<bit_per_symbol; ++k) {
                        int k_idx = k_base + k;
                        bool push_val = in_duty ? ((j_history >> k) & 1) : 0;
                        int k_sequence = sequence[k_idx] = (sequence[k_idx] << 1) | push_val;
                        complex<float>* new_ref_base = reference_base + 20 * (k_sequence & MASK17);
                        ref_base[k_idx] = new_ref_base;
                    }
                }
                complex<double>* ptr = emulated_segment + i;
                if (PQAM) {
                    for (int k=0; k<bit_per_symbol_channel; ++k) {
                        double k_amp = smallest_amp * (1 << k);
                        complex<float> point_I = ref_base[k_base + k][ref_idx];
                        complex<float> point_Q = ref_base[k_base + k + bit_per_symbol_channel][ref_idx];
                        *ptr += k_amp * complex<double>(point_I.real() + point_Q.imag(), point_I.imag() + point_Q.real());
                    }
                } else {
                    for (int k=0; k<bit_per_symbol_channel; ++k) {
                        double k_amp = smallest_amp * (1 << k);
                        complex<float> point = ref_base[k_base + k][ref_idx];
                        *ptr += k_amp * complex<double>(point.real(), point.imag());
                    }
                }
            }
        }
        idx = (idx + 1) % NLCD;
    }
};

string print_symbols(const vector<uint32_t>& symbols) {
    string str = "{ ";
    char buf[64];
    for (int i=0; i<(int)symbols.size(); ++i) {
        sprintf(buf, "%d%s ", symbols[i], i+1==(int)symbols.size()?"":",");
        str += buf;
    } str += "}";
    return str;
}
#define STR(symbols) (print_symbols(symbols).c_str())

struct DecodeBranch {
    double segma;
    vector<uint32_t> dec;  // this is the history result
    int now_dec_idx;
    EmulatorState emu;
    vector< complex<double> > buf;
    DecodeBranch() {
        segma = 0; now_dec_idx = 0;
        int history_symbols_len = prefix_suffix_symbol_count * 3;  // this is enough
        dec.resize(history_symbols_len);
        buf = EmulatorState::create_emulated_segment();
    }
    void push_symbol(uint32_t new_value, bool increment = true) {
        dec[now_dec_idx] = new_value;
        emu.PQAM_DSM_emulate(buf.data(), new_value);
        if (increment) now_dec_idx = (now_dec_idx + 1) % dec.size();
    }
    void push_symbol(uint32_t new_value, const vector< complex<double> >& origin_buf) {
        push_symbol(new_value);
        for (int i=0; i<(int)buf.size(); ++i) {
            complex<double> d1 = origin_buf[i] - buf[i];
            segma += pow(d1.real(), 2) + pow(d1.imag(), 2);
        }
    }
    bool last_equal(const DecodeBranch& branch) const {
        assert(branch.now_dec_idx == now_dec_idx);  // cannot compare with different idx
        for (int i=1; i<prefix_suffix_symbol_count; ++i) {
            int idx = (now_dec_idx + dec.size() - i) % dec.size();
            if (branch.dec[idx] != dec[idx]) return false;
        }
        return true;
    }
    bool all_equal(const DecodeBranch& branch) const {
        assert(branch.now_dec_idx == now_dec_idx);  // cannot compare with different idx
        for (int i=1; i<(int)dec.size(); ++i) {
            if (branch.dec[i] != dec[i]) return false;
        }
        return true;
    }
    bool operator< (const DecodeBranch &obj) const {
        return segma < obj.segma;
    }
};

void compute_random_timeout(MinDistance& mindis, mutex& mut) {
#ifdef ENABLE_CPUTIME
    struct timeval time;
	gettimeofday( &time, NULL );
	mt19937 generator(1000000 * time.tv_sec + time.tv_usec);
    int MASK_SYMBOL = (1 << BIT_PER_SYMBOL) - 1;
    uniform_int_distribution<int> distribution(0, MASK_SYMBOL);
    auto rand = std::bind(distribution, generator);

    do {
        DecodeBranch correct;
        set<DecodeBranch> branches;  // this is all branches active, which has maximum length of demod_buffer_length
        branches.insert(correct);

        double last_min_var = DBL_MAX;  // only less than this value should we check again
        double this_round_min = DBL_MAX;
        for (int k=0; k<prefix_suffix_symbol_count << 2; ++k) {
            uint32_t new_value = rand();
            correct.push_symbol(new_value, false);
            set<DecodeBranch> new_branches;
            for (auto bt = branches.begin(); bt != branches.end(); ++bt) {
                set<DecodeBranch> sub_new_branches;
                for (int changed=0; changed<MASK_SYMBOL; ++changed) {
                    DecodeBranch test_branch = *bt;
                    test_branch.push_symbol(changed, correct.buf);
                    sub_new_branches.insert(test_branch);
                }
                int sub_new_branch_length = sub_new_branches.size();
                if (sub_new_branch_length > each_step_length) sub_new_branch_length = each_step_length;
                auto sbt = sub_new_branches.begin();
                for (int j=0; j<sub_new_branch_length; ++j, ++sbt) {
                    new_branches.insert(*sbt);
                }
            }
            correct.now_dec_idx = (correct.now_dec_idx + 1) % correct.dec.size();
            // update branches
            branches.clear();
            int new_branch_length = new_branches.size();
            if (new_branch_length > demod_buffer_length) new_branch_length = demod_buffer_length;
            auto bt = new_branches.begin();
            for (int j=0; j<new_branch_length; ++j, ++bt) {
                branches.insert(*bt);
            }
            // search them with random suffix
            for (auto sbt = branches.begin(); sbt != branches.end(); ++sbt) {
                if (!sbt->all_equal(correct)) {
                    DecodeBranch test_branch = *sbt;
                    DecodeBranch test_origin = correct;
                    for (int i=0; i<prefix_suffix_symbol_count; ++i) {
                        uint32_t new_suffix = rand();
                        test_origin.push_symbol(new_suffix);
                        test_branch.push_symbol(new_suffix, test_origin.buf);
                    }
                    if (test_branch.segma < last_min_var) {
                        mut.lock();
                        last_min_var = mindis.var;  // update cache
                        if (mindis.origin.size() == 0 || test_branch.segma < mindis.var) {
                            last_min_var = mindis.var = test_branch.segma;
                            int dec_size = test_branch.dec.size();
                            mindis.changed.resize(dec_size);
                            mindis.origin.resize(dec_size);
                            for (int i=0; i<dec_size; ++i) {
                                int idx = (test_origin.now_dec_idx + i) % dec_size;
                                mindis.changed[i] = test_branch.dec[idx];
                                mindis.origin[i] = test_origin.dec[idx];
                            }
                            printf("\nnew var: %.16f with origin(%s) changed(%s)\n"
                                , mindis.var, STR(mindis.origin), STR(mindis.changed)); fflush(stdout);
                        }
                        mut.unlock();
                    }
                    if (test_branch.segma < this_round_min) {
                        this_round_min = test_branch.segma;
                    }
                }
            }
        }

        mut.lock();
        ++total_round;
        mut.unlock();
        for (int i=0; i<128; ++i) fprintf(stderr, "\b");
        double consumed_cputime = CONSUMED_CPUTIME;
        fprintf(stderr, "round: %d, %.16f, time: %f %% (%f/%d)", total_round, this_round_min, consumed_cputime / min_time * 100, consumed_cputime, min_time);
    } while ((min_time < 0 || CONSUMED_CPUTIME <= min_time) && running);
#else
    assert(0 && "ENABLE_CPUTIME not set, cannot run");
#endif
}

void signal_handler(int) {
    running = false;
}

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

    if (argc != 9 && argc != 10) {
		printf("usage: <local_filename> <NLCD> <bit_per_symbol_channel> <fire_interval> <duty> <PQAM> <demod_buffer_length> <each_step_length> [min_time/s]\n");
        printf("for example: ./Tester/ExploreLCD/EL_200203_MinDistancePQAMDSM_demod.exe 200115_model_500us_17mseq_9v.bin.resampled.reordered 8 1 20 2 1 64 4\n");
        printf("    min_time: system will at least run a round of demod_buffer_length, however will not run the next round if CPU time exceed min_time\n");
        printf("            this is the actual computation time, not system time. for Windows, time = kernelTime + userTime\n");
		return -1;
	}

	MongoDat::LibInit();
	if (MONGO_URL[0]) mongodat.open(MONGO_URL, MONGO_DATABASE);

	const char* local_filename = argv[1];
    NLCD = atoi(argv[2]); assert(NLCD > 0 && "NLCD must be greater than 0");
    bit_per_symbol_channel = atoi(argv[3]); assert(bit_per_symbol_channel > 0 && "bit_per_symbol_channel should be positive integer");
    fire_interval = atoi(argv[4]); assert(fire_interval > 0 && "fire_interval must be greater than 0");
    duty = atoi(argv[5]); assert(duty > 0 && duty <= CYCLE_FLOOR && "duty cannot exceed 2D");
    PQAM = atoi(argv[6]);
    demod_buffer_length = atoi(argv[7]); assert(demod_buffer_length > 1 && "demod_buffer_length should be > 1");
    each_step_length = atoi(argv[8]); assert(each_step_length > 1 && "each_step_length should be > 1");
    min_time = argc > 9 ? atoi(argv[9]) : -1;
    prefix_suffix_symbol_count = round(8 / (0.025 * fire_interval));  // 8ms effect length
    // sanity check
    assert(BIT_PER_SYMBOL <= 32 && "otherwise cannot use uint32_t");

    printf("DATA_RATE: %f bps\n", DATA_RATE);

    ifstream file(local_filename, std::ios::binary);
    file.unsetf(std::ios::skipws);  // Stop eating new lines in binary mode!!!
    file.seekg(0, std::ios::end);
    streampos fileSize = file.tellg();
    assert(fileSize == (1 << 17) * 20 * sizeof(complex<float>) && "file size error");
    file.seekg(0, std::ios::beg);
    vector< complex<float> > vec; vec.resize((1 << 17) * 20);
    file.read((char*)vec.data(), fileSize);
    reference_base = vec.data();

// start testing
    MinDistance mindis;
    mutex mut;
    vector<thread*> threads;
#define THREAD_COUNT 1
    for (int i=0; i<THREAD_COUNT; ++i) {
        threads.push_back(new thread(compute_random_timeout, ref(mindis), ref(mut)));
    }
    signal(SIGINT, signal_handler);
    for (auto it=threads.begin(); it!=threads.end(); ++it) {
        (*it)->join();
    }

    mut.lock();
    if (mindis.origin.size() == 0) {
        printf("\nexit with no result\n"); exit(-1);
    }
    double var = mindis.var;
    vector<uint32_t> origin = mindis.origin;
    vector<uint32_t> changed = mindis.changed;
    printf("\nmin var: %.16f with origin(%s) changed(%s) with %d round\n"
            , var, STR(origin), STR(changed), total_round); fflush(stdout);
    fprintf(stderr, "\nmin var: %.16f with origin(%s) changed(%s) with %d round\n"
            , var, STR(origin), STR(changed), total_round);
    DecodeBranch origin_emu;
    DecodeBranch changed_emu;
    vector< complex<double> > origin_emulated;
    vector< complex<double> > changed_emulated;
    int origin_size = origin.size();
    for (int i=0; i<origin_size; ++i) {
        origin_emu.push_symbol(origin[i]);
        origin_emulated.insert(origin_emulated.end(), origin_emu.buf.begin(), origin_emu.buf.end());
        changed_emu.push_symbol(changed[i]);
        changed_emulated.insert(changed_emulated.end(), changed_emu.buf.begin(), changed_emu.buf.end());
    }
    vector< complex<float> > emulated;
    int emulated_length = origin_emulated.size();
    for (int i=0; i<emulated_length; ++i) {
        emulated.push_back(complex<float>( origin_emulated[i].real() , origin_emulated[i].imag() ));
        emulated.push_back(complex<float>( changed_emulated[i].real() , changed_emulated[i].imag() ));
    }

    if (MONGO_URL[0]) {
        mongodat.upload_record("emulated", (float*)emulated.data(), 4, emulated.size() / 2
            , NULL, "emulated", 1./40, "time(ms)", 1, "Io,Qo,Ic,Qc");
        printf("upload emulated with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

        mongodat.close();
    }

    return 0;
}
