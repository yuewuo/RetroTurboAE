/*
 * This program is to evaluate the attributes of LCD
 * 
 * refer to "WebGUI/lua/190720_test_multi_frequency.lua" for more information
 * 
 * evaluate 0-1-0 and 1-0-1 transition respectively, output standard deviation of one bit error
 * 
 */

#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

vector<float> union_curve_parse(const vector<char>& binary, double sample_rate, int target_length);

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
	assert(record["real_frequency"].existed() && record["real_frequency"].type() == BSON_TYPE_DOUBLE);
	double frequency = record["real_frequency"].value<double>();
	assert(record["test_cnt"].existed() && record["test_cnt"].type() == BSON_TYPE_INT32);
	int test_cnt = record["test_cnt"].value<int32_t>();
	assert(record["count_1"].existed() && record["count_1"].type() == BSON_TYPE_INT32);
	int count_1 = record["count_1"].value<int32_t>();
	assert(record["count_0"].existed() && record["count_0"].type() == BSON_TYPE_INT32);
	int count_0 = record["count_0"].value<int32_t>();

	// compute parameters
	int preamble_cnt = count_1 + count_0;
	int one_curve_cnt = 1 + count_0 + count_1 + 1 + count_1 + count_0;
	int all_curves_cnt = one_curve_cnt * test_cnt;
	int all_cnt = preamble_cnt + all_curves_cnt;

	double sample_rate = 56875;
	assert(record["record_id"].existed() && record["record_id"].type() == BSON_TYPE_UTF8);
	string record_id = record["record_id"].value<string>();
	bson_oid_t data_id = MongoDat::parseOID(record_id.c_str());
	vector<char> binary = mongodat.get_binary_file(data_id);
	vector<float> union_curve = union_curve_parse(binary, sample_rate, (all_cnt / frequency) * sample_rate);
	
	mongodat.upload_record("union curve", (float*)union_curve.data(), 1, union_curve.size()
		, NULL, "simple union of four", 1/sample_rate*1000, "time(ms)", 1, "data");
	printf("upload union curve with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	int trans_length = sample_rate / frequency;
	printf("trans_length: %d\n", trans_length);

	// get the 0-1-0 transition
	vector<float> trans_010; trans_010.resize(trans_length);
	int trans_010_start = preamble_cnt * sample_rate / frequency;
	for (int i=0; i<test_cnt; ++i) {
		int trans_010_i = trans_010_start + i * one_curve_cnt * sample_rate / frequency;
		// printf("trans_010_i: %d, (%f ms)\n", trans_010_i, trans_010_i / sample_rate * 1000);
		for (int j=0; j<trans_length; ++j) {
			trans_010[j] += union_curve[trans_010_i + j];
		}
	}
	double trans_010_stddev = 0;
	for (int j=0; j<trans_length; ++j) {
		trans_010[j] /= test_cnt;
		trans_010_stddev += (trans_010[j]) * (trans_010[j]);
	}
	trans_010_stddev /= trans_length;

	// get the 1-0-1 transition
	vector<float> trans_101; trans_101.resize(trans_length);
	int trans_101_start = (preamble_cnt + 1 + count_0 + count_1) * sample_rate / frequency;
	for (int i=0; i<test_cnt; ++i) {
		int trans_101_i = trans_101_start + i * one_curve_cnt * sample_rate / frequency;
		// printf("trans_010_i: %d, (%f ms)\n", trans_010_i, trans_010_i / sample_rate * 1000);
		for (int j=0; j<trans_length; ++j) {
			trans_101[j] += union_curve[trans_101_i + j];
		}
	}
	double trans_101_stddev = 0;
	for (int j=0; j<trans_length; ++j) {
		trans_101[j] /= test_cnt;
		trans_101_stddev += (1 - trans_101[j]) * (1 - trans_101[j]);
	}
	trans_101_stddev /= trans_length;

	// generate a curve of 010 and 101
	vector<float> trans;
	trans.insert(trans.end(), trans_010.begin(), trans_010.end());
	trans.insert(trans.end(), trans_101.begin(), trans_101.end());
	// for debug
	mongodat.upload_record("trans", (float*)trans.data(), 1, trans.size()
		, NULL, "trans", 1/sample_rate*1000, "time(ms)", 1, "data");
	bson_oid_t trans_id = mongodat.get_fileID();
	string trans_id_str = MongoDat::OID2str(trans_id);
	printf("upload trans with ID: %s\n", trans_id_str.c_str());

	// save record
	record["trans_length"] = trans_length;
	record["trans_id"] = trans_id_str;
	record["trans_010_stddev"] = trans_010_stddev;
	record["trans_101_stddev"] = trans_101_stddev;
	printf("trans_010_stddev: %f\n", trans_010_stddev);
	printf("trans_101_stddev: %f\n", trans_101_stddev);
	record.save();

	mongodat.close();

	return 0;
}

vector<float> union_curve_parse(const vector<char>& binary, double sample_rate, int target_length) {
	typedef struct { int16_t s[4]; } uni_out_t;
	const uni_out_t* buffer2 = (const uni_out_t*)binary.data();
	int length = binary.size() / sizeof(uni_out_t);
	// evaluate noise ratio and all-zero level
	int zero_start = 0;  // to avoid some strange points at begining
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
	int one_padding = 1.5e-3 * sample_rate;  // use 1.5ms
	int one_start = rough_start + 0.0005 * sample_rate;  // about 0.5ms after rough_start
	assert(length >= one_start + one_padding && "too short");
	printf("evaluate all-one level from %d (%f ms) to %d (%f ms)\n", one_start, one_start / sample_rate * 1000, one_start + one_padding, (one_start + one_padding) / sample_rate * 1000);
	double one_avr[4] = {0};
	for (int i=0; i<one_padding; ++i) for (int j=0; j<4; ++j) one_avr[j] += buffer2[one_start+i].s[j];
	for (int j=0; j<4; ++j) one_avr[j] /= one_padding;
	printf("one_avr: %f %f %f %f\n", one_avr[0], one_avr[1], one_avr[2], one_avr[3]);
	// change the four curve into one curve
	vector<float> union_curve; union_curve.resize(length);
	double fenmu = sqrt(pow(one_avr[0]-zero_avr[0], 2) + pow(one_avr[1]-zero_avr[1], 2) + pow(one_avr[2]-zero_avr[2], 2) + pow(one_avr[3]-zero_avr[3], 2));
	for (int i=0; i<length; ++i) {
		double fenzi = sqrt(pow(buffer2[i].s[0]-zero_avr[0], 2) + pow(buffer2[i].s[1]-zero_avr[1], 2) + pow(buffer2[i].s[2]-zero_avr[2], 2) + pow(buffer2[i].s[3]-zero_avr[3], 2));
		union_curve[i] = fenzi / fenmu;
	}
	int middle_start = rough_start - 100;
	double last_middle_delta = 1;
	int o_middle = 0;
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
	// then slice to start point
#define UNION_CURVE_HALF_FAST 0.22
	int new_middle = UNION_CURVE_HALF_FAST * sample_rate / 1000;
	int union_curve_start = o_middle - new_middle;  // 0.7ms
	printf("new middle should at %d (%f ms)\n", new_middle, new_middle / sample_rate * 1000);
	union_curve.erase(union_curve.begin(), union_curve.begin() + union_curve_start);
	assert((int)union_curve.size() >= target_length && "curve length not enough");
	union_curve.erase(union_curve.begin() + target_length, union_curve.end());
	return union_curve;
}
