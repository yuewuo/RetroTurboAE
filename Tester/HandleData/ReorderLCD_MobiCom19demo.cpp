/*
 * This program works with MobiCom'19 tag, which is composed of 4 LCD (2×0°+2×45°),
 * each LCD has 16 individual pixels (4× 8:4:2:1). Since this is different from default 
 * settings of RetroTurbo, this program is designed to convert from RetroTurbo's to MobiCom19's.
 * 
 * RetroTurbo LCD:
 *    -------- ---- -- -  -------- ---- -- -  ( × NLCD)
 *    [   0° 8:4:2:1   ]  [   45° 8:4:2:1  ]
 *
 * MobiCom'19 LCD: (8 and 2 is wired connected, 4 and 1 is wired connected, though can only send 16QAM or 4QAM)
 *         A                   B                   C                   D
 * 1  -------- ---- -- -  -------- ---- -- -  -------- ---- -- -  -------- ---- -- -
 *    [   0° 8:4:2:1   ]  [   0° 8:4:2:1   ]  [   0° 8:4:2:1   ]  [   0° 8:4:2:1   ]
 * 2  -------- ---- -- -  -------- ---- -- -  -------- ---- -- -  -------- ---- -- -
 *    [   45° 8:4:2:1  ]  [   45° 8:4:2:1  ]  [   45° 8:4:2:1  ]  [   45° 8:4:2:1  ]
 * 3  -------- ---- -- -  -------- ---- -- -  -------- ---- -- -  -------- ---- -- -
 *    [   0° 8:4:2:1   ]  [   0° 8:4:2:1   ]  [   0° 8:4:2:1   ]  [   0° 8:4:2:1   ]
 * 4  -------- ---- -- -  -------- ---- -- -  -------- ---- -- -  -------- ---- -- -
 *    [   45° 8:4:2:1  ]  [   45° 8:4:2:1  ]  [   45° 8:4:2:1  ]  [   45° 8:4:2:1  ]
 * 
 */
#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int LCD_unit_test(bool is_turbo, const char* port, int LCD_index, uint8_t byte_data);
Tag_Sample_t RetroTurbo_map2_MobiCom19(Tag_Sample_t sample);

int main(int argc, char** argv) {
    HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	if (argc != 4 && argc != 5) {
		printf("usage: <collection> <id(12 byte hex = 24 char)> <origin_key> <converted_key>\n");
		printf("   or: <port> +<LCDindex> <ByteDataHex>  (for RetroTurbo LCD unit test)\n");
		printf("   or: <port> -<LCDindex> <ByteDataHex>  (for RetroTurbo LCD unit test)\n");
        printf("        0 <= LCDindex < 8, if LCDindex==8 then it will start automatic test iterating 0~7\n");
		return -1;
	}

    // unit test for index remapping
    if (argv[2][0] == '+' || argv[2][0] == '-') {
        const char* port = argv[1];
        int LCD_index = atoi(argv[2] + 1);
        assert(strlen(argv[3]) == 2 && "must be hex byte");
        char H = argv[3][0], L = argv[3][1];
#define ASSERT_HEX_VALID_CHAR_TMP(x) assert(((x>='0'&&x<='9')||(x>='a'&&x<='z')||(x>='A'&&x<='Z')) && "invalid char");
		ASSERT_HEX_VALID_CHAR_TMP(H) ASSERT_HEX_VALID_CHAR_TMP(L)
#undef ASSERT_HEX_VALID_CHAR_TMP
#define c2bs(x) (x>='0'&&x<='9'?(x-'0'):((x>='a'&&x<='f')?(x-'a'+10):((x>='A'&&x<='F')?(x-'A'+10):(0))))
		uint8_t byte_data = (c2bs(H) << 4) | c2bs(L);
#undef c2bs
        printf("LCD_index: %d, byte_data: 0x%02X\n", LCD_index, byte_data);
        return LCD_unit_test(argv[2][0] == '+', port, LCD_index, byte_data);
    }

    assert(argc == 5);
	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
	const char* origin_key = argv[3];
	const char* converted_key = argv[4];
    assert(strlen(converted_key) != 0 && "key cannot be empty string");

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

    BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
	assert(record.existed());
	BsonOp origin = record[origin_key];
	assert(origin.existed() && origin.type() == BSON_TYPE_ARRAY);
	vector<string> compressed; int origin_length = origin.count();
	for (int i=0; i<origin_length; ++i) {
		compressed.push_back(origin[i].value<string>());
		// printf("%s\n", compressed.back().c_str());
	}
    vector<pair<Tag_Sample_t, int>> compressed_samples = compressed_string_to_compressed_samples(compressed);
    vector<pair<Tag_Sample_t, int>> compressed_converted;
    for (auto it=compressed_samples.begin(); it!=compressed_samples.end(); ++it) {
        compressed_converted.push_back(make_pair(RetroTurbo_map2_MobiCom19(it->first), it->second));
    }
    vector<string> converted = compressed_samples_to_compressed_string(compressed_converted, 4);  // only four byte

    record[converted_key].remove();
	record[converted_key].build_array();
	record[converted_key].append(converted);

    record.save();
	mongodat.close();
	record.remove();

    return 0;
}

Tag_Sample_t RetroTurbo_map2_MobiCom19(Tag_Sample_t sample) {
    for (int i=8; i<TAG_L4XX_SAMPLE_BYTE; ++i) assert((sample.le(i) == 0 || sample.le(i) == sample.le(i-8)) && "cannot convert higher bits");
    // 0/3: 0x00, 0x00, 0x00, 0x00
    // 1/3: 0x02, 0x08, 0x20, 0x80
    // 2/3: 0x01, 0x04, 0x10, 0x40
    // 3/3: 0x03, 0x0C, 0x30, 0xC0
    // mapping from 8 pieces of RetruTurbo LCD:
    // idx -> (2*(idx/4), 2*(idx/4)+1)
    // only accept 0xHL, where H,L=0,A,5,F
    // map [0,A,5,F] to [0,2,1,3] << ((idx%4)*2)
    Tag_Sample_t ret;
    for (int i=0; i<8; ++i) {
        uint8_t b = sample.le(i);
        int base = 2 * (i/4);
        for (int j=0; j<2; ++j) {
            uint8_t c = 0x0F & (b >> (j*4));
            assert(c==0 || c==10 || c==5 || c==15);
            int idx = base + j;  // MobiCom19 LCD index
            uint8_t val = (c & 0x03) << ((i%4)*2);
            ret.le(idx) |= val;
        }
    }
    return ret;
}

int LCD_unit_test(bool is_turbo, const char* port, int LCD_index, uint8_t byte_data) {
    assert(LCD_index >= 0 && LCD_index <= 8);  // 8 for automatic test iterating 0~7

    TagL4Host_t tag;
	tag.verbose = true;
	tag.open(port);

	vector<Tag_Sample_t> samples;
	Tag_Sample_t zero;
	tag.set_tx_default_sample(zero);  // set default sample

    for (int idx=(LCD_index==8?0:LCD_index); idx<=(LCD_index==8?7:LCD_index); ++idx) {
	    Tag_Sample_t sample;
        if (is_turbo) {  // display directly
            sample.le(idx) = byte_data;
        } else {  // convert then display
            sample.le(idx) = byte_data;
            sample = RetroTurbo_map2_MobiCom19(sample);
            // memset(&sample.s, 0xFF, sizeof(Tag_Sample_t));
        }
        for (int i=0; i<10; ++i) samples.push_back(sample);
    }

    tag.tx_send_samples(10, samples);

	tag.close();
    return 0;
}
