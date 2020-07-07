#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
#include <mutex>
#include <thread>
#include <atomic>
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
The assumptions are described here:
    1. The minimum distance only occurs when there're at most two adjacent symbols differ from origin sequence.
        This assumption is concluded from 200121_MinDistance1kbpsDSM which is a brute-force algorithm shows the result above
        however, to ensures that this conclusion also applies to higher order, we leave a parameter for user to set: brute_force_symbols
    2. Those symbols that doesn't change at all can be ignored, since different pixels are composed together linearly
        This reduces the searching space, since we assume all the LCDs are the same
    3. For LCD whose symbols changes, the previous symbols and later symbols are not critical, thus can use random algorithm
        In each round, guess a prefix and suffix then test all the critical ones, print out the better solutions if found
*/

const int MASK17 = (1 << 17) - 1;

struct MinDistance {
    vector<uint32_t> prefix;
    vector<uint32_t> origin;
    vector<uint32_t> changed;
    vector<uint32_t> suffix;
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
int brute_force_symbols;  // see the comments above
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

void run_emulation(EmulatorState& emulator, vector< complex<double> >& result, const vector<uint32_t>& critical, const vector<uint32_t>& suffix) {
#ifdef DEBUG
    assert(result.size() == (critical.size() + suffix.size()) * EMULATE_SIZE);
#endif
    complex<double>* result_ptr = result.data();
    for (int i=0; i<brute_force_symbols; ++i) {
        emulator.PQAM_DSM_emulate(result_ptr + i*EMULATE_SIZE, critical[i], brute_force_symbols);
    }
    complex<double>* bias_ptr = result_ptr + brute_force_symbols * EMULATE_SIZE;
    for (int i=0; i<prefix_suffix_symbol_count; ++i) {
        emulator.PQAM_DSM_emulate(bias_ptr + i*EMULATE_SIZE, suffix[i], brute_force_symbols);
    }
}

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

void compute_random_timeout(MinDistance& mindis, mutex& mut) {
#ifdef ENABLE_CPUTIME
    struct timeval time;
	gettimeofday( &time, NULL );
	mt19937 generator(1000000 * time.tv_sec + time.tv_usec);
    int MASK_SYMBOL = (1 << BIT_PER_SYMBOL) - 1;
    uniform_int_distribution<int> distribution(0, MASK_SYMBOL);
    auto rand = std::bind(distribution, generator);
    do {
        uint64_t brute_max = 1 << (brute_force_symbols * BIT_PER_SYMBOL);
        // brute_force_symbols may be larger than NLCD, be careful !!!
        // int interested_NLCD = brute_force_symbols > NLCD ? NLCD : brute_force_symbols;
        vector<uint32_t> prefix; for (int i=0; i<prefix_suffix_symbol_count; ++i) prefix.push_back(rand());
        vector<uint32_t> origin; origin.resize(brute_force_symbols);  // iterate later
        vector<uint32_t> changed; changed.resize(brute_force_symbols);  // iterate later
        vector<uint32_t> suffix; for (int i=0; i<prefix_suffix_symbol_count; ++i) suffix.push_back(rand());
        EmulatorState prefixed;
        vector< complex<double> > emulated_segment = EmulatorState::create_emulated_segment();
        prefixed.idx = (NLCD - (prefix_suffix_symbol_count % NLCD)) % NLCD;
        assert(((prefix_suffix_symbol_count + prefixed.idx) % NLCD) == 0);  // this is not necessary ...
        for (int i=0; i<prefix_suffix_symbol_count; ++i) {
            prefixed.PQAM_DSM_emulate(emulated_segment.data(), prefix[i], brute_force_symbols);  // this is only to feed data
        }
        int emulated_length = (brute_force_symbols + prefix_suffix_symbol_count) * EMULATE_SIZE;
        vector< complex<double> > origin_emulated; origin_emulated.resize(emulated_length);
        vector< complex<double> > changed_emulated; changed_emulated.resize(emulated_length);
        for (uint64_t origin_idx=0; origin_idx<brute_max && running; ++origin_idx) {
            EmulatorState origin_emulator = prefixed;  // copy the states
            for (int i=0; i<brute_force_symbols; ++i) origin[i] = (origin_idx >> (i * prefixed.bit_per_symbol)) & MASK_SYMBOL;
            run_emulation(origin_emulator, origin_emulated, origin, suffix);
            for (uint64_t changed_idx=0; changed_idx<brute_max; ++changed_idx) {
                if (changed_idx == origin_idx) continue;
                EmulatorState changed_emulator = prefixed;  // copy the states
                for (int i=0; i<brute_force_symbols; ++i) changed[i] = (changed_idx >> (i * prefixed.bit_per_symbol)) & MASK_SYMBOL;
                run_emulation(changed_emulator, changed_emulated, changed, suffix);
                double var = 0;
                for (int i=0; i<emulated_length; ++i) {
                    complex<double> d1 = changed_emulated[i] - origin_emulated[i];
                    var += pow(d1.real(), 2) + pow(d1.imag(), 2);
                }
                mut.lock();
                if (mindis.prefix.size() == 0 || var < mindis.var) {
                    printf("\nnew var: %.16f with prefix(%s) origin(%s) changed(%s) suffix(%s)\n"
                            , var, STR(prefix), STR(origin), STR(changed), STR(suffix)); fflush(stdout);
                    mindis.var = var;
                    mindis.prefix = prefix;
                    mindis.origin = origin;
                    mindis.changed = changed;
                    mindis.suffix = suffix;
                }
                mut.unlock();
            }
        }
        mut.lock();
        ++total_round;
        for (int i=0; i<64; ++i) fprintf(stderr, "\b");
        double consumed_cputime = CONSUMED_CPUTIME;
        fprintf(stderr, "round: %d, time: %f %% (%f/%d)", total_round, consumed_cputime / min_time * 100, consumed_cputime, min_time);
        mut.unlock();
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

    if (argc != 8 && argc != 9) {
		printf("usage: <local_filename> <NLCD> <bit_per_symbol_channel> <fire_interval> <duty> <PQAM> <brute_force_symbols> [min_time/s]\n");
        printf("for example: ./Tester/ExploreLCD/EL_200121_MinDistancePQAMDSM.exe 200115_model_500us_17mseq_9v.bin.resampled.reordered 8 1 20 2 1 3\n");
        printf("    min_time: system will at least run a round of brute_force_symbols, however will not run the next round if CPU time exceed min_time\n");
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
    brute_force_symbols = atoi(argv[7]); assert(brute_force_symbols > 0 && "brute_force_symbols should be positive integer");
    min_time = argc > 8 ? atoi(argv[8]) : -1;
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

    assert(brute_force_symbols * BIT_PER_SYMBOL < 32 && "otherwise might be too large to compute");
// start testing
    MinDistance mindis;
    mutex mut;
    vector<thread*> threads;
#define THREAD_COUNT 5
    for (int i=0; i<THREAD_COUNT; ++i) {
        threads.push_back(new thread(compute_random_timeout, ref(mindis), ref(mut)));
    }
    signal(SIGINT, signal_handler);
    for (auto it=threads.begin(); it!=threads.end(); ++it) {
        (*it)->join();
    }

    // // test the behavior of CONSUMED_CPUTIME
    // printf("CONSUMED_CPUTIME: %f\n", CONSUMED_CPUTIME);
    // // volatile int k=0; for (int i=0; i<1000000; ++i) { for (int j=0; j<1000; ++j) { k += 1; } } printf("k: %d\n", k);
    // thread a([]() { volatile int k=0; for (int i=0; i<1000000; ++i) { for (int j=0; j<1000; ++j) { k += 1; } } printf("k: %d\n", k); });
    // thread b([]() { volatile int k=0; for (int i=0; i<1000000; ++i) { for (int j=0; j<1000; ++j) { k += 1; } } printf("k: %d\n", k); });
    // a.join(); b.join();
    // // Sleep(1000);
    // printf("CONSUMED_CPUTIME: %f\n", CONSUMED_CPUTIME);

    mut.lock();
    if (mindis.prefix.size() == 0) {
        printf("\nexit with no result\n"); exit(-1);
    }
    double var = mindis.var;
    vector<uint32_t> prefix = mindis.prefix;
    vector<uint32_t> origin = mindis.origin;
    vector<uint32_t> changed = mindis.changed;
    vector<uint32_t> suffix = mindis.suffix;
    printf("\nmin var: %.16f with prefix(%s) origin(%s) changed(%s) suffix(%s) with %d round\n"
            , var, STR(prefix), STR(origin), STR(changed), STR(suffix), total_round); fflush(stdout);
    fprintf(stderr, "\nmin var: %.16f with prefix(%s) origin(%s) changed(%s) suffix(%s) with %d round\n"
            , var, STR(prefix), STR(origin), STR(changed), STR(suffix), total_round);
    EmulatorState prefixed;
    int emulated_length = (brute_force_symbols + prefix_suffix_symbol_count) * EMULATE_SIZE;
    vector< complex<double> > emulated_segment = EmulatorState::create_emulated_segment();
    vector< complex<double> > origin_emulated; origin_emulated.resize(emulated_length);
    vector< complex<double> > changed_emulated; changed_emulated.resize(emulated_length);
    prefixed.idx = (NLCD - (prefix_suffix_symbol_count % NLCD)) % NLCD;
    for (int i=0; i<prefix_suffix_symbol_count; ++i) {
        prefixed.PQAM_DSM_emulate(emulated_segment.data(), prefix[i], brute_force_symbols);  // this is only to feed data
    }
    EmulatorState origin_emulator = prefixed, changed_emulator = prefixed;
    run_emulation(origin_emulator, origin_emulated, origin, suffix);
    run_emulation(changed_emulator, changed_emulated, changed, suffix);
    vector< complex<float> > emulated;
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

/*

Experiment Results:
(min_time: 60, brute_force_symbols: 2)
DATA_RATE = BIT_PER_SYMBOL * 40e3 / fire_interval
<NLCD> <bit_per_symbol_channel> <fire_interval> <duty> <PQAM>

[1kbps]
1 1 40 1 0: 
1 1 40 

*/
