#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char**argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 2) {
		printf("usage: <record_id (12byte in hex)>\n");
		return -1;
	}
	const char* record_id = argv[1];
	bson_oid_t id = MongoDat::parseOID(record_id);

	assert(mongodat.is_gridfs_file_existed(id) && "record not existed");

	printf("id: %s\n", MongoDat::OID2str(id).c_str());
	printf("type: %s\n", mongodat.typeof_record(id).c_str());

	bson_t *metadata = mongodat.get_metadata_record(id);
	bson_delete_field(metadata, "computed_squaresum_avr");   // remove original field
	char numbuf[32];
	bson_t arr;
	BSON_APPEND_ARRAY_BEGIN(metadata, "computed_squaresum_avr", &arr);
	int dimension = getint32frombson(metadata, "dimension", -1);
	int length = getint32frombson(metadata, "length", -1);
	vector<double> buffer = mongodat.get_scaled_data_record(id);
	for (int i=0; i<dimension; ++i) {
		sprintf(numbuf, "%d", i);
		double sum = 0;
		for (int j=0; j<length; ++j) {
			sum += buffer[i+j*dimension];  // this is inefficient for cache ! just demo
		}
		// printf("sum = %f\n", sum);
		double avr = sum / length;
		double square_sum = 0;
		for (int j=0; j<length; ++j) {
			double tmp = buffer[i+j*dimension] - avr;  // this is inefficient for cache ! just demo
			square_sum += tmp * tmp;
		}
		printf("square_sum: %f, square_avr: %f\n", square_sum, square_sum/length);
		BSON_APPEND_DOUBLE(&arr, numbuf, square_sum/length);  // output to database
	}
	bson_append_array_end(metadata, &arr);

	mongodat.save_metadata_record(id, metadata);
	bson_destroy(metadata);

	mongodat.close();

	return 0;
}
