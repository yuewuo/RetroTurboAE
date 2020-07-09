#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

    if (argc != 3 && argc != 5) {
		printf("usage: <local_filename> <sequence> [collection] [id(12 byte hex = 24 char)]\n");
        printf("for example: ./Tester/ExploreLCD/EL_200121_Emulator.exe 200115_model_500us_17mseq_9v.bin.resampled.reordered 00110000110001000000\n");
		return -1;
	}

	const char* local_filename = argv[1];
    const char* sequence = argv[2];

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

    vector<bool> biseq;
    for (const char* it = sequence; *it; ++it) {
        assert((*it == '0' || *it == '1') && "sequence should be compossed of 0 or 1");
        biseq.push_back(*it == '1');
    }

    vector< complex<float> > emulated;
    for (int i=0; i<(int)biseq.size(); ++i) {
        int idx = 0;
        for (int j=0; j<17; ++j) idx |= ((i-j<0?0:biseq[i-j])) << j;
        // printf("idx: "); for (int j=16; j>=0; --j) { printf("%d", (idx>>j)&1); } printf("\n");
        auto r = ref(idx);
        emulated.insert(emulated.end(), r, r+20);
    }

    mongodat.upload_record("emulated", (float*)emulated.data(), 2, emulated.size()
        , NULL, "emulated", 1./40, "time(ms)", 1, "I,Q");
    printf("upload emulated with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    
    if (argc > 4) {
        const char* collection_str = argv[3];
        const char* record_id_str = argv[4];
	    BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
        record["emulated_id"] = MongoDat::OID2str(mongodat.get_fileID());
        record["emulated_seq"] = sequence;
        record.save();
    }

	mongodat.close();

    return 0;
}
