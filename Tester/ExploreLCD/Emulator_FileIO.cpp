#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include <stdio.h>
#include <complex>
#include "modulator.h"
#include <vector>
#include <fstream>
using namespace std;

vector<uint8_t> get_bytes_from_hex_string(string hex);

int main(int argc, char** argv) {
    // delete this is not used
	MongoDat::LibInit();
    MongoDat mongodat;
	mongodat.open();

    if (argc != 6) {
        printf("usage: <frequency:4000,8000> <duty:2,4,6,8,12,16> <combine:1,2,4,8,16> <bit_per_symbol:2,4,6,8> <data:hex>\n");
        printf("default settings are: cycle=32, sample_rate=80k, NLCD=16\n");
        printf("no channel training\n");
        return -1;
    }

    double orate = 80e3;
    int frequency = atof(argv[1]);
    assert(frequency == 4000 || frequency == 8000);
    int duty = atoi(argv[2]);
    int combine = atoi(argv[3]);
    int bit_per_symbol = atoi(argv[4]);
    string hex_data = argv[5];
    char ref_file_name[32];
    sprintf(ref_file_name, "refs16x2_%d_%d.bin", frequency, duty);
    printf("reading ref file: %s\n", ref_file_name);
    ifstream input(ref_file_name, ios::binary);
    if (!input) {
        printf("[error] cannot open file\n");
        return -2;
    }
	input.seekg (0, input.end);
	int filesize = input.tellg(), writtenlen = 0;
	input.seekg (0, input.beg);
	vector<char> refsbin; refsbin.resize(filesize);
	while (writtenlen < filesize) {
		int writesize = filesize - writtenlen;
		if (!input.read((char*)refsbin.data(), writesize)) return -3;  // read failed
		writtenlen += writesize;
	}
	input.close();
    printf("read ref file: %d bytes\n", (int)refsbin.size());

    FastDSM_Encoder encoder;
    encoder.NLCD = 16;
    encoder.combine = combine;
    int cycle = encoder.cycle = 32;
    encoder.duty = duty;
    encoder.bit_per_symbol = bit_per_symbol;
    encoder.frequency = frequency;
    vector<uint8_t> data = get_bytes_from_hex_string(hex_data);
    encoder.encode(data);
    printf("o_encoded_data: %d\n", (int)encoder.o_encoded_data.size());
    
    const int refs_multi = sizeof(float) * 2 * 16 * (cycle * orate / frequency) * 2;
    assert(refsbin.size() % refs_multi == 0 && "invalid file size");
    int refs_cnt = refsbin.size() / refs_multi;
    int effect_length = -1;
    for (int i=0; i<16; ++i) {
        if (refs_cnt == 1<<i) {
            effect_length = i;
            break;
        }
    }
    assert(effect_length != -1 && "invalid file size");
    printf("effect_length: %d\n", effect_length);
    const complex<float>* refs = (const complex<float>*)refsbin.data();
    int refs_single = cycle * orate / frequency;
#define ref(LCD, is_back, status, i) refs[((LCD * 2 + is_back) + refs_cnt) * refs_single + i]

    vector< complex<float> > emulated;
    emulated.resize(encoder.o_encoded_data.size() * orate / frequency);
    vector<string> str_encoded_data = encoder.compressed_vector(encoder.o_encoded_data);
    for (int i=0; i<(int)str_encoded_data.size(); ++i) {
        printf("%s\n", str_encoded_data[i].c_str());
    }

    const int encoded_size = encoder.o_encoded_data.size();
    int mask = (1 << effect_length) - 1;
    for (int i=0; i<16; ++i) {
        // first try to find the first fire
        int j=0;
        while (j<encoded_size) {
            for (; j<encoded_size; ++j) {
                if (encoder.o_encoded_data[j].s[i]) break;
            }
            if (j >= encoded_size) continue;  // this LCD never sends signals
            int ref_idx[8];  // eight pixels should be recorded seperately
            for (int k=0; k<8; ++k) ref_idx[k] = 0;
            for (; j+cycle<encoded_size; j+=cycle) {
                uint8_t now = encoder.o_encoded_data[j].s[i];
                // check DSM timing
                if (now) {
                    for (int k=0; k<duty; ++k) assert(encoder.o_encoded_data[j+k].s[i] == now);
                    for (int k=duty; k<cycle; ++k) assert(encoder.o_encoded_data[j+k].s[i] == 0);
                } else {
                    for (int k=0; k<cycle; ++k) assert(encoder.o_encoded_data[j+k].s[i] == 0);
                }
                // emulate
                int ostart = j*orate/frequency;
                int olen = cycle*orate/frequency;
                for (int k=0; k<8; ++k) {
                    bool bit = (now >> k) & 1;
                    ref_idx[k] = mask & ((ref_idx[k] << 1) | bit);
                    for (int x=0; x<olen; ++x) {
                        // TODO here is constrains about ref
                        emulated[ostart+x] += ref(i, k<4?0:1, ref_idx[k], x) * (complex<float>)((k<4?(1<<k):(1<<(k-4))) / 4. / 15);
                    }
                }

                // found 3 continuous 0, reset to 0 state to support wpr's channel training
                bool all_zero = true;
                for (int k=0; k<8; ++k) if (ref_idx[k]) all_zero = false;
                if (all_zero) break;
            }
            ++j;
        }
    }

	mongodat.upload_record("emulated", (float*)emulated.data(), 2, emulated.size(), NULL, "emulated", 1/80., "time(ms)", 1, "I,Q");
	printf("upload emulated with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

    return 0;
}

vector<uint8_t> get_bytes_from_hex_string(string hex) {
	vector<uint8_t> bs;
	assert(hex.size() % 2 == 0 && "invalid hex string");
	for (size_t i=0; i<hex.size(); i += 2) {
		char H = hex[i], L = hex[i+1];
#define ASSERT_HEX_VALID_CHAR_TMP(x) assert(((x>='0'&&x<='9')||(x>='a'&&x<='z')||(x>='A'&&x<='Z')) && "invalid char");
		ASSERT_HEX_VALID_CHAR_TMP(H) ASSERT_HEX_VALID_CHAR_TMP(L)
#undef ASSERT_HEX_VALID_CHAR_TMP
#define c2bs(x) (x>='0'&&x<='9'?(x-'0'):((x>='a'&&x<='f')?(x-'a'+10):((x>='A'&&x<='F')?(x-'A'+10):(0))))
		char b = (c2bs(H) << 4) | c2bs(L);
#undef c2bs
		bs.push_back(b);
	}
	return bs;
}
