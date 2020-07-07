#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
#include <mutex>
#include <thread>
#include <atomic>
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

/*
You can download reference file at https://github.com/wuyuepku/RetroTurbo/releases/tag/LCDmodel
*/

const int MASK17 = (1 << 17) - 1;

struct MinDistance {
    int prefix;
    int origin;
    int changed;
    int suffix;
    double var;
};

int D;
int duty;

// duty should be 1,2,3,4 for 0.5ms, 1.0ms, 1.5ms, 2.0ms ...
inline void DSM_encode(vector<uint32_t>& encoded, bool bit, uint32_t& history, int& DSMidx) {
    int size = encoded.size();
    // assert(duty <= 2 * size && "duty cannot be larger than cycle");  // this should be checked outside
    history = (history << 1) | bit;
    for (int i=0; i<size; ++i) {
        encoded[i] = encoded[i] << 2;
        int last_distance = (size + DSMidx - i) % size;
        int last_bit = (history >> last_distance) & 1;
        if (last_distance*2 < duty) encoded[i] |= last_bit << 1;
        if (last_distance*2 + 1 < duty) encoded[i] |= last_bit;
    }
    DSMidx = (DSMidx + 1) % size;
}

void DSM_print(vector<uint32_t>& encoded) {
    int size = encoded.size();
    for (int i=0; i<size; ++i) {
        for (int j=31; j>=0; --j) {
            printf("%d", (encoded[i]>>j)&1);
        } printf("\n");
    }
}

bool verbose = true;

void compute_given_prefix(int prefix_start, int prefix_end, MinDistance& mindis, mutex& mut, vector< complex<float> > &vec) {
    auto ref2k = [&vec](uint32_t idx) { assert(idx < (1<<17)); return vec.begin() + 20 * idx; };
    for (int prefix=prefix_start; prefix<prefix_end; ++prefix) {
        if (verbose) { printf("%3d/128\n", prefix); fflush(stdout); }
        for (int origin=0; origin < 256; ++origin) {
            // if (verbose) printf("%3d/128  %3d/256\n", prefix, origin);
            for (int changed=0; changed < 256; ++changed) {
                if (changed != origin) {
                    for (int suffix=0; suffix < 128; ++suffix) {
                        double var = 0;
                        // compute the 7 + 8 + 7 = 22 bit sequence, then use miller coding to compute them
                        uint32_t data_o = (prefix << 15) | (origin << 7) | suffix;
                        uint32_t data_c = (prefix << 15) | (changed << 7) | suffix;
                        uint32_t data[2] = { data_o, data_c };
                        vector<uint32_t> encoded[2]; for (int i=0; i<2; ++i) encoded[i].insert(encoded[i].end(), D, 0);
                        uint32_t history[2] = { 0, 0 };
                        int DSMidx[2] = { 0, 0 };
                        for (int i=21; i>=15; --i) {  // first encode the prefix
                            for (int j=0; j<2; ++j) {
                                bool bit = (data[j]>>i)&1;
                                DSM_encode(encoded[j], bit, history[j], DSMidx[j]);
                            }
                        }
                        for (int i=14; i>=0; --i) {
                            for (int j=0; j<2; ++j) {
                                bool bit = (data[j]>>i)&1;
                                DSM_encode(encoded[j], bit, history[j], DSMidx[j]);
                            }
                            for (int bias=1; bias>=0; --bias) {
                                for (int k=0; k<20; ++k) {
                                    complex<double> d1;
                                    for (int j=0; j<D; ++j) {
                                        auto ref_o = ref2k((encoded[0][j]>>bias) & MASK17);
                                        auto ref_1 = ref2k((encoded[1][j]>>bias) & MASK17);
                                        d1 += ref_1[k] - ref_o[k];
                                    } d1 /= (double)D;  // each pixel is of 1/D size
                                    var += pow(d1.real(), 2) + pow(d1.imag(), 2);
                                }
                            }
                        }
                        mut.lock();
                        if (mindis.prefix == -1 || var < mindis.var) {
                            if (verbose) printf("update var: %f with prefix(%d) origin(%d) changed(%d) suffix(%d)\n"
                                    , var, prefix, origin, changed, suffix); fflush(stdout);
                            mindis.var = var;
                            mindis.prefix = prefix;
                            mindis.origin = origin;
                            mindis.changed = changed;
                            mindis.suffix = suffix;
                        }
                        mut.unlock();
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

    if (argc != 4 && argc != 5) {
		printf("usage: <local_filename> <D> <duty> [s|silence]\n");
        printf("for example: ./Tester/ExploreLCD/EL_200121_MinDistance1kbpsDSM.exe 200115_model_500us_17mseq_9v.bin.resampled.reordered 4 3\n");
		return -1;
	}

	MongoDat::LibInit();
	if (MONGO_URL[0]) mongodat.open(MONGO_URL, MONGO_DATABASE);

	const char* local_filename = argv[1];
    D = atoi(argv[2]); assert(D > 1 && "D must be greater than 1");
    duty = atoi(argv[3]); assert(duty > 0 && duty <= 2 * D && "duty cannot exceed 2D");
    if (argc == 5) verbose = false;

    ifstream file(local_filename, std::ios::binary);
    file.unsetf(std::ios::skipws);  // Stop eating new lines in binary mode!!!
    file.seekg(0, std::ios::end);
    streampos fileSize = file.tellg();
    assert(fileSize == (1 << 17) * 20 * sizeof(complex<float>) && "file size error");
    file.seekg(0, std::ios::beg);
    vector< complex<float> > vec; vec.resize((1 << 17) * 20);
    file.read((char*)vec.data(), fileSize);

    // start testing
    MinDistance mindis;
    mindis.prefix = -1;
    mutex mut;
    vector<thread*> threads;
#define CORE_NUM 8
    for (int prefix=0; prefix < 128; prefix += (128/CORE_NUM)) {
        threads.push_back(new thread(compute_given_prefix, prefix, prefix+(128/CORE_NUM), ref(mindis), ref(mut), ref(vec)));
    }
    for (auto it=threads.begin(); it!=threads.end(); ++it) {
        (*it)->join();
    }

    mut.lock();
    int prefix = mindis.prefix;  // = 95;
    int origin = mindis.origin;  // = 0;
    int changed = mindis.changed;  // = 64;
    int suffix = mindis.suffix;  // = 0;
    double var = mindis.var;  // = 11.515078;
    printf("minimum var: %f with prefix(%d) origin(%d) changed(%d) suffix(%d)\n"
        , var, prefix, origin, changed, suffix);
    printf("origin: ");
        for (int j=7; j>=0; --j) { printf("%d", (prefix>>j)&1); }
        for (int j=8; j>=0; --j) { printf("%d", (origin>>j)&1); }
        for (int j=7; j>=0; --j) { printf("%d", (suffix>>j)&1); } printf("\n");
    printf("change: ");
        for (int j=7; j>=0; --j) { printf("%d", (prefix>>j)&1); }
        for (int j=8; j>=0; --j) { printf("%d", (changed>>j)&1); }
        for (int j=7; j>=0; --j) { printf("%d", (suffix>>j)&1); } printf("\n");

    auto ref2k = [&vec](uint32_t idx) { assert(idx < (1<<17)); return vec.begin() + 20 * idx; };
    vector< complex<float> > emulated;
    // compute the 7 + 8 + 7 = 22 bit sequence, then use miller coding to compute them
    uint32_t data_o = (prefix << 15) | (origin << 7) | suffix;
    uint32_t data_c = (prefix << 15) | (changed << 7) | suffix;
    uint32_t data[2] = { data_o, data_c };
    vector<uint32_t> encoded[2]; for (int i=0; i<2; ++i) encoded[i].insert(encoded[i].end(), D, 0);
    uint32_t history[2] = { 0, 0 };
    int DSMidx[2] = { 0, 0 };
    for (int i=21; i>=15; --i) {  // first encode the prefix
        for (int j=0; j<2; ++j) {
            bool bit = (data[j]>>i)&1;
            DSM_encode(encoded[j], bit, history[j], DSMidx[j]);
        }
    }
    for (int i=14; i>=0; --i) {
        for (int j=0; j<2; ++j) {
            bool bit = (data[j]>>i)&1;
            DSM_encode(encoded[j], bit, history[j], DSMidx[j]);
        }
        for (int bias=1; bias>=0; --bias) {
            for (int k=0; k<20; ++k) {
                complex<float> point_o;
                complex<float> point_c;
                for (int j=0; j<D; ++j) {
                    auto ref_o = ref2k((encoded[0][j]>>bias) & MASK17);
                    auto ref_1 = ref2k((encoded[1][j]>>bias) & MASK17);
                    point_o += ref_1[k];
                    point_c += ref_o[k];
                }
                point_o /= (float)D;  // each pixel is of 1/D size
                point_c /= (float)D;
                emulated.push_back(point_o);
                emulated.push_back(point_c);
            }
        }
    }

    if (MONGO_URL[0]) {
        mongodat.upload_record("emulated", (float*)emulated.data(), 4, emulated.size() / 2
            , NULL, "emulated", 1./40, "time(ms)", 1, "Io,Qo,Ic,Qc");
        printf("upload emulated with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

        mongodat.close();
    }

    return 0;
}
