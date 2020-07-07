#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "modulator.h"
#include "soxr/soxr.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;
FastDSM_Encoder encoder;

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
	NEED_RECORD_INT32(preamble_repeat)  // repeat for how many times, usually 50 is good enough, which is about 3s
	NEED_RECORD_INT32(o_preamble_length);
	assert(record["frequency"].existed() && record["frequency"].type() == BSON_TYPE_DOUBLE);
	printf("frequency: %f\n", (float)(encoder.frequency = record["frequency"].value<double>()));
	assert(record["record_id"].existed() && record["record_id"].type() == BSON_TYPE_UTF8 && "record_id needed");
	string binary_id_str = record["record_id"].value<string>();
	assert(MongoDat::isPossibleOID(binary_id_str.c_str()) && "record_id invalid");
	bson_oid_t record_id = MongoDat::parseOID(binary_id_str.c_str());
	vector<char> binary = mongodat.get_binary_file(record_id);
	typedef struct { int16_t s[4]; } uni_out_t;
	const uni_out_t* data = (const uni_out_t*)binary.data();
	int length = binary.size() / sizeof(uni_out_t);
	typedef struct { float s[4]; } float_out_t;
	vector<float_out_t> float_in; float_in.resize(length);
	for (int i=0; i<length; ++i) for (int j=0; j<4; ++j) float_in[i].s[j] = data[i].s[j];

	// first resampling them from 56.875k to 80k
	double irate = 56875;
	double orate = 80000;
	size_t olen = (size_t)(length * orate / irate + .5);
	// size_t odone;
	vector<float_out_t> float_out; float_out.resize(olen);
	soxr_oneshot(irate, orate, 4, (const float*)float_in.data(), length, NULL, (float*)float_out.data(), olen, NULL, NULL, NULL, NULL);

	// output drawing for debug soxr
	// mongodat.upload_record("soxr out", (float*)float_out.data(), 4, float_out.size(), NULL, "soxr out", 1/80., "time(ms)", 1, "Ia,Qa,Ib,Qb");
	// printf("upload soxr out with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// measure zero-level
	int zero_padding = 0.1 * orate;  // 100ms
	double zero_avr[4] = {0};
	for (int i=0; i<zero_padding; ++i) for (int j=0; j<4; ++j) zero_avr[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) zero_avr[j] /= zero_padding;
	printf("zero_avr: %f %f %f %f\n", zero_avr[0], zero_avr[1], zero_avr[2], zero_avr[3]);

	// then set it as zero
	for (size_t i=0; i<olen; ++i) for (int j=0; j<4; ++j) float_out[i].s[j] -= zero_avr[j];
	
	// output drawing for debug soxr
	// mongodat.upload_record("soxr out", (float*)float_out.data(), 4, float_out.size(), NULL, "soxr out", 1/80., "time(ms)", 1, "Ia,Qa,Ib,Qb");
	// printf("upload soxr out with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// find the maximum change
	double max_change[4] = {0};
	for (size_t i=0; i<olen; ++i) for (int j=0; j<4; ++j) { double change = fabs(float_out[i].s[j]); max_change[j] = max(max_change[j], change);}
	double max_change_sum = 0;
	for (int j=0; j<4; ++j) max_change_sum += max_change[j];
	printf("max_change: %f %f %f %f, sum: %f\n", max_change[0], max_change[1], max_change[2], max_change[3], max_change_sum);

	// find the first that reach 1/5 change sum
	int rough_start = 0;
	for (; rough_start < (int)olen; ++rough_start) {
		double change = 0;
		for (int j=0; j<4; ++j) change += fabs(float_out[rough_start].s[j]);
		if (change > max_change_sum/5) break;
	} printf("found rough start at %d (%f ms)\n", rough_start, rough_start / orate * 1000);

	// sum preamble_repeat-1 together to 0
	int copy_start = rough_start - 2e-3*orate;
	int copy_interval = encoder.o_preamble_length * orate / encoder.frequency;
	for (int i=1; i<encoder.preamble_repeat; ++i) {
		int start = copy_start + i * copy_interval;
		for (int j=0; j<copy_interval; ++j) {
			for (int k=0; k<4; ++k) float_out[copy_start + j].s[k]+= float_out[start + j].s[k];
		}
	}

	// output drawing for debug soxr
	// mongodat.upload_record("soxr out", (float*)float_out.data(), 4, float_out.size(), NULL, "soxr out", 1/80., "time(ms)", 1, "Ia,Qa,Ib,Qb");
	// printf("upload soxr out with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// fine tune rough_start, measure +1.5ms~+2.5ms
	double ones[4] = {0};
	int ones_start = rough_start + 1.5e-3*orate;
	int ones_end = rough_start + 2.5e-3*orate;
	for (int i=ones_start; i<ones_end; ++i) for (int j=0; j<4; ++j) ones[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) ones[j] /= (ones_end - ones_start);
	printf("ones: %f %f %f %f\n", ones[0], ones[1], ones[2], ones[3]);
	double middle = (fabs(ones[0]) + fabs(ones[1]) + fabs(ones[2]) + fabs(ones[3])) / 2;
	int search_start = rough_start - 2e-3*orate;
	int search_idx;
	for (search_idx=search_start; search_idx<ones_start; ++search_idx) {
		double sum = 0;
		for (int j=0; j<4; ++j) sum += fabs(float_out[search_idx].s[j]);
		if (sum > middle) break;
	}
	printf("found middle at %d (%f ms)\n", search_idx, search_idx/orate*1e3);
	rough_start = search_idx;  // use accurate instead

	// from rough_start, measure +1.5ms~+2.5ms and +25~+29ms, test B1
	double B1_avr[4] = {0};
	int B1_start_1 = rough_start + 1.5e-3*orate;
	int B1_end_1 = rough_start + 2.5e-3*orate;
	int B1_start_2 = rough_start + 25e-3*orate;
	int B1_end_2 = rough_start + 29e-3*orate;
	printf("measuring B1 from %d (%f ms) to %d (%f ms) , and %d (%f ms) to %d (%f ms)\n", B1_start_1, B1_start_1/orate*1e3, B1_end_1, B1_end_1/orate*1e3, B1_start_2, B1_start_2/orate*1e3, B1_end_2, B1_end_2/orate*1e3);
	for (int i=B1_start_1; i<B1_end_1; ++i) for (int j=0; j<4; ++j) B1_avr[j] += float_out[i].s[j];
	for (int i=B1_start_2; i<B1_end_2; ++i) for (int j=0; j<4; ++j) B1_avr[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) B1_avr[j] /= (B1_end_1 - B1_start_1 + B1_end_2 - B1_start_2);
	printf("B1_avr: %f %f %f %f\n", B1_avr[0], B1_avr[1], B1_avr[2], B1_avr[3]);

	// from rough_start, measure +14ms~+18ms, test B2
	double B2_avr[4] = {0};
	int B2_start_1 = rough_start + 14e-3*orate;
	int B2_end_1 = rough_start + 18e-3*orate;
	printf("measuring B2 from %d (%f ms) to %d (%f ms)\n", B2_start_1, B2_start_1/orate*1e3, B2_end_1, B2_end_1/orate*1e3);
	for (int i=B2_start_1; i<B2_end_1; ++i) for (int j=0; j<4; ++j) B2_avr[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) B2_avr[j] /= (B2_end_1 - B2_start_1);
	printf("B2_avr: %f %f %f %f\n", B2_avr[0], B2_avr[1], B2_avr[2], B2_avr[3]);

	// from rough_start, measure +5.5ms~+6.5ms and +34ms~+38ms, test B3
	double B3_avr[4] = {0};
	int B3_start_1 = rough_start + 5.5e-3*orate;
	int B3_end_1 = rough_start + 6.5e-3*orate;
	int B3_start_2 = rough_start + 34e-3*orate;
	int B3_end_2 = rough_start + 38e-3*orate;
	printf("measuring B3 from %d (%f ms) to %d (%f ms) , and %d (%f ms) to %d (%f ms)\n", B3_start_1, B3_start_1/orate*1e3, B3_end_1, B3_end_1/orate*1e3, B3_start_2, B3_start_2/orate*1e3, B3_end_2, B3_end_2/orate*1e3);
	for (int i=B3_start_1; i<B3_end_1; ++i) for (int j=0; j<4; ++j) B3_avr[j] += float_out[i].s[j];
	for (int i=B3_start_2; i<B3_end_2; ++i) for (int j=0; j<4; ++j) B3_avr[j] += float_out[i].s[j];
	for (int j=0; j<4; ++j) B3_avr[j] /= (B3_end_1 - B3_start_1 + B3_end_2 - B3_start_2);
	printf("B3_avr: %f %f %f %f\n", B3_avr[0], B3_avr[1], B3_avr[2], B3_avr[3]);

	// first compute IaQa, IbQb
	double A_max = fabs(B1_avr[0]) + fabs(B1_avr[1]);
	double *A_ptr = B1_avr;
	if (fabs(B2_avr[0]) + fabs(B2_avr[1]) > A_max) A_ptr = B2_avr;
	if (fabs(B3_avr[0]) + fabs(B3_avr[1]) > A_max) A_ptr = B3_avr;
	double B_max = fabs(B1_avr[2]) + fabs(B1_avr[3]);
	double *B_ptr = B1_avr;
	if (fabs(B2_avr[2]) + fabs(B2_avr[3]) > B_max) B_ptr = B2_avr;
	if (fabs(B3_avr[2]) + fabs(B3_avr[3]) > B_max) B_ptr = B3_avr;
	// then convert them
	typedef struct { float s[2]; } two_t;
	vector<two_t> two; two.resize(olen);
	for (size_t i=0; i<olen; ++i) {
		two[i].s[0] = A_ptr[0] * float_out[i].s[0] - A_ptr[1] * float_out[i].s[1];
		two[i].s[1] = B_ptr[2] * float_out[i].s[2] - B_ptr[3] * float_out[i].s[3];
	}
	// output drawing for debug soxr
	// mongodat.upload_record("two out", (float*)two.data(), 2, two.size(), NULL, "soxr out", 1/80., "time(ms)", 1, "I,Q");
	// printf("upload two out with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// recompute average, linear
	double B1_avr_two[2] = { A_ptr[0] * B1_avr[0] - A_ptr[1] * B1_avr[1], B_ptr[2] * B1_avr[2] - B_ptr[3] * B1_avr[3] };
	double B2_avr_two[2] = { A_ptr[0] * B2_avr[0] - A_ptr[1] * B2_avr[1], B_ptr[2] * B2_avr[2] - B_ptr[3] * B2_avr[3] };
	double B3_avr_two[2] = { A_ptr[0] * B3_avr[0] - A_ptr[1] * B3_avr[1], B_ptr[2] * B3_avr[2] - B_ptr[3] * B3_avr[3] };

	// solve equation a,b,c,d
	// a * B1_avr_two[0] + b * B1_avr_two[1] = 0
	// c * B1_avr_two[0] + d * B1_avr_two[1] = 1
	// a * B2_avr_two[0] + b * B2_avr_two[1] = 1
	// c * B2_avr_two[0] + d * B2_avr_two[1] = 0
	double a = B1_avr_two[1] / ( B2_avr_two[0] * B1_avr_two[1] - B1_avr_two[0] * B2_avr_two[1] );
	double b = B1_avr_two[0] / ( B1_avr_two[0] * B2_avr_two[1] - B2_avr_two[0] * B1_avr_two[1] );
	double c = B2_avr_two[1] / ( B1_avr_two[0] * B2_avr_two[1] - B2_avr_two[0] * B1_avr_two[1] );
	double d = B2_avr_two[0] / ( B2_avr_two[0] * B1_avr_two[1] - B1_avr_two[0] * B2_avr_two[1] );
	
	for (size_t i=0; i<olen; ++i) {
		float I = two[i].s[0], Q = two[i].s[1];
		two[i].s[0] = a * I + b * Q;
		two[i].s[1] = c * I + d * Q;
	}
	// output drawing for debug soxr
	// mongodat.upload_record("two out", (float*)two.data(), 2, two.size(), NULL, "soxr out", 1/80., "time(ms)", 1, "I,Q");
	// printf("upload two out with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	// sanity check
	double B3I = a * B3_avr_two[0] + b * B3_avr_two[1];
	double B3Q = c * B3_avr_two[0] + d * B3_avr_two[1];
	printf("B3: %f %f\n", B3I, B3Q);

	// output reference from rough_start-1ms to 4096 points
	mongodat.upload_record("reference", (float*)(two.data() + rough_start - (int)(1e-3*orate)), 2, 4096, NULL, "reference", 1/80., "time(ms)", 1, "I,Q");
	printf("upload reference file: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
	record["preamble_id"] = MongoDat::OID2str(mongodat.get_fileID());

	// save file to database
	printf("saved id: %s\n", MongoDat::OID2str(*record.save()).c_str());

	mongodat.close();
	record.remove();

	return 0;
}
