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
        printf("for example: ./Tester/ExploreLCD/EL_200121_Produce1kSsRef.exe 200115_model_500us_17mseq_9v.bin.resampled.reordered\n");
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

    vector< complex<float> > ref1kSs;
    for (int i=0; i<256; ++i) {
        int idx = 0;
        for (int j=0; j<16; ++j) {
            idx |= ((i>>(j/2))&1) << j;
        }
        // printf("  "); for (int j=18; j>=0; --j) { printf("%d", (idx>>j)&1); } printf("\n");
        auto ref1 = ref(idx >> 1);
        auto ref2 = ref(idx);
        ref1kSs.insert(ref1kSs.end(), ref1, ref1+20);
        ref1kSs.insert(ref1kSs.end(), ref2, ref2+20);
    }

    mongodat.upload_record("ref1kSs", (float*)ref1kSs.data(), 2, ref1kSs.size()
        , NULL, "ref1kSs", 1./40, "time(ms)", 1, "I,Q");
    printf("upload ref1kSs with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	mongodat.close();

    return 0;
}
