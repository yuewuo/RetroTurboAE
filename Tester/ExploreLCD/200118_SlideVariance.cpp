// this is to find the preamble...... I forgot to reset MCU when collecting the data
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "m_sequence.h"
#include <complex>
#include <iterator>
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);
    
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

    const char* local_filename = "200115_model_500us_17mseq_5.5v.bin";

    ifstream file(local_filename, std::ios::binary);
    file.unsetf(std::ios::skipws);  // Stop eating new lines in binary mode!!!
    file.seekg(0, std::ios::end);
    streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    vector<char> vec; vec.resize(fileSize);
    file.read(vec.data(), fileSize);
    printf("vec.size() = %d\n", (int)vec.size());
    const int16_t* vec_dat = (const int16_t*)vec.data();
    int points = fileSize / sizeof(int16_t) / 4;

    vector<float> variances;
#define SLIDE_POINTS 4000 // 56.875 * 64ms ~ 4000
    for (int i=0; i+SLIDE_POINTS < points; i += SLIDE_POINTS) {
        float avr[4] = {0,0,0,0};
        int bias = 4*i;
        for (int j=0; j<SLIDE_POINTS; ++j) {
            for (int k=0; k<4; ++k) {
                avr[k] += vec_dat[bias+4*j+k];
            }
        }
        for (int k=0; k<4; ++k) {
            avr[k] /= SLIDE_POINTS;
        }
        float variance[4] = {0,0,0,0};
        for (int j=0; j<SLIDE_POINTS; ++j) {
            for (int k=0; k<4; ++k) {
                variance[k] += pow(vec_dat[bias+4*j+k] - avr[k], 2);
            }
        }
        variances.push_back(sqrt(variance[0] + variance[1] + variance[2] + variance[3]));
    }

    mongodat.upload_record("variances", (float*)variances.data(), 1, variances.size()
        , NULL, "variances", 1000./56875*SLIDE_POINTS, "time(ms)", 1, "variance");
    printf("upload variances with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

    return 0;
}
