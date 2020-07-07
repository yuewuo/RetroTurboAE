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
    bool miller_start;
    bool miller_last_bit;
    int prefix;
    int origin;
    int changed;
    int suffix;
    double var;
};

inline uint32_t miller_encode(uint32_t encoded, bool bit, bool last_bit) {
    bool last_sig = encoded & 1;
    if (bit) {  // If bit is 1, invert the signal at the middle of the bit (this is the delayed transition).
        return (encoded << 2) | (last_sig << 1) | (!last_sig);
    } else {  // If bit is 0 but previous bit was also 0, invert the signal at the edge of the bit, else do not invert the signal.
        if (last_bit) {
            return (encoded << 2) | (last_sig << 1) | (last_sig);
        } else {
            return (encoded << 2) | ((!last_sig) << 1) | (!last_sig);
        }
    }
}

void compute_given_prefix(int prefix_start, int prefix_end, MinDistance& mindis, mutex& mut, vector< complex<float> > &vec) {
    auto ref2k = [&vec](uint32_t idx) { assert(idx < (1<<17)); return vec.begin() + 20 * idx; };
    for (int prefix=prefix_start; prefix<prefix_end; ++prefix) {
        for (int origin=0; origin < 256; ++origin) {
            printf("%3d/128  %3d/256\n", prefix, origin);
            for (int changed=0; changed < 256; ++changed) {
                if (changed != origin) {
                    for (int suffix=0; suffix < 128; ++suffix) {
                        for (uint32_t miller_start=0; miller_start < 2; ++miller_start) {
                            for (uint32_t miller_last_bit=0; miller_last_bit < 2; ++miller_last_bit) {
                                double var = 0;
                                // compute the 7 + 8 + 7 = 22 bit sequence, then use miller coding to compute them
                                uint32_t data_o = (prefix << 15) | (origin << 7) | suffix;
                                uint32_t data_c = (prefix << 15) | (changed << 7) | suffix;
                                uint32_t data[2] = { data_o, data_c };
                                uint32_t encoded[2] = { miller_start, miller_start };
                                bool last_bit[2] = { (bool)miller_last_bit, (bool)miller_last_bit };
                                for (int i=21; i>=15; --i) {  // first encode the prefix
                                    for (int j=0; j<2; ++j) {
                                        bool bit = (data[j]>>i)&1;
                                        encoded[j] = miller_encode(encoded[j], bit, last_bit[j]);
                                        last_bit[j] = bit;
                                    }
                                }
                                for (int i=14; i>=0; --i) {
                                    for (int j=0; j<2; ++j) {
                                        bool bit = (data[j]>>i)&1;
                                        encoded[j] = miller_encode(encoded[j], bit, last_bit[j]);
                                        last_bit[j] = bit;
                                    }
                                    for (int bias=1; bias>=0; --bias) {
                                        auto ref_o = ref2k((encoded[0]>>bias) & MASK17);
                                        auto ref_1 = ref2k((encoded[1]>>bias) & MASK17);
                                        for (int k=0; k<20; ++k) {
                                            complex<float> d1 = ref_1[k] - ref_o[k];
                                            var += pow(d1.real(), 2) + pow(d1.imag(), 2);
                                        }
                                    }
                                }
                                mut.lock();
                                if (mindis.prefix == -1 || var < mindis.var) {
                                    printf("update var: %f with miller_start(%d) miller_last_bit(%d) prefix(%d) origin(%d) changed(%d) suffix(%d)\n"
                                        , var, miller_start, miller_last_bit, prefix, origin, changed, suffix);
                                    mindis.var = var;
                                    mindis.miller_start = miller_start;
                                    mindis.miller_last_bit = miller_last_bit;
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
    }
}

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

    if (argc != 2) {
		printf("usage: <local_filename>\n");
        printf("for example: ./Tester/ExploreLCD/EL_200121_MinDistance2kSsMiller.exe 200115_model_500us_17mseq_9v.bin.resampled.reordered\n");
		return -1;
	}
    
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	const char* local_filename = argv[1];
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
#define CORE_NUM 4
    for (int prefix=0; prefix < 128; prefix += (128/CORE_NUM)) {
        threads.push_back(new thread(compute_given_prefix, prefix, prefix+(128/CORE_NUM), ref(mindis), ref(mut), ref(vec)));
    }
    for (auto it=threads.begin(); it!=threads.end(); ++it) {
        (*it)->join();
    }

    mut.lock();
    bool miller_start = mindis.miller_start; // = 0;
    bool miller_last_bit = mindis.miller_last_bit; // = 0;
    int prefix = mindis.prefix; // = 63;
    int origin = mindis.origin; // = 1;
    int changed = mindis.changed; // = 193;
    int suffix = mindis.suffix; // = 0;
    double var = mindis.var; // = 0.623303;
    printf("minimum var: %f with miller_start(%d) miller_last_bit(%d) prefix(%d) origin(%d) changed(%d) suffix(%d)\n"
        , var, miller_start, miller_last_bit, prefix, origin, changed, suffix);
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
    uint32_t encoded[2] = { miller_start, miller_start };
    bool last_bit[2] = { miller_last_bit, miller_last_bit };
    for (int i=21; i>=15; --i) {
        for (int j=0; j<2; ++j) {
            bool bit = (data[j]>>i)&1;
            encoded[j] = miller_encode(encoded[j], bit, last_bit[j]);
            last_bit[j] = bit;
        }
    }
    for (int i=15; i>=0; --i) {
        for (int j=0; j<2; ++j) {
            bool bit = (data[j]>>i)&1;
            encoded[j] = miller_encode(encoded[j], bit, last_bit[j]);
            last_bit[j] = bit;
        }
        for (int bias=1; bias>=0; --bias) {
            auto ref_o = ref2k((encoded[0]>>bias) & MASK17);
            auto ref_1 = ref2k((encoded[1]>>bias) & MASK17);
            for (int k=0; k<20; ++k) {
                emulated.push_back(ref_o[k]);
                emulated.push_back(ref_1[k]);
            }
        }
    }

    mongodat.upload_record("emulated", (float*)emulated.data(), 4, emulated.size() / 2
        , NULL, "emulated", 1./40, "time(ms)", 1, "Io,Qo,Ic,Qc");
    printf("upload emulated with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	mongodat.close();

    return 0;
}
