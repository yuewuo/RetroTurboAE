#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
#include <complex>
#include <iterator>
#include "soxr/soxr.h"
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);
    
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 3) {
		printf("usage: <local_filename> <out_filename>\n");
        printf("    only support local files, because data should be hundreds of megabytes and too large for handling\n");
        printf("for example: ./Tester/ExploreLCD/EL_200224_Regenerate8kSsRef17Mseq.exe 200115_model_500us_17mseq_9v.bin.resampled.reordered ref17mseq8kSs.9v.bin\n");
		return -1;
	}

	const char* local_filename = argv[1];
    const char* out_filename = argv[2];

    int irate = 40000;
#define orate (2*irate)

    vector<bool> mseq17 = generate_m_sequence(17);
    int mseq17len = mseq17.size();
    printf("mseq17 length: %d\n", mseq17len);

    ifstream file(local_filename, std::ios::binary);
    file.unsetf(std::ios::skipws);  // Stop eating new lines in binary mode!!!
    file.seekg(0, std::ios::end);
    streampos fileSize = file.tellg();
    int slice_length = irate / 2000;
    int slice_cnt = 1 << 17;
    int read_count = slice_length * slice_cnt;
    assert((int)fileSize == read_count * (int)sizeof(complex<float>) && "filesize error");
    file.seekg(0, std::ios::beg);
    vector< complex<float> > vec; vec.resize(read_count);
    file.read((char*)vec.data(), fileSize);
    printf("vec.size() = %d\n", (int)vec.size());

    vector< complex<float> > doubled;
    for (int i=0; i<slice_cnt; ++i) {
        int bias = i * slice_length;
        for (int j=0; j<slice_length-1; ++j) {
            doubled.insert(doubled.end(), vec[bias+j]);
            doubled.insert(doubled.end(), (vec[bias+j] + vec[bias+j+1])/2.f);
        }
        int j = slice_length-1;
        doubled.insert(doubled.end(), vec[bias+j]);
        doubled.insert(doubled.end(), vec[bias+j] + (vec[bias+j] - vec[bias+j-1])/2.f);
    }
    assert(doubled.size() == 2 * vec.size());

    ofstream output(out_filename, ios::binary);
    output.write((const char*)doubled.data(), doubled.size() * sizeof(complex<float>));
    output.close();

    mongodat.upload_record("doubled", (float*)doubled.data(), 2, doubled.size()
        , NULL, "doubled", 1000./orate, "time(ms)", 1, "I,Q");
    printf("upload doubled with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	mongodat.close();

    return 0;
}
