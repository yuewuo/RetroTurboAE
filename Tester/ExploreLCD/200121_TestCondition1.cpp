#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

/*
You can download reference file at https://github.com/wuyuepku/RetroTurbo/releases/tag/LCDmodel
Then use ./200121_Produce1kSsRef.cpp converting it to 1S/s version with 8bit sequence

I did that on my computer with 9V: 5E26961CD66B000040000C02
                             5.5V: 5E2696F85E4A000071007242
*/

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

    if (argc != 2) {
		printf("usage: <ref1kSs_id>\n");
        printf("for example: ./Tester/ExploreLCD/EL_200121_TestCondition1.exe 5E26961CD66B000040000C02\n");
		return -1;
	}
    
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	const char* ref1kSs_id_str = argv[1];
    assert(MongoDat::isPossibleOID(ref1kSs_id_str) && "data_id invalid");
    bson_oid_t ref1kSs_id = MongoDat::parseOID(ref1kSs_id_str);
    vector<char> ref1kSs = mongodat.get_binary_file(ref1kSs_id);
    assert(ref1kSs.size() == 256 * 40 * sizeof(complex<float>) && "size error");
    auto ref = [&ref1kSs](uint32_t idx) { assert(idx < 256); return ((const complex<float>*)ref1kSs.data()) + 40 * idx; };

    // start testing
    for (int prefix=0; prefix < 128; ++prefix) {
        for (int origin=0; origin < 256; ++origin) {
            for (int changed=0; changed < 128; ++changed) {
                int changed_1 = changed | (origin & 0x80);  // first bit not changed
                int changed_2 = changed | ((~origin) & 0x80);  // first bit changed
                // when computing the variance, only compute 8bit origin (because prefix isn't changed
                //      , and after origin 8bit there's no difference between 1 and 2)
                double var1 = 0, var2 = 0;
                for (int i=0; i<7; ++i) {
                    int prefix_local = ((prefix & ((1 << i) - 1)) << (8-i));
                    int data_o = (origin >> i) | prefix_local;
                    int data_1 = (changed_1 >> i) | prefix_local;
                    int data_2 = (changed_2 >> i) | prefix_local;
                    auto ref_o = ref(data_o);
                    auto ref_1 = ref(data_1);
                    auto ref_2 = ref(data_2);
                    for (int k=0; k<40; ++k) {
                        complex<float> d1 = ref_1[k] - ref_o[k];
                        complex<float> d2 = ref_2[k] - ref_o[k];
                        var1 += pow(d1.real(), 2) + pow(d1.imag(), 2);
                        var2 += pow(d2.real(), 2) + pow(d2.imag(), 2);
                    }
                }
                // printf("%d: %f %f\n", var1<=var2, var1, var2);
                if ((~origin) & 0x80) printf("%d: %f %f\n", var1<=var2, var1, var2);
            }
        }
    }

    // mongodat.upload_record("ref1kSs", (float*)ref1kSs.data(), 2, ref1kSs.size()
    //     , NULL, "ref1kSs", 1./40, "time(ms)", 1, "I,Q");
    // printf("upload ref1kSs with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	mongodat.close();

    return 0;
}
