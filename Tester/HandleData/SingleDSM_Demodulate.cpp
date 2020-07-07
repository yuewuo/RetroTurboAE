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

struct SingleDSM_Decoder {
	double frequency;
	int NLCD;  // the amount of LCD (8bit 8-4-2-1 dual-pixel)
	int ct_fast;  // default is 32, which is 4ms, for noise canceling
	int ct_slow;  // default is 128, which is 16ms, for clean initial state
	int combine;  // default is 1, how many LCD should be combined to send the same information
	int cycle;  // default is 32, 4ms, for 32 LCD (2PQAM * 16DSM), 4ms/16=0.25ms is limit of 9V driven LCD
					// this is the default setup, for combing more LCD, this keeps the same
	int duty;  // default is 4, 0.5ms, which is just enough for a single piece of LCD to go up and then down in 4ms
	double sample_rate;
	vector<unsigned char> decode(const filter_out_t* buffer, int length, int bytes);
	int o_start_index;
	vector<float> union_curve;
	vector<float> o_ref;
	vector<float> amplitudes;
	int o_middle;
	int o_ct_start;
	int o_data_start;
};

SingleDSM_Decoder decoder;

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
	if (record["real_frequency"].existed() && record["real_frequency"].type() == BSON_TYPE_DOUBLE) {
		decoder.frequency = record["real_frequency"].value<double>();
	} else decoder.frequency = 4000;
	decoder.sample_rate = 56875;
	// get data in hex format string
	assert(record["data"].existed() && record["data"].type() == BSON_TYPE_UTF8 && "data needed in hex format");
	vector<uint8_t> data = record["data"].get_bytes_from_hex_string();
	assert(!data.empty() && "data cannot be empty");
	printf("data.size(): %d\n", (int)data.size());

	// decode
	assert(record["record_id"].existed() && record["record_id"].type() == BSON_TYPE_UTF8 && "record_id needed");
	string binary_id_str = record["record_id"].value<string>();
	assert(MongoDat::isPossibleOID(binary_id_str.c_str()) && "record_id invalid");
	bson_oid_t record_id = MongoDat::parseOID(binary_id_str.c_str());
	vector<char> binary = mongodat.get_binary_file(record_id);
	vector<uint8_t> decoded = decoder.decode((const filter_out_t*)binary.data(), binary.size()/sizeof(filter_out_t), data.size());
	printf("origin: %s\n", MongoDat::dump(data).c_str());
	printf("decode: %s\n", MongoDat::dump(decoded).c_str());

	double BER = 0;
	for (size_t i=0; i<data.size() * 8; ++i) {
		if ((decoded[i/8] ^ data[i/8]) & (1 << (i%8))) ++BER;
	}
	BER /= data.size() * 8;
	printf("BER: %f %%", BER * 100);

	// save union curve (optional for debug)
	// mongodat.upload_record("union curve", (float*)decoder.union_curve.data(), 1, decoder.union_curve.size(), NULL, "simple union of four", 1/56.875, "time(ms)", 1, "data");
	// printf("upload union curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
	// mongodat.upload_record("reference all", (float*)decoder.o_ref.data(), 1, decoder.o_ref.size(), NULL, "reference", 1/56.875, "time(ms)", 1, "data");
	// printf("upload reference curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// then record everything computed
#define RECORD_INT32(name) printf(#name": %d\n", decoder.name); record[#name] = decoder.name;
#define RECORD_DOUBLE(name) printf(#name": %f\n", decoder.name); record[#name] = decoder.name;
	RECORD_INT32(o_middle)
	RECORD_INT32(o_ct_start)
	RECORD_INT32(o_data_start)
	record["BER"] = BER;
	record["decoded"] = MongoDat::dump(decoded);

	// save file to database
	record.save();

	mongodat.close();
	record.remove();

	return 0;
}

vector<unsigned char> SingleDSM_Decoder::decode(const filter_out_t* buffer, int length, int bytes) {
	vector<unsigned char> data;
	data.resize(bytes);
	typedef struct { int16_t s[4]; } uni_out_t;
	const uni_out_t* buffer2 = (const uni_out_t*)buffer;
	// evaluate noise ratio and all-zero level
	int zero_start = 0.020 * sample_rate;  // to avoid some strange points at begining
	int zero_padding = 0.020 * sample_rate;
	int zero_end = zero_start + zero_padding;
	assert(length >= zero_padding && "too short");
	printf("evaluate noise ratio and all-zero level from %d (%f ms) to %d (%f ms)\n", zero_start, (double)zero_start / sample_rate * 1000, zero_end, zero_end / sample_rate * 1000);
	double zero_avr[4] = {0};
	for (int i=zero_start; i<zero_end; ++i) for (int j=0; j<4; ++j) zero_avr[j] += buffer2[i].s[j];
	for (int j=0; j<4; ++j) zero_avr[j] /= zero_padding;
	printf("zero_avr: %f %f %f %f\n", zero_avr[0], zero_avr[1], zero_avr[2], zero_avr[3]);
	double stddev[4] = {0};
	for (int i=zero_start; i<zero_end; ++i) for (int j=0; j<4; ++j) stddev[j] += pow(buffer2[i].s[j] - zero_avr[j], 2);
	for (int j=0; j<4; ++j) stddev[j] = sqrt(stddev[j] / zero_padding);
	printf("stddev: %f %f %f %f\n", stddev[0], stddev[1], stddev[2], stddev[3]);
	// next find the rough fast edge start point where delta is larger than 100x stddev
	int rough_start = 0;
	double dev_th = 0;
	for (int j=0; j<4; ++j) dev_th += abs(stddev[j]);
	dev_th *= 10;
	if (dev_th > 300) dev_th = 300;
	printf("dev_th: %f\n", dev_th);
	for (int i=zero_start + zero_padding; i<length; ++i) {
		double dev = 0;
		for (int j=0; j<4; ++j) dev += abs(buffer2[i].s[j] - zero_avr[j]);
		if (dev > dev_th) { rough_start = i; break; }
	}
	assert(rough_start != 0 && "cannot find rough start of first fast edge");
	printf("rough_start: %d (%f ms)\n", rough_start, rough_start / sample_rate * 1000);
	// evaluate all-one level and noise ratio
	int one_padding = 128 * sample_rate / frequency - 0.004 * sample_rate;
	int one_start = rough_start + 0.002 * sample_rate;  // about 2ms after rough_start
	assert(length >= one_start + one_padding && "too short");
	printf("evaluate all-one level from %d (%f ms) to %d (%f ms)\n", one_start, one_start / sample_rate * 1000, one_start + one_padding, (one_start + one_padding) / sample_rate * 1000);
	double one_avr[4] = {0};
	for (int i=0; i<one_padding; ++i) for (int j=0; j<4; ++j) one_avr[j] += buffer2[one_start+i].s[j];
	for (int j=0; j<4; ++j) one_avr[j] /= one_padding;
	printf("one_avr: %f %f %f %f\n", one_avr[0], one_avr[1], one_avr[2], one_avr[3]);
	// change the four curve into one curve
	union_curve.clear(); union_curve.resize(length);
	double fenmu = sqrt(pow(one_avr[0]-zero_avr[0], 2) + pow(one_avr[1]-zero_avr[1], 2) + pow(one_avr[2]-zero_avr[2], 2) + pow(one_avr[3]-zero_avr[3], 2));
	for (int i=0; i<length; ++i) {
		double fenzi = sqrt(pow(buffer2[i].s[0]-zero_avr[0], 2) + pow(buffer2[i].s[1]-zero_avr[1], 2) + pow(buffer2[i].s[2]-zero_avr[2], 2) + pow(buffer2[i].s[3]-zero_avr[3], 2));
		union_curve[i] = fenzi / fenmu;
	}
	// next find the sample almost equal to middle
	int middle_start = rough_start - 100;
	double last_middle_delta = 1;
	o_middle = 0;
	for (int i=0; i<200; ++i) {
		double delta = abs(union_curve[middle_start + i] - 0.5);
		if (delta < 0.25) {  // must be in 1/4 to deprecate noise
			if (delta < last_middle_delta) {
				o_middle = middle_start + i;
				last_middle_delta = delta;
			} else break;
		}
	}
	assert(o_middle && "cannot find middle, strange");
	printf("found accurate middle at %d (%f ms)\n", o_middle, o_middle / sample_rate * 1000);
	// then record reference signal
	int fast_speed = 0.0004 * sample_rate;
	int slow_speed = 0.006 * sample_rate;
	printf("fast_speed is set to %d (%f ms)\n", fast_speed, fast_speed / sample_rate * 1000);
	printf("slow_speed is set to %d (%f ms)\n", slow_speed, slow_speed / sample_rate * 1000);
	int start_exact = o_middle - (fast_speed / 2);
	int dura_128 = 128 * sample_rate / frequency;
	int dura_duty = duty * sample_rate / frequency;
	int slow_ref_start = start_exact + dura_128;
	printf("record fast edge reference from %d (%f ms) to %d (%f ms)\n", start_exact, start_exact / sample_rate * 1000, start_exact + fast_speed, (start_exact + fast_speed) / sample_rate * 1000);
	printf("record slow edge reference from %d (%f ms) to %d (%f ms)\n", slow_ref_start, slow_ref_start / sample_rate * 1000, slow_ref_start + slow_speed, (slow_ref_start + slow_speed) / sample_rate * 1000);
	// build reference function here
	// assert(NLCD == 16 && combine == 1);
	int dsm_order = NLCD / combine;
	vector<int> from_last_one;
	from_last_one.insert(from_last_one.end(), dsm_order, dura_duty + slow_speed);
	auto tail_all = [&](int idx)->float {
		int dura = from_last_one[idx]; assert(dura >= 0);
		if (dura < fast_speed) return union_curve[start_exact + dura];
		if (dura < dura_duty) return 1;
		if (dura < dura_duty + slow_speed) return union_curve[slow_ref_start + (dura - dura_duty)];
		return 0;
	};
	// output a reference for debug
	o_ref.clear(); from_last_one[0] = 0;
	for (int i=0; i<dura_duty + slow_speed + 200; ++i) {
		o_ref.push_back(tail_all(0)); ++from_last_one[0];
	} from_last_one[0] = dura_duty + slow_speed;
	// channel training get amplitudes
	amplitudes.resize(dsm_order);
#if 0
	o_ct_start = start_exact + (128 + 128) * 56.875 / 4;
	printf("channel training start at %d (%f ms)\n", o_ct_start, o_ct_start / sample_rate * 1000);
	float ct_last = 0;
	int dura_ct_fast = ct_fast * 56.875 / 4;
	for (int i=0; i<16; ++i) {
		int sub_start = o_ct_start + i * dura_ct_fast + fast_speed;
		float avr = 0;
		for (int k=0; k < dura_ct_fast - fast_speed; ++k) avr += union_curve[sub_start + k];
		avr /= dura_ct_fast - fast_speed;
		amplitudes[i] = avr - ct_last;
		printf("amplitude: %f\n", amplitudes[i]);
		ct_last = avr;
	}
#else
	for (int i=0; i<dsm_order; ++i) amplitudes[i] = 1./dsm_order;
#endif
	auto simulated_all = [&]()->float {
		float sum = 0; for (int i=0; i<dsm_order; ++i) sum += amplitudes[i] * tail_all(i); return sum;
	};
	// data should start at
	o_data_start = start_exact + (128 + 128 + ct_fast * dsm_order + ct_slow) * sample_rate / frequency;
	float data_interval = (float)cycle / dsm_order * sample_rate / frequency;
	printf("data start at %d (%f ms)\n", o_data_start, o_data_start / sample_rate * 1000);
	printf("data_interval: %f (%f ms)\n", data_interval, data_interval / sample_rate * 1000);
	int duty_cnt = bytes * 8;
	for (int i=0; i<duty_cnt; ++i) {
		int sub_start = o_data_start + data_interval * i;
		int sub_end = o_data_start + data_interval * (i + 1);
		// judge in data_interval samples, whether current bit is 0 or 1
		int idx = i % dsm_order;
		float sd0 = 0, sd1 = 0;  // standard deviation
		int record_last_one = from_last_one[idx];
#if 0
		for (int j=sub_start; j<sub_end; ++j) {
			// if current bit is 1
			from_last_one[idx] = j - sub_start;
			sd1 += pow(simulated_all() - union_curve[j], 2);
			// if current bit is 0
			from_last_one[idx] = record_last_one + (j - sub_start);
			sd0 += pow(simulated_all() - union_curve[j], 2);
			// move all data ahead
			for (int k=0; k<16; ++k) ++from_last_one[k];
		}
#else
		// first get average value
		float avr_union = 0, avr0 = 0, avr1 = 0;
		for (int j=sub_start; j<sub_end; ++j) {
			avr_union += union_curve[j];
			from_last_one[idx] = j - sub_start;
			avr1 += simulated_all();
			from_last_one[idx] = record_last_one + (j - sub_start);
			avr0 += simulated_all();
			for (int k=0; k<dsm_order; ++k) ++from_last_one[k];
		}
		for (int k=0; k<dsm_order; ++k) from_last_one[k] -= (sub_end - sub_start);
		avr_union /= sub_end - sub_start;
		avr0 /= sub_end - sub_start;
		avr1 /= sub_end - sub_start;
		for (int j=sub_start; j<sub_end; ++j) {
			// if current bit is 1
			from_last_one[idx] = j - sub_start;
			sd1 += pow(simulated_all() - avr1 - union_curve[j] + avr_union, 2);
			from_last_one[idx] = record_last_one + (j - sub_start);
			sd0 += pow(simulated_all() - avr0 - union_curve[j] + avr_union, 2);
			for (int k=0; k<dsm_order; ++k) ++from_last_one[k];
		}
#endif
		// printf("%f %f\n", sd0, sd1);
		// assert(i < 32);
		if (sd1 < sd0) {  // 1 is better
			from_last_one[idx] = sub_end - sub_start;
			data[i/8] |= 1 << (i%8);
		} else from_last_one[idx] = record_last_one + sub_end - sub_start;
	}

	return data;
}
