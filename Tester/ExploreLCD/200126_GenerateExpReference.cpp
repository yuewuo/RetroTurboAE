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
		printf("usage: <charging_edge_t0/ms> <discharging_edge_t0/ms>\n");
        printf("    generating a 2^17 sequence reference at 40kS/s and 0.5ms long each\n");
		return -1;
	}

	double charging_edge_t0 = atof(argv[1]);
    double discharging_edge_t0 = atof(argv[2]);

    vector<bool> mseq17 = generate_m_sequence(17);
    int mseq17len = mseq17.size();
    printf("mseq17 length: %d\n", mseq17len);

	double orate = 40000;

    int vec_size = (1 << 17) * 20;
    vector< complex<float> > vec; vec.resize(vec_size);
    float charging_rate = exp(- 1/40. / charging_edge_t0);
    float discharging_rate = exp(-1/40. / discharging_edge_t0);
    for (int j=0; j<3; ++j) {  // 3 repeat to simulate more accurately
        for (int i=20; i<vec_size; ++i) {
            int last_idx = ((vec_size-20) + i - 1) % (vec_size-20) + 20;
            float last_real = vec[last_idx].real();
            bool now_signal = mseq17[(i/20-1) % mseq17.size()];
            float this_real = now_signal ? 1 - (1-last_real) * charging_rate : last_real * discharging_rate;
            if (this_real > 1) { this_real = 1; } if (this_real < 0) { this_real = 0; } 
            vec[i % (vec_size-20) + 20] = complex<float>(this_real, 0);
        }
    }

    mongodat.upload_record("vec", (float*)vec.data(), 2, vec.size()
        , NULL, "vec", 1000./orate, "time(ms)", 1, "I,Q");
    printf("upload vec with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

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

    int interval = 0.5 * orate / 1000 + 0.1;
    assert(interval == 20);
    vector< vector< complex<float> > > pieces;
    pieces.resize(2 << 17);
    for (int i=1; i<=mseq17len; ++i) {  // record those pieces
        int starting_bias = (rev_map[i]+1) * interval;
        assert(pieces[i].size() == 0 && "this piece shouldn't be used now !!!");
        pieces[i].resize(interval);
        for (int k=0; k<interval; ++k) {
            pieces[i][k] += vec[starting_bias + k];
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

    char local_filename[256]; sprintf(local_filename, "simu_%dus_%dus.reordered", (int)(1e3*charging_edge_t0), (int)(1e3*discharging_edge_t0));
    ofstream output(local_filename, ios::binary);
    output.write((const char*)reordered.data(), reordered.size() * sizeof(complex<float>));
    output.close();

    mongodat.upload_record("reordered", (float*)reordered.data(), 2, reordered.size()
        , NULL, "reordered", 1000./orate, "time(ms)", 1, "I,Q");
    printf("upload reordered with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	mongodat.close();

    return 0;
}
