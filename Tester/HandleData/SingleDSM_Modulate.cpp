#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "modulator.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

struct SingleDSM_Encoder: public FastDSM_Encoder {
	void encode(const vector<unsigned char>& data) {
		assert(!data.empty());
		compute_o_parameters();  // compute o_parameters
		// first compute the amount of data to encode
		o_cycle_cnt = data.size() * 8;
		o_encoded_data_length = (o_cycle_cnt / o_dsm_order) * cycle + (o_cycle_cnt % o_dsm_order) * o_phase_delta + (cycle - o_phase_delta);
		o_encoded_data.clear();
		o_encoded_data.resize(o_encoded_data_length);
		for (int i=0; i<o_cycle_cnt; ++i) {
			int idx = i % o_dsm_order;
			Tag_Sample_t sample;
			if ((data[i/8] >> (i%8)) & 0x01) {
				for (int j=0; j<combine; ++j) {
					int bitidx = idx + j * o_dsm_order;
					sample.s[bitidx/8] |= 1 << (bitidx%8);
				}
			}
			int start = (i / o_dsm_order) * cycle + idx * o_phase_delta;
			for (int j=0; j<duty; ++j) o_encoded_data[start + j] |= sample;
		}
	}
	void build_channel_training() {
		compute_o_parameters();
		o_channel_training.clear();
		Tag_Sample_t sample;
		for (int i=0; i<o_dsm_order; ++i) {
			for (int j=0; j<combine; ++j) {
				int bitidx = i + j * o_dsm_order;
				sample.s[bitidx/8] |= 1 << (bitidx%8);
			}
			o_channel_training.insert(o_channel_training.end(), ct_fast, sample);
		}
		Tag_Sample_t zero;
		o_channel_training.insert(o_channel_training.end(), ct_slow, zero);
		o_channel_training_length = o_channel_training.size();
	}
	vector<string> compressed_vector(const vts_t& tsv) {
		assert(NLCD && "NLCD needed");
		vector<string> ret;
		if (!tsv.empty()) {
			Tag_Sample_t last = tsv[0];
			int equcnt = 0;
			for (size_t i=0; i<tsv.size(); ++i) {
				if (tsequal(last, tsv[i])) {
					equcnt += 1;
				} else {
					string line;
					for (int j=0; j<(NLCD+7)/8; ++j) { 
						char strbuf[3]; sprintf(strbuf, "%02X", (uint8_t)last.s[j]); line += strbuf; 
					}
					{ char strbuf[16]; sprintf(strbuf, ":%d", equcnt); line += strbuf;}
					ret.push_back(line);
					last = tsv[i];
					equcnt = 1;
				}
			}
			string line;
			for (int j=0; j<(NLCD+7)/8; ++j) {
				char strbuf[3]; sprintf(strbuf, "%02X", (uint8_t)last.s[j]); line += strbuf;
			}
			{ char strbuf[16]; sprintf(strbuf, ":%d", equcnt); line += strbuf;}
			ret.push_back(line);
		}
		return ret;
	}
};

SingleDSM_Encoder encoder;

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
	printf(#name": %d\n", (int)(encoder.name = record[#name].value<int32_t>()));
	printf("reading parameters from database record [%s:%s]\n", collection_str, record_id_str);
	NEED_RECORD_INT32(NLCD)
	NEED_RECORD_INT32(ct_fast)
	NEED_RECORD_INT32(ct_slow)
	NEED_RECORD_INT32(combine)
	NEED_RECORD_INT32(cycle)
	NEED_RECORD_INT32(duty)
	encoder.bit_per_symbol = 1;  // for computing throughput
	assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE); \
	printf("frequency: %f\n", (float)(encoder.frequency = record["frequency"].value<double>()));
	// get data in hex format string
	assert(record["data"].existed() && record["data"].type() == BSON_TYPE_UTF8 && "data needed in hex format");
	vector<uint8_t> data = record["data"].get_bytes_from_hex_string();
	assert(!data.empty() && "data cannot be empty");
	printf("data.size(): %d\n", (int)data.size());

	// generate preamble and save in record
	Tag_Sample_t B0; memset(B0.s, 0x00, encoder.NLCD);
	Tag_Sample_t B3; memset(B3.s, 0xFF, encoder.NLCD);
	encoder.o_preamble.clear();
	encoder.o_preamble.insert(encoder.o_preamble.end(), 32, B0);
	encoder.o_preamble.insert(encoder.o_preamble.end(), 128, B3);
	encoder.o_preamble.insert(encoder.o_preamble.end(), 128, B0);
	encoder.o_preamble_length = encoder.o_preamble.size();
	printf("preamble:\n"); encoder.dump(encoder.o_preamble);
	record["o_preamble_length"] = encoder.o_preamble_length;
	record["o_preamble"].remove();
	record["o_preamble"].build_array();
	vector<string> strs_preamble = encoder.compressed_vector(encoder.o_preamble);
	record["o_preamble"].append(strs_preamble);

	// channel training
	encoder.build_channel_training();
	record["o_channel_training_length"] = encoder.o_channel_training_length;
	record["o_channel_training"].remove();
	record["o_channel_training"].build_array();
	vector<string> strs_channel_training = encoder.compressed_vector(encoder.o_channel_training);
	record["o_channel_training"].append(strs_channel_training);

	// encode
	encoder.encode(data);
	// then record everything computed
#define RECORD_INT32(name) printf(#name": %d\n", encoder.name); record[#name] = encoder.name;
#define RECORD_DOUBLE(name) printf(#name": %f\n", encoder.name); record[#name] = encoder.name;
	RECORD_INT32(o_dsm_order)
	RECORD_INT32(o_phase_delta)
	RECORD_DOUBLE(o_symbol_rate)
	RECORD_DOUBLE(o_throughput)
	RECORD_INT32(o_cycle_cnt)
	RECORD_INT32(o_encoded_data_length)
	record["o_encoded_data"].remove();
	record["o_encoded_data"].build_array();
	vector<string> str_encoded_data = encoder.compressed_vector(encoder.o_encoded_data);
	record["o_encoded_data"].append(str_encoded_data);

	// combine those three parts
	vector<string> o_out;
	o_out.insert(o_out.end(), strs_preamble.begin(), strs_preamble.end());
	o_out.insert(o_out.end(), strs_channel_training.begin(), strs_channel_training.end());
	o_out.insert(o_out.end(), str_encoded_data.begin(), str_encoded_data.end());
	record["o_out"].remove();
	record["o_out"].build_array();
	record["o_out"].append(o_out);

	// save file to database
	printf("saved id: %s\n", MongoDat::OID2str(*record.save()).c_str());

	mongodat.close();
	record.remove();

	return 0;
}
