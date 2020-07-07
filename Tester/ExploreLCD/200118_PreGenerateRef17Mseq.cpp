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
        printf("for example: ./Tester/ExploreLCD/EL_200118_PreGenerateRef17Mseq.exe 200115_model_500us_17mseq_9v.bin 4342.0 9\n");
		return -1;
	}

	const char* local_filename = argv[1];
    double bias = atof(argv[2]);
    int count = atof(argv[3]);

    vector<bool> mseq17 = generate_m_sequence(17);
    printf("mseq17 length: %d\n", (int)mseq17.size());
    
	double irate = 56875;
	double orate = 40000;

    int read_count = (bias + 10000 + count * (mseq17.size() * 0.5)) / 1000. * irate;
    printf("read_count: %d\n", read_count);
    int read_bytes = read_count * 4 * sizeof(int16_t);
    printf("read_bytes: %d\n", read_bytes);

    ifstream file(local_filename, std::ios::binary);
    file.unsetf(std::ios::skipws);  // Stop eating new lines in binary mode!!!
    file.seekg(0, std::ios::end);
    streampos fileSize = file.tellg();
    assert(fileSize >= read_bytes && "cannot read enough data to process");
    file.seekg(0, std::ios::beg);
    vector<char> vec; vec.resize(read_bytes);
    file.read(vec.data(), read_bytes);
    printf("vec.size() = %d\n", (int)vec.size());
    const int16_t* vec_dat = (const int16_t*)vec.data();

    // first rotate data according to preamble, user should align bias to the start of raising edge
    int bias_idx = (bias) / 1000. * irate;
    float bias_idx_interval = 16 / 1000. * irate;
    int avr_bias_each = 6 / 1000. * irate;
    int avr_count_each = 10 / 1000. * irate;
    complex<double> avr1s[2], avr0s[2];
    printf("uploading...\n");
    printf("bias_idx: %d\n", bias_idx);
    // mongodat.upload_record("debug", (int16_t*)(vec_dat+bias_idx*4), 4, bias_idx_interval*7*2
    //     , NULL, "debug", 1/56.875, "time(ms)", 1, "Ia,Qa,Ib,Qb");
    // printf("upload debug with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    for (int i=0; i<7; ++i) {
        int bias_1s = 4 * (int)(bias_idx + i*2*bias_idx_interval + avr_bias_each);
        int bias_0s = bias_1s + 4 * (int)bias_idx_interval;
        for (int j=0; j<avr_count_each; ++j) {
            for (int k=0; k<2; ++k) {
                avr1s[k] += complex<double>(vec_dat[bias_1s+j*4+k*2], vec_dat[bias_1s+j*4+1+k*2]);
                avr0s[k] += complex<double>(vec_dat[bias_0s+j*4+k*2], vec_dat[bias_0s+j*4+1+k*2]);
            }
        }
    }
    for (int k=0; k<2; ++k) {
        avr1s[k] /= (double)(avr_count_each*7);
        avr0s[k] /= (double)(avr_count_each*7);
        printf("avr1s[%d]: %f + %fj\n", k, avr1s[k].real(), avr1s[k].imag());
        printf("avr0s[%d]: %f + %fj\n", k, avr0s[k].real(), avr0s[k].imag());
    }
    vector< complex<float> > rotated;
    rotated.resize(read_count);
    complex<float> avrdelta[2];
    float avrdelta_m[2];
    for (int i=0; i<2; ++i) {
        avrdelta[i] = avr1s[i] - avr0s[i];
        avrdelta_m[i] = sqrt(pow(avrdelta[i].real(), 2) + pow(avrdelta[i].imag(), 2));
    }

    // rotate it
    for (int i=0; i<read_count; ++i) {
        double Ia = vec_dat[4*i + 0] - avr0s[0].real();
        double Qa = vec_dat[4*i + 1] - avr0s[0].imag();
        double Ib = vec_dat[4*i + 2] - avr0s[1].real();
        double Qb = vec_dat[4*i + 3] - avr0s[1].imag();
        double I = (Ia * avrdelta[0].real() + Qa * avrdelta[0].imag()) / avrdelta_m[0];
        double Q = (Ib * avrdelta[1].real() + Qb * avrdelta[1].imag()) / avrdelta_m[1];
        double unit = 2 * avrdelta_m[0] * avrdelta_m[1];
        double Ir = (I * avrdelta_m[1] + Q * avrdelta_m[0]) / unit;
        double Qr = (I * avrdelta_m[1] - Q * avrdelta_m[0]) / unit;
        rotated[i] = complex<float>(Ir, Qr);
    }
    mongodat.upload_record("debug", (float*)(rotated.data()+bias_idx), 2, bias_idx_interval*7*2
        , NULL, "debug", 1/56.875, "time(ms)", 1, "I,Q");
    printf("upload debug with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    
    ofstream output((string(local_filename) + ".rotated").c_str(), ios::binary);
    output.write((const char*)(rotated.data()+bias_idx-1000), (rotated.size()-bias_idx+1000) * sizeof(complex<float>));
    output.close();

    // mongodat.upload_record("rotated", (float*)rotated.data(), 2, rotated.size()
    //     , NULL, "resampled", 1000./orate, "time(ms)", 1, "I,Q");
    // printf("upload rotated with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

    // const volatile int olen = read_count * orate / irate + .5;
    // vector< complex<float> > resampled;
    // resampled.resize(olen);
    // soxr_oneshot(irate, orate, 2, (const float*)rotated.data(), read_count, NULL, (float*)resampled.data(), olen, NULL, NULL, NULL, NULL);

    // string output_filename = local_filename;
    // output_filename += ".resampled";
    // ofstream output(output_filename.c_str(), ios::binary);
    // output.write((const char*)rotated.data(), rotated.size() * sizeof(complex<float>));
    // output.close();

    // mongodat.upload_record("resampled", (float*)resampled.data(), 2, resampled.size()
    //     , NULL, "resampled", 1000./orate, "time(ms)", 1, "I,Q");
    // printf("upload resampled with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	mongodat.close();

    return 0;
}

// the resampling consumes too much time thus use MATLAB instead
/*

filename = '200115_model_500us_17mseq_9v.bin';
A = fread(fopen(strcat(filename, '.rotated')), [2 33804310], 'float32');
B = resample(A', 40000, 56875)';
fwrite(fopen(strcat(filename, '.resampled'), 'w'), B, 'float32');

*/
