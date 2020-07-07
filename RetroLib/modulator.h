/*
 * Modulator Program
 */

#include "tag-L4xx-ex.h"
// need a pre-defined type "Tag_Sample_t"

typedef uint8_t Tag_Single_t;
#define vts_t vector<Tag_Sample_t>

class FastDSM_Encoder {  // by default is 8kS/s, 8bit per PQAM symbol and will not change in RetroTurbo
public:
	int NLCD;  // the amount of LCD (8bit 8-4-2-1 dual-pixel)
	double frequency;  // default is 8000
	int ct_fast;  // default is 32, which is 4ms, for noise canceling
	int ct_slow;  // default is 128, which is 16ms, for clean initial state
	int combine;  // default is 1, how many LCD should be combined to send the same information
	int cycle;  // default is 32, 4ms, for 32 LCD (2PQAM * 16DSM), 4ms/16=0.25ms is limit of 9V driven LCD
					// this is the default setup, for combing more LCD, this keeps the same
	int duty;  // default is 4, 0.5ms, which is just enough for a single piece of LCD to go up and then down in 4ms
	int bit_per_symbol;  // default is 4: 16PQM
	int preamble_repeat; // default is 1, non-1 for auto preamble usage
	int channel_training_type;  // default is 0: use wpr's channel training tool, also can be 1: naive steps
	FastDSM_Encoder();
	vts_t o_preamble;  // default is 0:32,1:32,3:32,2:128,1:128,2:128,0:128, you can change it
	void build_preamble();
	vts_t o_channel_training;
	void build_channel_training();
	vts_t o_encoded_data;
	void encode(const vector<unsigned char>& data);
	static string dump(const Tag_Single_t& ts);
	vector<string> compressed_vector(const vts_t& tsv);
	static vector<Tag_Sample_t> uncompressed_vector(const vector<string>& compressed);
	string dump(const Tag_Sample_t& ts);
	bool tsequal(const Tag_Sample_t& a, const Tag_Sample_t& b);
	string dump(const vts_t& tsv);
	// with o_ prefix, they are the output of function compute_o_parameters
	int o_dsm_order;  // after combined
	int o_phase_delta;  // bias each LCD with this
	double o_symbol_rate;
	double o_throughput;
	// written by preamble func
	int o_preamble_length;
	// written by channel training func
	int o_channel_training_length;
	// written by encode func
	int o_cycle_cnt;
	int o_encoded_data_length;
	int o_bit_2_send;
	int o_data_start;
	int o_ct_start;
	void compute_o_parameters();
	static vector<Tag_Single_t> PQAMencode(const uint8_t* data, size_t size, int bps);  // bps (bits per symbol) should be 2, 4, 8, 16
	static vector<uint8_t> PQAMdecode(vector<Tag_Single_t> singles, size_t data_size, int bps);  // data_size is the size desired for output
};

FastDSM_Encoder::FastDSM_Encoder() {
	NLCD = 0;
	ct_fast = 32;
	ct_slow = 128;
	combine = 1;
	cycle = 32;
	duty = 4;
	frequency = 8000;
	bit_per_symbol = 4;
	preamble_repeat = 1;
	channel_training_type = 0;
}

void FastDSM_Encoder::build_channel_training() {
	compute_o_parameters();
	o_channel_training.clear();
	if (channel_training_type == 0) {
		o_channel_training.resize(cycle*(2 * o_dsm_order + 4)); 
		uint8_t (*pout)[16] = (uint8_t (*)[16])o_channel_training.data(); 
		fill(pout[0], pout[o_channel_training.size()], 0);
		unsigned ct = o_dsm_order == 16 ?  0b10111110001110110011010010100000 :
				o_dsm_order == 8 ?  0b1111011001010000 : o_dsm_order == 4 ?  0b11101000: 1;
		int ctt[4] = {0x00,0x0F,0xF0,0xFF};
		for (int t = 0; t < 2 * o_dsm_order; ++t) {
			for (int jj = 0; jj < o_dsm_order; jj++) {
				uint8_t s1 = ct >> ((t + 2 * jj) % (2 * o_dsm_order)) & 3, s2 = ctt[s1];
				for (int j = jj; j < 16;  j += NLCD/combine) for (int k = 0; k < duty; ++k)
						pout[k + t * cycle][TAG_L4XX_SAMPLE_BYTE - 1 - j] = s2;
			}
		}
	} else if (channel_training_type == 1) {
		printf("ct_fast: %d\n", ct_fast);
		printf("ct_slow: %d\n", ct_slow);
		Tag_Sample_t sample;
		for (int i=0; i<o_dsm_order; ++i) {  // every dsm do it individually
			for (int j=0; j<combine; ++j) sample.s[TAG_L4XX_SAMPLE_BYTE - 1 - (i + j * o_dsm_order)] = 0x0F;
			o_channel_training.insert(o_channel_training.end(), ct_fast, sample);
			for (int j=0; j<combine; ++j) sample.s[TAG_L4XX_SAMPLE_BYTE - 1 - (i + j * o_dsm_order)] = 0xFF;
			o_channel_training.insert(o_channel_training.end(), ct_fast, sample);
		}
		Tag_Sample_t zero;
		o_channel_training.insert(o_channel_training.end(), ct_slow, zero);
	}
	o_channel_training_length = o_channel_training.size();
}

void FastDSM_Encoder::compute_o_parameters() {
	assert(NLCD && "NLCD needed");
	assert(NLCD >= combine);
	o_dsm_order = NLCD / combine;  // divide cycle into o_dsm_order pieces to send
	assert(o_dsm_order <= TAG_L4XX_SAMPLE_BYTE && "LCD count is not enough for such high DSM order");
	o_phase_delta = cycle / o_dsm_order;
	o_symbol_rate = frequency * o_dsm_order / cycle;
	o_throughput = bit_per_symbol * o_symbol_rate;
}

void FastDSM_Encoder::encode(const vector<unsigned char>& data) {
	assert(!data.empty());
	compute_o_parameters();  // compute o_parameters
	vector<Tag_Single_t> singles = PQAMencode(data.data(), data.size(), bit_per_symbol);
	// first compute the amount of data to encode
	o_cycle_cnt = singles.size();
	o_encoded_data_length = (o_cycle_cnt / o_dsm_order) * cycle + (o_cycle_cnt % o_dsm_order) * o_phase_delta + (cycle - o_phase_delta);
	o_encoded_data.clear();
	o_encoded_data.resize(o_encoded_data_length);
	for (size_t i=0; i<singles.size(); ++i) {
		Tag_Single_t single = singles[i];
		int idx = i % o_dsm_order;
		int start = (i / o_dsm_order) * cycle + idx * o_phase_delta;
		Tag_Sample_t sample;
		for (int j=0; j<combine; ++j) sample.s[TAG_L4XX_SAMPLE_BYTE - 1 - (idx + j * o_dsm_order)] = single;
		for (int j=0; j<duty; ++j) o_encoded_data[start + j] |= sample;
	}
}

void FastDSM_Encoder::build_preamble() {
	assert(NLCD && "NLCD needed");
	Tag_Sample_t B0; memset(B0.les(NLCD), 0x00, NLCD);
	Tag_Sample_t B3; memset(B3.les(NLCD), 0xFF, NLCD);
	Tag_Sample_t B1; memset(B1.les(NLCD), 0x0F, NLCD);
	Tag_Sample_t B2; memset(B2.les(NLCD), 0xF0, NLCD);
	o_preamble.clear();
	int basic = 32 * frequency / 8000;
	for (int i=0; i<preamble_repeat; ++i) {
		o_preamble.insert(o_preamble.end(), basic, B0);
		o_preamble.insert(o_preamble.end(), basic, B1);
		o_preamble.insert(o_preamble.end(), basic, B3);
		o_preamble.insert(o_preamble.end(), basic * 3, B2);  // new LCD is much faster, so change this to 3, which is 12ms, enough
		o_preamble.insert(o_preamble.end(), basic * 3, B1);
		o_preamble.insert(o_preamble.end(), basic * 2, B3);
		o_preamble.insert(o_preamble.end(), basic * 3, B0);  // because preamble file is usually size of 32768 (ask prwang?), that is 32768/4/2/10/32=12.8 basic, must be larger than this, use 14
	}
	o_preamble_length = o_preamble.size();
}

bool FastDSM_Encoder::tsequal(const Tag_Sample_t& a, const Tag_Sample_t& b) {
	return 0 == memcmp(a.les(NLCD), b.les(NLCD), NLCD);
}

string FastDSM_Encoder::dump(const Tag_Single_t& ts) {
	string ret;
	char strbuf[3];
	sprintf(strbuf, "%02X", (uint8_t)ts);
	ret += strbuf;
	return ret;
}

string FastDSM_Encoder::dump(const Tag_Sample_t& ts) {
	string ret;
	for (int i=0; i<NLCD; ++i) {
		ret += dump(ts.les(NLCD)[i]);
	}
	return ret;
}

vector<string> FastDSM_Encoder::compressed_vector(const vts_t& tsv) {
	assert(NLCD && "NLCD needed");
	vector<string> ret = samples_to_compressed_string(tsv, NLCD);
	return ret;
}

string FastDSM_Encoder::dump(const vts_t& tsv) {
	assert(NLCD && "NLCD needed");
	string ret;
	vector<string> v = compressed_vector(tsv);
	for (auto i = v.begin(); i != v.end(); ++i) {
		printf("%s\n", i->c_str());
	}
	char strbuf[64];
	sprintf(strbuf, "tag sample list [NLCD=%d] has %d samples\n", (int)NLCD, (int)tsv.size());
	ret += strbuf;
	return ret;
}

static uint8_t mod_gray4b[] = {0x0,0x1,0x3,0x2,0x6,0x7,0x5,0x4,0xC,0xD,0xF,0xE,0xA,0xB,0x9,0x8};
					// 0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF
static uint8_t mod_map4b[] = {0x0,0x8,0x4,0xC,0x2,0xA,0x6,0xE,0x1,0x9,0x5,0xD,0x3,0xB,0x7,0xF};
static uint8_t mod_map3b[] = {0x0,0x4,0x2,0x6,0x1,0x5,0x3,0x7};  // do not use the smallest pixel
static uint8_t mod_map2b[] = {0x0,0xA,0x5,0xF};
static uint8_t mod_map1b[] = {0x0,0xF};

vector<uint8_t> FastDSM_Encoder::PQAMdecode(vector<Tag_Single_t> singles, size_t data_size, int bps) {
	assert(bps%2==0 && bps>=2 && bps<=8 && "invalid bps");
	vector<uint8_t> data; data.resize(data_size);
	int splitsize = (data_size * 8 + bps - 1) / bps;  // upper and padding zero
	assert(splitsize <= (int)singles.size() && "input length is not enough for decode");
	uint8_t diff_cnt = (1 << (bps/2));
	for (int i=0; i<splitsize; ++i) {
		// just the reverse process of PQAMencode
		Tag_Single_t single = singles[i];
		uint8_t mod_mapXb_ch1 = single & 0x0F;
		uint8_t mod_mapXb_ch2 = (single >> 4) & 0x0F;
		uint8_t ch1 = 0, ch2 = 0;
		switch (bps) {
		case 8:
			for (; ch1<16; ++ch1) { if (mod_map4b[ch1] == mod_mapXb_ch1) break; } assert(ch1!=16);
			for (; ch2<16; ++ch2) { if (mod_map4b[ch2] == mod_mapXb_ch2) break; } assert(ch2!=16);
			break;
		case 6:
			for (; ch1<8; ++ch1) { if (mod_map3b[ch1] == mod_mapXb_ch1) break; } assert(ch1!=8);
			for (; ch2<8; ++ch2) { if (mod_map3b[ch2] == mod_mapXb_ch2) break; } assert(ch2!=8);
			break;
		case 4:
			for (; ch1<4; ++ch1) { if (mod_map2b[ch1] == mod_mapXb_ch1) break; } assert(ch1!=4);
			for (; ch2<4; ++ch2) { if (mod_map2b[ch2] == mod_mapXb_ch2) break; } assert(ch2!=4);
			break;
		case 2:
			for (; ch1<2; ++ch1) { if (mod_map1b[ch1] == mod_mapXb_ch1) break; } assert(ch1!=2);
			for (; ch2<2; ++ch2) { if (mod_map1b[ch2] == mod_mapXb_ch2) break; } assert(ch2!=2);
			break;
		}
		uint8_t split_low_half = 0; for (; split_low_half<diff_cnt; ++split_low_half) { if (mod_gray4b[split_low_half] == ch1) break; } assert(split_low_half!=diff_cnt);
		uint8_t split_high_half = 0; for (; split_high_half<diff_cnt; ++split_high_half) { if (mod_gray4b[split_high_half] == ch2) break; } assert(split_high_half!=diff_cnt);
		uint8_t split = (split_high_half << (bps/2)) | split_low_half;
		for (int j=0; j<bps; ++j) {
			int bitidx = i*bps + j;
			uint8_t bit = (split >> j) & 0x01;
			data[bitidx/8] |= bit << (bitidx%8);
		}
	}
	return data;
}

vector<Tag_Single_t> FastDSM_Encoder::PQAMencode(const uint8_t* data, size_t size, int bps) {
	assert(bps%2==0 && bps>=2 && bps<=8 && "invalid bps");
	vector<Tag_Single_t> singles;
	int splitsize = (size * 8 + bps - 1) / bps;  // upper and padding zero
	uint8_t halfmask = (1 << (bps/2)) - 1;
	// printf("mask: 0x%02X, halfmask: 0x%02X, splitsize: %d\n", mask, halfmask, splitsize);
	for (int i=0; i<splitsize; ++i) {
		uint8_t split = 0;
		for (int j=0; j<bps; ++j) {
			int bitidx = i*bps + j;
			// if (bitidx/8 >= size) printf("padding 0\n");
			uint8_t bit = bitidx/8 < (int)size ? ((data[bitidx/8] >> (bitidx%8)) & 1) : 0;
			split |= bit << j;
		}
		uint8_t ch1 = mod_gray4b[split & halfmask];
		uint8_t ch2 = mod_gray4b[(split >> (bps/2)) & halfmask];
		Tag_Single_t single = 0;
		switch (bps) {
			case 8: single = mod_map4b[ch1] | (mod_map4b[ch2] << 4); break;
			case 6: single = mod_map3b[ch1] | (mod_map3b[ch2] << 4); break;
			case 4: single = mod_map2b[ch1] | (mod_map2b[ch2] << 4); break;
			case 2: single = mod_map1b[ch1] | (mod_map1b[ch2] << 4); break;
		}
		// printf("ch1: 0x%01X, ch2: 0x%01X, single: 0x%02X\n", ch1, ch2, single);
		singles.push_back(single);
	}
	return singles;
}

vector<Tag_Sample_t> FastDSM_Encoder::uncompressed_vector(const vector<string>& compressed) {
	vector<Tag_Sample_t> samples;
	for (size_t i=0; i<compressed.size(); ++i) {
		string line = compressed[i];
		int cnt = 1;
		size_t idx = line.find(':');
		if (idx != string::npos) {
			cnt = atoi(line.c_str() + idx + 1);
            line = line.substr(0, idx);
		}
		samples.insert(samples.end(), cnt, tag_invert_sample_from_str(line));  // the sample is inverted so that shorter string could also work well
	}  // this part tested
    return samples;
}
