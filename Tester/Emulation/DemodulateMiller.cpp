/*
 * This program is to fetch data and try to demodulate it
 * 
 * The basic obervation is that, using "Tester/Emulation/ScatterPlot.cpp" we can see very nice scatter plot, 
 *		 simply select the nearest point would give out a naive demodulation
 *		 select multiple nearest ones would give possibly better result, with demodulation time trade-off
 * they are in the same manner so that this program requires a butter length of how many points to take, we define two parameters here:
 *		 1. demod_buffer_length: the maximum number of possible decode object
 *		 2. demod_nearest_count: this demodulation works as: do the same work in scatterplot, then find several nearst in the constellation diagram and compute their values
 *							note that this method is not accurate when evaluate the constellation diagram, because each sub-pixel's history is recordde individually. I assume that this wouldn't effect a lot, nonetheless later computation would fix this slight difference
 */
// use static here, simply test it and used later with demodulator
#define STATIC_EMULATOR_FOR_EASIER_COPY
#include "emulator.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include <regex>
#include <random>
#include <set>
#include <map>
#include <rpc_client.hpp>
#include "millerdecode.h"
#include "ref.txt"
#include "soxr/soxr.h"
#include <fstream>
using namespace std;

miller_decode decoder;
extern "C" void miller_decode_init(const cf *sample_buffer, const volatile int *sample_size)
{
	decoder.sample_buffer = (const cf *)sample_buffer;
	decoder.sample_size = sample_size;
}
extern "C" int miller_decode_one(int n_bits, unsigned char *byte_out, double *SNR, int *Offset, int *Mf, double *G_scale_re, double *G_scale_im, double *D, double *G_dc_re, double *G_dc_im, double *Dp)
{
	if (n_bits > decoder.max_bits - decoder.delay_bits - 7)
	{
		reader_ctrl_fprint(CTRL_FLOW_PHY_MSG, "packet too long\n");
		return RES_NO;
	}
	decoder.dump_samples(n_bits);
	if (-1 == decoder.get_prefix_samp(SNR, Offset, Mf, G_scale_re, G_scale_im, D, G_dc_re, G_dc_im))
		return RES_NO;
	float dp1 = decoder.dp(n_bits);
	int n_bytes = (n_bits + 7) / 8;
	for (int i = 0, p = 0; i < n_bytes; ++i)
	{
		byte_out[i] = 0;
		for (int j = 0; j < 8; ++j, ++p)
		{
			byte_out[i] |= decoder.bit_out[p] << j;
		}
	}
	*Dp = (double)dp1;
	reader_ctrl_fprintf(CTRL_FLOW_PHY_MSG, ", DP=%.8f, Pac = ", dp1);
	for (int i = 0; i < n_bytes; ++i)
		reader_ctrl_fprintf(CTRL_FLOW_PHY_MSG, " %02X", byte_out[i]);
	return RES_NORMAL;
}
extern "C" int decode_config(const char *cfg) { return decoder.config(cfg); }
extern "C" int decode_set_rate(int rate)
{
	if (!(0 <= rate && rate <= 3))
	{
		reader_ctrl_fprint(CTRL_FLOW_PHY_MSG, "rate can only be 0..3");
		return RES_ERROR;
	}
	decoder.set_rate(rate);
	return RES_NORMAL;
}

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char **argv)
{
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 3 && argc != 4)
	{
		printf("usage: <collection> <id(12 byte hex = 24 char)> <port>\n");
		printf("\n");
		printf("the database document should have the following elements:\n");
		printf("	common: frequency, rate\n");
		printf("	data: format as hex string\n");
		printf("	channel: retroturbo records 2 channels by default, but we only need one here\n");
		printf("	demod_data_id: the result of emulation or real experiment, to be analyzed\n");
		return -1;
	}

	const char *collection_str = argv[1];
	const char *record_id_str = argv[2];
	int server_port = argc == 4 ? atoi(argv[3]) : 0;
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);

	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);

	assert(record["rate"].existed() && record["rate"].type() == BSON_TYPE_INT32);
	int rate = record["rate"].value<int32_t>();
	printf("rate: %d\n", rate);
	assert(rate >= 0 && rate <= 3 && "invalid rate");

	assert(record["data"].existed() && record["data"].type() == BSON_TYPE_UTF8 && "data needed in hex format");
	vector<uint8_t> data = record["data"].get_bytes_from_hex_string();

	assert(record["channel"].existed() && record["channel"].type() == BSON_TYPE_INT32);
	int channel = record["channel"].value<int32_t>();
	printf("channel: %d\n", channel);
	assert((channel == 1 || channel == 2) && "invalid channel: 1 or 2");

	string BER_output_key = "BER";
	if (record["BER_output_key"].existed() && record["BER_output_key"].type() == BSON_TYPE_UTF8) {
		BER_output_key = record["BER_output_key"].value<string>();
	} printf("BER_output_key: %s\n", BER_output_key.c_str());

	string SNR_output_key = "SNR";
	if (record["SNR_output_key"].existed() && record["SNR_output_key"].type() == BSON_TYPE_UTF8) {
		SNR_output_key = record["SNR_output_key"].value<string>();
	} printf("SNR_output_key: %s\n", SNR_output_key.c_str());

	string offset_output_key = "offset";
	if (record["offset_output_key"].existed() && record["offset_output_key"].type() == BSON_TYPE_INT32) {
		offset_output_key = record["offset_output_key"].value<string>();
	} printf("offset_output_key: %s\n", offset_output_key.c_str());

	string mf_output_key = "mf";
	if (record["mf_output_key"].existed() && record["mf_output_key"].type() == BSON_TYPE_INT32) {
		mf_output_key = record["mf_output_key"].value<string>();
	} printf("mf_output_key: %s\n", mf_output_key.c_str());

	string g_scale_re_output_key = "g_scale_re";
	if (record["g_scale_re_output_key"].existed() && record["g_scale_re_output_key"].type() == BSON_TYPE_UTF8) {
		g_scale_re_output_key = record["g_scale_re_output_key"].value<string>();
	} printf("g_scale_re_output_key: %s\n", g_scale_re_output_key.c_str());

	string g_scale_im_output_key = "g_scale_im";
	if (record["g_scale_im_output_key"].existed() && record["g_scale_im_output_key"].type() == BSON_TYPE_UTF8) {
		g_scale_im_output_key = record["g_scale_im_output_key"].value<string>();
	} printf("g_scale_im_output_key: %s\n", g_scale_im_output_key.c_str());

	string d_output_key = "d";
	if (record["d_output_key"].existed() && record["d_output_key"].type() == BSON_TYPE_UTF8) {
		d_output_key = record["d_output_key"].value<string>();
	} printf("d_output_key: %s\n", d_output_key.c_str());

	string g_dc_re_output_key = "g_dc_re";
	if (record["g_dc_re_output_key"].existed() && record["g_dc_re_output_key"].type() == BSON_TYPE_UTF8) {
		g_dc_re_output_key = record["g_dc_re_output_key"].value<string>();
	} printf("g_dc_re_output_key: %s\n", g_dc_re_output_key.c_str());

	string g_dc_im_output_key = "g_dc_im";
	if (record["g_dc_im_output_key"].existed() && record["g_dc_im_output_key"].type() == BSON_TYPE_UTF8) {
		g_dc_im_output_key = record["g_dc_im_output_key"].value<string>();
	} printf("g_dc_im_output_key: %s\n", g_dc_im_output_key.c_str());

	string dp_output_key = "dp";
	if (record["dp_output_key"].existed() && record["dp_output_key"].type() == BSON_TYPE_UTF8) {
		dp_output_key = record["dp_output_key"].value<string>();
	} printf("dp_output_key: %s\n", dp_output_key.c_str());

	assert(record["demod_data_id"].existed() && record["demod_data_id"].type() == BSON_TYPE_UTF8);
	string demod_data_id_str = record["demod_data_id"].value<string>();
	assert(MongoDat::isPossibleOID(demod_data_id_str.c_str()) && "demod_data_id invalid");
	bson_oid_t demod_data_id = MongoDat::parseOID(demod_data_id_str.c_str());
	vector<char> demod_data_binary = mongodat.get_binary_file(demod_data_id);
	int demod_data_length = demod_data_binary.size() / sizeof(int16_t);
	// printf("DEBUG: demod_data_length = %d\n", demod_data_length);
	// const complex<float>* demod_data_has_bias = (const complex<float>*)demod_data_binary.data();
	const int16_t *sd1b = (const int16_t *)demod_data_binary.data();
	vector<cf> demod_data_channel_select;
	for (int i = 0; i < demod_data_length; i += 2)
	{
		if ((channel == 1 && i % 4 == 0) || (channel == 2 && i % 4 == 2))
		{
			cf tmp;
			tmp.re = (float)sd1b[i];
			tmp.im = (float)sd1b[i + 1];
			demod_data_channel_select.push_back(tmp);
		}
	}

	double irate = 56875;
	double orate = 20000;
	const volatile int olen = (const volatile int)(demod_data_length / 4 * orate / irate + .5);
	vector<cf> demod_input;
	demod_input.resize(olen);
	// printf("DEBUG: olen = %d\n", olen);
	
	soxr_oneshot(irate, orate, 2, (const float *)demod_data_channel_select.data(), demod_data_length / 4, NULL, (float *)demod_input.data(), olen, NULL, NULL, NULL, NULL);

//	(ofstream("dump0.bin",ios::binary).write((const char*)demod_input.data(), demod_input.size() * sizeof(cf));
	// printf("DEBUG: ready to dump files\n");
	// FILE*samples = fopen("dump_125.txt", "w"); fprintf(samples, "[");
	// for(int i = 0; i < olen; i++) {
	//   fprintf(samples,"%f,%f;", demod_input[i].re, demod_input[i].im);
	// } fprintf(samples, "]\n"); 
	// fclose(samples);

	// FILE*samples = fopen("dump_ref.txt", "w"); fprintf(samples, "{");
	// for(int i = 2046; i < 2046 + 1120; i++) {
	//   fprintf(samples,"%f,", demod_input[i].im);
	// } fprintf(samples, "}\n"); 
	// fclose(samples);

	miller_decode_init(demod_input.data(), &olen);
	decode_set_rate(rate);
	vector<uint8_t> decoded;
	decoded.reserve(data.size());
	// printf("DEBUG: data.size() = %d\n", data.size());
	double SNR;
	int offset, mf;
	double g_scale_re, g_scale_im, d, g_dc_re, g_dc_im, dp;
	miller_decode_one(data.size() * 8, decoded.data(), &SNR, &offset, &mf, &g_scale_re, &g_scale_im, &d, &g_dc_re, &g_dc_im, &dp);

	string decoded_string;
	for(int i = 0; i < data.size(); i++)
	{
		char strbuf[4];
		sprintf(strbuf, "%02X", decoded[i]);
		decoded_string += strbuf;
	}
	printf("origin: %s\n", MongoDat::dump(data).c_str());
	record["decoded"] = decoded_string;
	printf("decode: %s\n", decoded_string.c_str());

	double BER = 0;
	for (int i=0; i<(int)data.size(); ++i) {
		for (int j=0; j<8; ++j) {
			if ((decoded[i] ^ data[i]) & (1 << j)) ++BER;
		}
	}
	BER /= data.size() * 8;
	printf("BER: %f %%\n", BER * 100);

	
	record[BER_output_key.c_str()] = BER;
	record[SNR_output_key.c_str()] = SNR;
	record[offset_output_key.c_str()] = offset;
	record[mf_output_key.c_str()] = mf;
	record[g_scale_re_output_key.c_str()] = g_scale_re;
	record[g_scale_im_output_key.c_str()] = g_scale_im;
	record[d_output_key.c_str()] = d;
	record[g_dc_re_output_key.c_str()] = g_dc_re;
	record[g_dc_im_output_key.c_str()] = g_dc_im;
	record[dp_output_key.c_str()] = dp;
	record.save();
	mongodat.close();

	return 0;
}
