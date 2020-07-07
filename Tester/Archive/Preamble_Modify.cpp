// this is used to modify preamble file, for offset adjustment
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include <vector>
#include <string>
#include <map>
#include <complex>
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 2) {
		printf("usage: <preamble_ref_id>\n");
		return -1;
	}

	const char* preamble_ref_id_str = argv[1];

	bson_oid_t preamble_ref_id = MongoDat::parseOID(preamble_ref_id_str);
	vector<char> preamble_ref_binary = mongodat.get_binary_file(preamble_ref_id);
	assert(preamble_ref_binary.size() % sizeof(complex<float>) == 0 && "preamble_ref alignment error");
	vector<complex<float>> preamble_ref; preamble_ref.resize(preamble_ref_binary.size() / sizeof(complex<float>));
	memcpy(preamble_ref.data(), preamble_ref_binary.data(), preamble_ref_binary.size());

	double sample_rate = 80000;

	// add 0 to the front of preamble
#define ADD_TIME_OFFSET (int)(0.125 * 1e-3 * sample_rate)
	preamble_ref.insert(preamble_ref.begin(), ADD_TIME_OFFSET, complex<float>(0,0));
	preamble_ref.erase(preamble_ref.end() - ADD_TIME_OFFSET, preamble_ref.end());

	mongodat.upload_record("preamble_ref", (float*)preamble_ref.data(), 2, preamble_ref.size()
		, NULL, "simple union of four", 1/sample_rate*1000, "time(ms)", 1, "I,Q");
	printf("upload preamble_ref with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

	return 0;

}
