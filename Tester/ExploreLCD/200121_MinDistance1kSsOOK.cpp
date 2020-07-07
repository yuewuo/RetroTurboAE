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
Then use ./200121_Produce1kSsRef.cpp converting it to 1S/s version with 8bit sequence

I did that on my computer with 9V: 5E26961CD66B000040000C02
                             5.5V: 5E2696F85E4A000071007242
*/

struct MinDistance {
    int prefix;
    int origin;
    int changed;
    int suffix;
    double var;
};

void compute_given_prefix(int prefix_start, int prefix_end, MinDistance& mindis, mutex& mut, vector<char> &ref1kSs) {
    auto ref1k = [&ref1kSs](uint32_t idx) { assert(idx < 256); return ((const complex<float>*)ref1kSs.data()) + 40 * idx; };
    for (int prefix=prefix_start; prefix<prefix_end; ++prefix) {
        for (int origin=0; origin < 256; ++origin) {
            printf("%3d/128  %3d/256\n", prefix, origin);
            for (int changed=0; changed < 256; ++changed) {
                if (changed != origin) {
                    for (int suffix=0; suffix < 128; ++suffix) {
                        double var = 0;
                        // first compute region of the modified bits
                        for (int i=0; i<8; ++i) {
                            int prefix_local = (prefix & ((1 << i) - 1)) << (8-i);
                            int data_o = (origin >> i) | prefix_local;
                            int data_1 = (changed >> i) | prefix_local;
                            auto ref_o = ref1k(data_o);
                            auto ref_1 = ref1k(data_1);
                            for (int k=0; k<40; ++k) {
                                complex<float> d1 = ref_1[k] - ref_o[k];
                                var += pow(d1.real(), 2) + pow(d1.imag(), 2);
                            }
                        }
                        // then compute the tail 7bits
                        for (int i=0; i<7; ++i) {
                            int suffix_local = suffix >> i;
                            int data_o = ((origin & ((2 << i) - 1)) << (7-i)) | suffix_local;
                            int data_1 = ((changed & ((2 << i) - 1)) << (7-i)) | suffix_local;
                            auto ref_o = ref1k(data_o);
                            auto ref_1 = ref1k(data_1);
                            for (int k=0; k<40; ++k) {
                                complex<float> d1 = ref_1[k] - ref_o[k];
                                var += pow(d1.real(), 2) + pow(d1.imag(), 2);
                            }
                        }
                        mut.lock();
                        if (mindis.prefix == -1 || var < mindis.var) {
                            printf("update var: %f with prefix(%d) origin(%d) changed(%d) suffix(%d)\n", var, prefix, origin, changed, suffix);
                            mindis.var = var;
                            mindis.prefix = prefix;
                            mindis.origin = origin;
                            mindis.changed = changed;
                            mindis.suffix = suffix;
                        }
                        mut.unlock();
                        // printf("%d: %f %f\n", var1<=var2, var1, var2);
                        // if ((~origin) & 0x80) printf("%d: %f %f\n", var1<=var2, var1, var2);
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

    if (argc != 2) {
		printf("usage: <ref1kSs_id>\n");
        printf("for example: ./Tester/ExploreLCD/EL_200121_MinDistance1kSsOOK.exe 5E26961CD66B000040000C02\n");
		return -1;
	}
    
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	const char* ref1kSs_id_str = argv[1];
    assert(MongoDat::isPossibleOID(ref1kSs_id_str) && "data_id invalid");
    bson_oid_t ref1kSs_id = MongoDat::parseOID(ref1kSs_id_str);
    vector<char> ref1kSs = mongodat.get_binary_file(ref1kSs_id);
    assert(ref1kSs.size() == 256 * 40 * sizeof(complex<float>) && "size error");

    // start testing
    MinDistance mindis;
    mindis.prefix = -1;
    mutex mut;
    vector<thread*> threads;
#define CORE_NUM 4
    for (int prefix=0; prefix < 128; prefix += (128/CORE_NUM)) {
        threads.push_back(new thread(compute_given_prefix, prefix, prefix+(128/CORE_NUM), ref(mindis), ref(mut), ref(ref1kSs)));
    }
    for (auto it=threads.begin(); it!=threads.end(); ++it) {
        (*it)->join();
    }

    mut.lock();
    int prefix = mindis.prefix;  // = 53;  // for testing the following functions
    int origin = mindis.origin;  // = 37;
    int changed = mindis.changed;  // = 39;
    int suffix = mindis.suffix;  // = 54;
    double var = mindis.var;  // = 0.613825;
    printf("minimum var: %f with prefix(%d) origin(%d) changed(%d) suffix(%d)\n", var, prefix, origin, changed, suffix);
    printf("origin: ");
        for (int j=7; j>=0; --j) { printf("%d", (prefix>>j)&1); }
        for (int j=8; j>=0; --j) { printf("%d", (origin>>j)&1); }
        for (int j=7; j>=0; --j) { printf("%d", (suffix>>j)&1); } printf("\n");
    printf("change: ");
        for (int j=7; j>=0; --j) { printf("%d", (prefix>>j)&1); }
        for (int j=8; j>=0; --j) { printf("%d", (changed>>j)&1); }
        for (int j=7; j>=0; --j) { printf("%d", (suffix>>j)&1); } printf("\n");

    auto ref1k = [&ref1kSs](uint32_t idx) { assert(idx < 256); return ((const complex<float>*)ref1kSs.data()) + 40 * idx; };
    vector< complex<float> > emulated;
    // first compute region of the modified bits
    for (int i=7; i>=0; --i) {
        int prefix_local = (prefix & ((1 << i) - 1)) << (8-i);
        int data_o = (origin >> i) | prefix_local;
        int data_1 = (changed >> i) | prefix_local;
        auto ref_o = ref1k(data_o);
        auto ref_1 = ref1k(data_1);
        for (int k=0; k<40; ++k) {
            emulated.push_back(ref_o[k]);
            emulated.push_back(ref_1[k]);
        }
    }
    // then compute the tail 7bits
    for (int i=6; i>=0; --i) {
        int suffix_local = suffix >> i;
        int data_o = ((origin & ((2 << i) - 1)) << (7-i)) | suffix_local;
        int data_1 = ((changed & ((2 << i) - 1)) << (7-i)) | suffix_local;
        auto ref_o = ref1k(data_o);
        auto ref_1 = ref1k(data_1);
        for (int k=0; k<40; ++k) {
            emulated.push_back(ref_o[k]);
            emulated.push_back(ref_1[k]);
        }
    }

    mongodat.upload_record("emulated", (float*)emulated.data(), 4, emulated.size() / 2
        , NULL, "emulated", 1./40, "time(ms)", 1, "Io,Qo,Ic,Qc");
    printf("upload emulated with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	mongodat.close();

    return 0;
}
