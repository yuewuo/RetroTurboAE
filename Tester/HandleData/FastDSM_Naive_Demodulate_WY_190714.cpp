#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "modulator.h"
#define MEMORY_READER_H7XX
#include "reader-H7xx.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

class Naive_Demodulator_WY_190714 {
public:
	int NLCD;  // the amount of LCD (8bit 8-4-2-1 dual-pixel)
	double frequency;
	int padding_preamble;  // the sample count before preamble start
	int o_preamble_length;
	int ct_fast;
	int ct_slow;
	int combine;
	int cycle;
	int duty;
	int bit_per_symbol;
	double sample_rate;
	vector<unsigned char> decode(const complex<float>* buffer, int length, int bytes);
};

Naive_Demodulator_WY_190714 decoder;

vector<complex<float>> debug_ct_ref;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 3) {
		printf("usage: <collection> <id(12 byte hex = 24 char)>");
		return -1;
	}

	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];

	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
#define NEED_RECORD_INT32(name) \
	assert(record[#name].existed() && record[#name].type() == BSON_TYPE_INT32); \
	printf(#name": %d\n", (int)(decoder.name = record[#name].value<int32_t>()));
	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);
	NEED_RECORD_INT32(NLCD)
	NEED_RECORD_INT32(ct_fast)
	NEED_RECORD_INT32(ct_slow)
	NEED_RECORD_INT32(combine)
	NEED_RECORD_INT32(cycle)
	NEED_RECORD_INT32(duty)
	NEED_RECORD_INT32(bit_per_symbol)
	NEED_RECORD_INT32(o_preamble_length)
	if (record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE) {
		decoder.frequency = record["frequency"].value<double>();
	} else decoder.frequency = 4000;
	decoder.sample_rate = 80000;
	decoder.padding_preamble = 64;
	// get data in hex format string
	assert(record["data"].existed() && record["data"].type() == BSON_TYPE_UTF8 && "data needed in hex format");
	vector<uint8_t> data = record["data"].get_bytes_from_hex_string();
	assert(!data.empty() && "data cannot be empty");
	printf("data.size(): %d\n", (int)data.size());

	// decode
	assert(record["data_id"].existed() && record["data_id"].type() == BSON_TYPE_UTF8 && "data_id needed");
	string binary_id_str = record["data_id"].value<string>();
	assert(MongoDat::isPossibleOID(binary_id_str.c_str()) && "data_id invalid");
	bson_oid_t record_id = MongoDat::parseOID(binary_id_str.c_str());
	vector<char> binary = mongodat.get_binary_file(record_id);
	vector<uint8_t> decoded = decoder.decode((const complex<float>*)binary.data(), binary.size()/sizeof(complex<float>), data.size());
	printf("origin: %s\n", MongoDat::dump(data).c_str());
	printf("decode: %s\n", MongoDat::dump(decoded).c_str());
	
	mongodat.upload_record("debug_ct_ref", (float*)debug_ct_ref.data(), 2, debug_ct_ref.size(), NULL, "debug_ct_ref", 1/80., "time(ms)", 1, "I,Q");
	printf("upload debug_ct_ref with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// double BER;
	// for (size_t i=0; i<data.size() * 8; ++i) {
	// 	if ((decoded[i/8] ^ data[i/8]) & (1 << (i%8))) ++BER;
	// }
	// BER /= data.size() * 8;
	// printf("BER: %f %%", BER * 100);

	// save union curve (optional for debug)
	// mongodat.upload_record("union curve", (float*)decoder.union_curve.data(), 1, decoder.union_curve.size(), NULL, "simple union of four", 1/56.875, "time(ms)", 1, "data");
	// printf("upload union curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
	// mongodat.upload_record("reference all", (float*)decoder.o_ref.data(), 1, decoder.o_ref.size(), NULL, "reference", 1/56.875, "time(ms)", 1, "data");
	// printf("upload reference curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// then record everything computed
// #define RECORD_INT32(name) printf(#name": %d\n", decoder.name); record[#name] = decoder.name;
// #define RECORD_DOUBLE(name) printf(#name": %f\n", decoder.name); record[#name] = decoder.name;
// 	RECORD_INT32(o_middle)
// 	RECORD_INT32(o_ct_start)
// 	RECORD_INT32(o_data_start)
// 	record["BER"] = BER;
// 	record["decoded"] = MongoDat::dump(decoded);

	// save file to database
	record.save();

	mongodat.close();
	record.remove();

	return 0;
}

float I(const complex<float>& x) { return x.real(); }
float Q(const complex<float>& x) { return x.imag(); }
void I(complex<float>& x, float y) { x.real(y); }
void Q(complex<float>& x, float y) { x.imag(y); }

vector<unsigned char> Naive_Demodulator_WY_190714::decode(const complex<float>* buffer, int length, int bytes) {
#define MS(x) ((x) / sample_rate * 1000)
#define FMT(x) (int)(x), (float)MS(x)
#define T "(%d, %f ms)"
#define TI(x) ((x) * sample_rate / frequency)
	vector<unsigned char> data;
	data.resize(bytes);
	printf("have %d samples, sample[0](I,Q) = (%f, %f)\n", length, I(buffer[0]), Q(buffer[0]));
	printf("preamble start at " T "\n", FMT(padding_preamble));
	int ct_start_cnt = padding_preamble + TI(o_preamble_length - 16);
	printf("channel training start at " T "\n", FMT(ct_start_cnt));

	// first record curve of each LCD by channel training
	int channel_training_cnt_each = NLCD / combine;
	int duty_cnt = TI(duty);
	int cycle_cnt = TI(cycle);
	printf("recording curve using channel training, fast: " T ", slow: " T "\n", FMT(duty_cnt), FMT(cycle_cnt-duty_cnt));
	struct IQref {
		complex<float> amplitude[2];
		vector<complex<float>> ref[2];
	};
	vector<IQref> ct_ref;
	ct_ref.resize(channel_training_cnt_each);
	for (int i=0; i<channel_training_cnt_each; ++i) {
		ct_ref[i].ref[0].resize(cycle_cnt);
		ct_ref[i].ref[1].resize(cycle_cnt);
	}
	complex<float> ct_bias;
	int ct_fast_cnt = TI(ct_fast);
	printf("record from " T " to " T "\n", FMT(ct_start_cnt-ct_fast_cnt), FMT(ct_start_cnt));
	for (int i=ct_start_cnt-ct_fast_cnt; i<ct_start_cnt; ++i) {
		ct_bias += buffer[i];
	}
	ct_bias /= ct_fast_cnt;
	printf("ct_bias is (%f, %f)\n", I(ct_bias), Q(ct_bias));
	int ct_slow_start_cnt = ct_start_cnt + channel_training_cnt_each * 2 * ct_fast_cnt;
	printf("ct_slow_start_cnt: %d\n", ct_slow_start_cnt);
	complex<float> slow_var(1, 1);  // I=1, Q=1
	for (int i=0; i<channel_training_cnt_each; ++i) {
		for (int j=0; j<2; ++j) {
			complex<float> this_ct_bias;
			int idx = ct_start_cnt + (j + 2*i) * ct_fast_cnt;
			for (int k=duty_cnt; k<ct_fast_cnt; ++k) {  // using flat region to compute amplitude
				this_ct_bias += buffer[idx + k];
			}
			this_ct_bias /= (ct_fast_cnt - duty_cnt);
			ct_ref[i].amplitude[j] = this_ct_bias - ct_bias;
			// printf("ct %d,%d: " T ", amp = (%f, %f)\n", i, j, FMT(idx), I(ct_ref[i].amplitude[j]), Q(ct_ref[i].amplitude[j]));
			// record the fast edge
			for (int k=0; k<duty_cnt; ++k) {
				ct_ref[i].ref[j][k] = buffer[idx+k] - ct_bias;
			}
			ct_bias = this_ct_bias;
			// then add slow edge reference by computing their amplitude
			for (int k=0; k<cycle_cnt-duty_cnt; ++k) {
				ct_ref[i].ref[j][duty_cnt + k] = buffer[ct_slow_start_cnt + k] / slow_var * ct_ref[i].amplitude[j];
			}
			debug_ct_ref.insert(debug_ct_ref.end(), ct_ref[i].ref[j].begin(), ct_ref[i].ref[j].end());
		}
	}

	// then try to reconstruct waveform using the reference
	int data_start_cnt = ct_slow_start_cnt + TI(ct_slow);
	printf("data start at " T "\n", FMT(data_start_cnt));

	printf("\n");
	return data;
#undef MS
}
