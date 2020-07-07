#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
#include "modulator.h"
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;
FastDSM_Encoder encoder;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

    if (argc != 2 && argc != 3) {
		printf("usage: <local_filename> [byte_count:12]\n");
        printf("for example: ./Tester/ExploreLCD/EL_200606_Demonstrate.exe 200115_model_500us_17mseq_9v.bin.resampled.reordered\n");
		return -1;
	}

	const char* local_filename = argv[1];
    int byte_count = argc > 2 ? atoi(argv[2]) : 12;

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

    encoder.NLCD = 4;
    // encoder.NLCD = 8;
#define NLCD (encoder.NLCD)
    encoder.ct_fast = 0;
    encoder.ct_slow = 0;
    encoder.combine = 1;
    encoder.cycle = 8;
    encoder.duty = 2;
    encoder.bit_per_symbol = 4;
    encoder.channel_training_type = 0;
    encoder.frequency = 2000;
    vector<uint8_t> data;
    data.resize(byte_count);

    // generate random data
    struct timeval time;
	gettimeofday(&time, NULL);
	mt19937 gen(1000000 * time.tv_sec + time.tv_usec);
	uniform_int_distribution<unsigned> dis(0, 255);
    printf("data:"); for (int i=0; i<byte_count; ++i) {
		data[i] = dis(gen);
        printf(" %02X", data[i]);
	} printf("\n");

    encoder.build_preamble();
    encoder.build_channel_training();
    encoder.encode(data);
    // encoder.dump(encoder.o_encoded_data);
    vector<Tag_Sample_t>& samples = encoder.o_encoded_data;

    // output LCD * 2(IQ) * 2pixel
    vector< vector< float > > pixels_out;
    pixels_out.resize(4 * NLCD);
    for (int i=0; i<NLCD; ++i) {
        for (int j=0; j < (int)samples.size(); ++j) {
#define EMULATE_ONE(mask, bias, ratio)\
            {\
                int idx = 0;\
                for (int k=0; k<17; ++k) idx |= ((j-k<0?0:(!!(samples[j-k].le(i)&mask)))) << k;\
                auto r = ref(idx);\
                vector< float >& emulated = pixels_out[4*i + bias];\
                for (int k=0; k<20; ++k) {\
                    emulated.push_back(ratio * (r+k)->real());\
                }\
            }
            EMULATE_ONE(0x01, 0, 2);
            EMULATE_ONE(0x02, 1, 1);
            EMULATE_ONE(0x10, 2, 2);
            EMULATE_ONE(0x20, 3, 1);
        }
    }
    for (int i=0; i<4*NLCD; ++i) {
        assert(pixels_out[i].size() == 20 * samples.size());
    }

    vector< float > individual_plot;
    for (int j=0; j<20 * (int)samples.size(); ++j) {
        for (int i=0; i<4*NLCD; ++i) {
            individual_plot.push_back(pixels_out[i][j]);
        }
    }
    mongodat.upload_record("individual_plot", (float*)individual_plot.data(), 4*NLCD, 20 * (int)samples.size()
        , NULL, "individual_plot", 1./40, "time(ms)", 1, 
        NLCD == 8 ? "A0,B0,C0,D0,A1,B1,C1,D1,A2,B2,C2,D2,A3,B3,C3,D3,A4,B4,C4,D4,A5,B5,C5,D5,A6,B6,C6,D6,A7,B7,C7,D7" :
        "A0,B0,C0,D0,A1,B1,C1,D1,A2,B2,C2,D2,A3,B3,C3,D3"
    );
    printf("upload individual_plot with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

    vector<float> all_plot;
    for (int j=0; j<20 * (int)samples.size(); ++j) {
        all_plot.push_back(0);
        all_plot.push_back(0);
        for (int i=0; i<4*NLCD; ++i) {
            if ((i%4) < 2) {
                all_plot[all_plot.size()-2] += pixels_out[i][j];
            } else all_plot[all_plot.size()-1] += pixels_out[i][j];
        }
    }
    mongodat.upload_record("all_plot", (float*)all_plot.data(), 2, 20 * (int)samples.size()
        , NULL, "all_plot", 1./40, "time(ms)", 1, "I,Q");
    printf("upload all_plot with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

    // output to python format
    ofstream pyout("demo_data.py");
    pyout << "data = [" << endl;
    for (int i=0; i<4*NLCD; ++i) {
        pyout << "    [ ";
        for (int j=0; j<20 * (int)samples.size(); ++j) {
            pyout << pixels_out[i][j];
            if (j + 1 <20 * (int)samples.size()) pyout << " , ";
        }
        pyout << " ]";
        if (i+1 < 4*NLCD) pyout << ",";
        pyout << endl;
    }
    pyout << "]" << endl;

	mongodat.close();

    return 0;
}
