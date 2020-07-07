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

	if (argc != 4) {
		printf("usage: <local_filename> <bias/ms> <count>\n");
        printf("    only support local files, because data should be hundreds of megabytes and too large for handling\n");
        printf("    bias should be the starting edge of first preamble (charging edge)\n");
        printf("for example: ./Tester/ExploreLCD/EL_200118_PostGenerateRef17Mseq.exe 200115_model_500us_17mseq_9v.bin.resampled 241.6 9\n");
		return -1;
	}

	const char* local_filename = argv[1];
    double bias = atof(argv[2]);
    int count = atof(argv[3]);

    vector<bool> mseq17 = generate_m_sequence(17);
    int mseq17len = mseq17.size();
    printf("mseq17 length: %d\n", mseq17len);

	double orate = 40000;

    int read_count = (bias + 16 + count * (mseq17len * 0.5)) / 1000. * orate;
    printf("read_count: %d\n", read_count);
    int read_bytes = read_count * 2 * sizeof(float);
    printf("read_bytes: %d\n", read_bytes);

    ifstream file(local_filename, std::ios::binary);
    file.unsetf(std::ios::skipws);  // Stop eating new lines in binary mode!!!
    file.seekg(0, std::ios::end);
    streampos fileSize = file.tellg();
    assert(fileSize >= read_bytes && "cannot read enough data to process");
    file.seekg(0, std::ios::beg);
    vector< complex<float> > vec; vec.resize(read_count);
    file.read((char*)vec.data(), read_bytes);
    printf("vec.size() = %d\n", (int)vec.size());

    // then build the search map
    vector<int> rev_map;  // indicate where to find the segment
    rev_map.insert(rev_map.begin(), 2 << 17, -1);
    for (int i=0; i<mseq17len; ++i) {
        uint32_t a = mseq17[i];
        for (int j=1; j<17; ++j) {
            a |= mseq17[ (mseq17len + i - j) % mseq17len] << j;
        }
        assert(a > 0 && a < rev_map.size());
        rev_map[a] = i;
    }
    // sanity check
    assert(rev_map[0] == -1);
    for (int i=1; i<mseq17len; ++i) assert(rev_map[i] != -1);
    // this indicates that all pieces are matched, which is exactly the behavior of m-sequence

    int starting_record = ( bias + 16 ) / 1000. * orate;
    int big_interval = mseq17len * 0.5 / 1000. * orate + 0.1;
    int interval = 0.5 * orate / 1000 + 0.1;
    assert(interval == 20);
    vector< vector< complex<float> > > pieces;
    pieces.resize(2 << 17);
    for (int i=1; i<=mseq17len; ++i) {  // record those pieces
        int starting_bias = starting_record + rev_map[i] * interval;
        assert(pieces[i].size() == 0 && "this piece shouldn't be used now !!!");
        pieces[i].resize(interval);
        for (int j=1; j<count; ++j) {  // do not use the first m-sequence
            int j_bias = starting_bias + j * big_interval;
            assert(j_bias < read_count);
            for (int k=0; k<interval; ++k) {
                pieces[i][k] += vec[j_bias + k];
            }
        }
        for (int k=0; k<interval; ++k) {
            pieces[i][k] /= (float)(count - 1);
        }
    }

    // the final step is to emulate the sequence 0000....000 which does not appear in 17th-order m-sequence
    // to do this, we use a tricky that connects the tail of 1000....000 and head of 0000....001
    complex<float> tail_1000 = pieces[1 << 16][interval-1];
    complex<float> head_0001 = pieces[1][0];
    printf("tail_1000: %f + %f j\n", tail_1000.real(), tail_1000.imag());
    printf("head_0001: %f + %f j\n", head_0001.real(), head_0001.imag());
    printf("tail_1000 and head_0001 should both be around 0+0j, thus use linear interpolation between them\n");
    assert(pieces[0].size() == 0 && "this piece shouldn't be used now !!!");
    pieces[0].resize(interval);
    for (int k=0; k<interval; ++k) {
        pieces[0][k] = tail_1000 * (1-k/20.f) + head_0001 * (k/20.f);
    }

    vector< complex<float> > reordered;
    reordered.resize(interval * (1 << 17));
    for (int i=0; i<=mseq17len; ++i) {
        assert((int)pieces[i].size() == interval && "this piece should have data now");
        for (int k=0; k<interval; ++k) {
            reordered[i*interval+k] = pieces[i][k];
        }
    }

    ofstream output((string(local_filename) + ".reordered").c_str(), ios::binary);
    output.write((const char*)reordered.data(), reordered.size() * sizeof(complex<float>));
    output.close();

    mongodat.upload_record("reordered", (float*)reordered.data(), 2, reordered.size()
        , NULL, "reordered", 1000./orate, "time(ms)", 1, "I,Q");
    printf("upload reordered with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	mongodat.close();

    return 0;
}
