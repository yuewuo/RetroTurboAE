#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "MillerModulatorRecordData.h"
#include <vector>


const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;
MillerModulator encoder;

// UNUSED OLD CODE
// static const unsigned char miller_table[16] = {0xCC, 0xCE, 0x38, 0xC6, 0x1C, 0x1E, 0x18, 0xE6, 0x8C, 0x8E, 0x78, 0x86, 0x9C, 0x9E, 0x98, 0x66};
// static int encode_miller(unsigned char *tbuf, int tlen)
// {
//     tbuf[tlen++] = 0;
//     int i, j, tlen2 = tlen * 2;
//     for (i = tlen - 1, j = tlen2; i >= 0; --i)
//     {
//         tbuf[--j] = miller_table[tbuf[i] >> 4];
//         tbuf[--j] = miller_table[tbuf[i] & 0xF];
//     }
//     if (~tbuf[0] & 0x02)
//         tbuf[0] = ~tbuf[0];
//     for (i = 1; i < tlen2; ++i)
//     {
//         if (tbuf[i] & 0x02)
//         {
//             if (tbuf[i - 1] & 0x80)
//                 tbuf[i] = ~tbuf[i];
//         }
//         else
//         {
//             if (!(tbuf[i - 1] & 0x40))
//                 tbuf[i] = ~tbuf[i];
//         }
//     }
//     tbuf[tlen2] = (tbuf[tlen2 - 1] >> 7) & 1;
//     return (tlen << 4) + 1;
// }

int main(int argc, char** argv) {



    /*
     * Database management
     */

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
	NEED_RECORD_INT32(length)
	NEED_RECORD_INT32(reverse)
#undef NEED_RECORD_INT32
	assert(record["real_frequency"].existed() && record["real_frequency"].type() == BSON_TYPE_DOUBLE); \
	printf("real_frequency: %f\n", (float)(encoder.frequency = record["real_frequency"].value<double>()));


	vector<unsigned char> buf;
	buf.resize(2 * encoder.length + 1);

    /*
     * Insert packet to database
     */
    srand(time(NULL));
// #define rand() 0
	string data_string;
	constexpr uint8_t ref1[] = {
	 0x2b, 0xe3, 0x8b, 0x29, 0x09, 0x2d, 0x58, 0xf0
	};
	for(int i = 0; i < encoder.length; i++)
	{
		buf[i] = rand() % 256;
		if(i == 0)
			buf[i] |= 1;
		// buf[i] = ref1[i];
		char strbuf[4];
		sprintf(strbuf, "%02X", buf[i]);
		data_string += strbuf;
	}
// #undef rand
	
    record["data"] = data_string;

	// int symbol_length = encode_miller(buf.data(), encoder.length);
    encoder.Generate(buf.data(), encoder.length);
	

    record["packet"].remove();
    record["packet"].build_array();
    record["packet"].append(encoder.packet);
    
	// save file to database
	printf("saved id: %s\n", MongoDat::OID2str(*record.save()).c_str());

	mongodat.close();
	record.remove();

	return 0;
}
