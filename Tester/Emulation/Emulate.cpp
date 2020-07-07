#include "emulator.h"
#define ALLOW_DATABASE_REF  // comment this out to disable database reference fetch, to avoid any dependencies of libraries
#ifdef ALLOW_DATABASE_REF
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;
#endif
#include <regex>
#include <fstream>
using namespace std;

FastDSM_Encoder encoder;
FastDSM_Emulator emulator;

int main(int argc, char** argv) {
#ifdef ALLOW_DATABASE_REF
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);
#endif

    if (argc != 8) {
        printf("usage: <refs:[filename in the format \"turborefs_%%d_%%d_%%d_%%d_%%d.anything.else\" %% (NLCD, duty, cycle, frequency, effect_length)]|[database id(12 byte hex = 24 char)]> ");
        printf("<ct_fast:int> <ct_slow:int> <combine:int> <bit_per_symbol:int> <bias:int> <data:hex>\n");
        printf("\nbias is the same as \"Tester/ExploreLCD/EL_General_Repeat_Record_Get.cpp\", because preamble usually have slight disalignment which needs to adjust manually\n");
        printf("in this case, the emulator will \"emulate\" this bias and return exactly the same time alignment as real experiment, so you can decode the emulated signal the same way as real experiment ones\n");
        return -1;
    }

    const char* refs_cmd = argv[1];
    encoder.ct_fast = atoi(argv[2]);
    encoder.ct_slow = atoi(argv[3]);
    emulator.combine = encoder.combine = atoi(argv[4]);
    encoder.bit_per_symbol = atoi(argv[5]);
    emulator.bias = atoi(argv[6]);
    const char* hex_data = argv[7];

    if (refs_cmd[0] == 't') {  // should be a valid refs filename
        string filename = refs_cmd;
        smatch sm;
        regex r("^turborefs_([1-9]\\d*)_([1-9]\\d*)_([1-9]\\d*)_([1-9]\\d*)_([1-9]\\d*)\\..*$");
        if (regex_search(filename, sm, r)) {
            assert(sm.size() == 6 && "strange regex match size");
            emulator.NLCD = atoi(sm.str(1).c_str());
            emulator.duty = atoi(sm.str(2).c_str());
            emulator.cycle = atoi(sm.str(3).c_str());
            emulator.frequency = atoi(sm.str(4).c_str());
            emulator.effect_length = atoi(sm.str(5).c_str());
            ifstream input(filename, ios::binary);
            assert(input && "file not exists, check your filename");
	        vector<unsigned char> binary(istreambuf_iterator<char>(input), {});
            int data_length = binary.size() / sizeof(complex<float>);
            emulator.refs.resize(data_length);
            memcpy((void*)emulator.refs.data(), binary.data(), binary.size());  // save the reference in the emulator
        } else {
            printf("please check the format. more information please run this program without any parameters, see the usage printed\n");
            return -3;
        }
    } else {
#ifdef ALLOW_DATABASE_REF
        assert(MongoDat::isPossibleOID(refs_cmd) && "data_id invalid");
        bson_oid_t data_id = MongoDat::parseOID(refs_cmd);
        vector<char> binary = mongodat.get_binary_file(data_id);
        int data_length = binary.size() / sizeof(complex<float>);
        emulator.refs.resize(data_length);
        memcpy((void*)emulator.refs.data(), binary.data(), binary.size());  // save the reference in the emulator
	    bson_t* metadata_ = mongodat.get_metadata_record(data_id);
        BsonOp metadata(&metadata_);
        assert(metadata["NLCD"].existed() && metadata["NLCD"].type() == BSON_TYPE_INT32); emulator.NLCD = metadata["NLCD"].value<int32_t>();
        assert(metadata["duty"].existed() && metadata["duty"].type() == BSON_TYPE_INT32); emulator.duty = metadata["duty"].value<int32_t>();
        assert(metadata["cycle"].existed() && metadata["cycle"].type() == BSON_TYPE_INT32); emulator.cycle = metadata["cycle"].value<int32_t>();
        assert(metadata["effect_length"].existed() && metadata["effect_length"].type() == BSON_TYPE_INT32); emulator.effect_length = metadata["effect_length"].value<int32_t>();
        assert(metadata["frequency"].existed() && metadata["frequency"].type() == BSON_TYPE_DOUBLE); emulator.frequency = metadata["frequency"].value<double>();
#else
        printf("database reference fetch is not enabled, thus cannot accept database id input\n");
        printf("if you exactly aim at filename input, please check the format. more information please run this program without any parameters, see the usage printed\n");
        return -2;
#endif
    }

    // output the basic parameters for easy debug purpose
    printf("NLCD: %d\n", encoder.NLCD = emulator.NLCD);
    printf("duty: %d\n", encoder.duty = emulator.duty);
    printf("cycle: %d\n", encoder.cycle = emulator.cycle);
    printf("frequency: %f\n", encoder.frequency = emulator.frequency);
    printf("effect_length: %d\n", emulator.effect_length);
    printf("combine: %d\n", emulator.combine);
    emulator.sanity_check();  // this will check the size of reference file

    // read the data to encode
    vector<uint8_t> data = FastDSM_Emulator::get_bytes_from_hex_string(hex_data);
#if defined(ALLOW_DATABASE_REF) && 0
    printf("data: %s\n", MongoDat::dump(data).c_str());
    vector< complex<float> > test_scatter;  // this is used to test scatter plotting on WebGUI
    double mean = 0;
    double stddev = 0.05;
    auto dist = bind(std::normal_distribution<double>{mean, stddev}, mt19937(random_device{}()));
    for (int i=0; i<8; ++i) {
        for (int j=0; j<8; ++j) {
            for (int k=0; k<100; ++k) {
                test_scatter.push_back(complex<float>(i + dist(), j + dist()));
            }
        }
    }
    mongodat.upload_scatter("test_scatter", (float*)test_scatter.data(), test_scatter.size(), NULL, "test_scatter", 1, "x", 1, "y");
    printf("upload test_scatter file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
#endif

    // compute the bias of emulation (basically preamble and channel training)
    encoder.build_preamble();
    encoder.build_channel_training();
    emulator.bias += (encoder.o_preamble.size() + encoder.o_channel_training.size()) * emulator.orate / emulator.frequency;
    printf("bias: %d\n", emulator.bias);

    encoder.compute_o_parameters();
    vector<Tag_Single_t> singles = FastDSM_Encoder::PQAMencode(data.data(), data.size(), encoder.bit_per_symbol);
// start test
    // vector<Tag_Single_t> singles;
    // for (int i=0; i<4; ++i) {  // test single LCD
    //     singles.push_back(0x0F);
    //     singles.insert(singles.end(), 15, 0x00);
    // }
    // for (int i=0; i<4; ++i) {  // test two set of LCD
    //     singles.push_back(0x0F);
    //     singles.insert(singles.end(), 7, 0x00);
    // }
    // singles.insert(singles.end(), 16, 0x00);
    // for (int i=0; i<0x0F; ++i) {
    //     singles.push_back(i);
    //     singles.insert(singles.end(), 15, 0x00);
    // }
// end test
    vector< complex<float> > emulated = emulator.emulate(singles);
#ifdef ALLOW_DATABASE_REF
    mongodat.upload_record("emulated", (float*)emulated.data(), 2, emulated.size(), NULL, "emulated", 1/80., "time(ms)", 1, "I,Q");
    printf("upload emulated file with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    printf("you can open RetroTurbo WebGUI and copy the following line, click run to see the waveform:\n");
    printf("rt.reader.plot_rx_data(\"%s\")\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
#endif
    char o_filename[64];
    sprintf(o_filename, "emulated_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%s.bin"
        , emulator.NLCD, emulator.duty, emulator.cycle, (int)emulator.frequency, emulator.effect_length
        , encoder.ct_fast, encoder.ct_slow, encoder.combine, encoder.bit_per_symbol, emulator.bias, hex_data);
    printf("file saved as %s\n", o_filename);
    ofstream output(o_filename, ios::binary);
    assert(o_filename && "cannot open file to write");
    output.write((const char*)emulated.data(), emulated.size() * sizeof(complex<float>));
	output.close();

    return 0;
}
