#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

    if (argc != 2) {
		printf("usage: <local_filename>\n");
        printf("for example: ./Tester/ExploreLCD/EL_200120_TestAccuracy.exe 200115_model_500us_17mseq_9v.bin.resampled.reordered\n");
		return -1;
	}

	const char* local_filename = argv[1];

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

    ifstream file(local_filename, std::ios::binary);
    file.unsetf(std::ios::skipws);  // Stop eating new lines in binary mode!!!
    file.seekg(0, std::ios::end);
    streampos fileSize = file.tellg();
    assert(fileSize == (1 << 17) * 20 * sizeof(complex<float>) && "file size error");
    file.seekg(0, std::ios::beg);
    vector< complex<float> > vec; vec.resize((1 << 17) * 20);
    file.read((char*)vec.data(), fileSize);
    auto ref = [&vec](uint32_t idx) { assert(idx < (1<<17)); return vec.begin() + 20 * idx; };

    // test order from 1 to 16 based on result of 17 order
    for (int order=1; order<=16; ++order) {
        int max_val = 1 << order;
        int max_prefix = 1 << (17 - order);
        double max_var2 = 0;
        double var2_avr = 0;
        double var_avr = 0;
        int max_var_base = -1;
        int max_var_prefix = -1;
        for (int base = 0; base < max_val; ++base) {
            // find the maximum difference with base
            auto base_ref = ref(base);
            for (int prefix = 0; prefix < max_prefix; ++prefix) {
                int var = (prefix << order) | base;
                auto var_ref = ref(var);
                double var2 = 0;
                for (int j=0; j<20; ++j) {
                    complex<float> diff = var_ref[j] - base_ref[j];
                    var2 += pow(diff.real(), 2) + pow(diff.imag(), 2);
                }
                var2 /= 20;
                var_avr += sqrt(var2);
                var2_avr += var2;
                if (var2 > max_var2) {
                    max_var2 = var2;
                    max_var_base = base;
                    max_var_prefix = prefix;
                }
            }
        }
        var2_avr /= (1 << 17);
        var_avr /= (1 << 17);
        printf("order %2d: sqrt(max_var2) = %f, sqrt(var2_avr) = %f, var_avr = %f\n", order, sqrt(max_var2), sqrt(var2_avr), var_avr);
        printf("  "); for (int j=17-order; j>=0; --j) { printf("%d", (0>>j)&1); } 
            for (int j=order; j>=0; --j) { printf("%d", (max_var_base>>j)&1); } printf("\n");
        printf("  "); for (int j=17-order; j>=0; --j) { printf("%d", (max_var_prefix>>j)&1); }
            for (int j=order; j>=0; --j) { printf("%d", (max_var_base>>j)&1); } printf("\n");
    }

	mongodat.close();

    return 0;
}
