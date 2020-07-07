#define DLL
#include <bitset>
#include "rtm.h"
#include <reader-H7xx-ex.h>
#include <cstring>
#include <string>
#include <random>
#include <rpc_client.hpp>
using namespace rest_rpc;
using namespace rest_rpc::rpc_service;
using namespace std;
#include "tag-L4xx-ex.h"
ofstream db = ofstream("db.raw", ofstream::binary);
struct tag_sample1 { array<char, TAG_L4XX_SAMPLE_BYTE> s; MSGPACK_DEFINE(s); };
DLLEXPORT int seed(int idx) {
	int ret;
	if (idx <= 0) {
		asm volatile("rdseed %0":"=r"(ret));
	} else {
		mt19937 rn(233);
		while (idx --> 0) {
      ret = rn();
		}
	}
	return ret;
}
ReaderH7Host_t reader;
DLLEXPORT __attribute__((destructor)) void deinit()
{
	LOGM("finalize called\n");
	if (reader.com) reader.close();
	LOGM("reader close done\n");
}
DLLEXPORT  void init(const char* reader_port,
				const char* ref_filename,
				const char* debug_log)
{
	if (debug_log) {
		freopen(debug_log, "w", stderr);
	}
	setvbuf(stderr, NULL, _IONBF, 0);
	LOGM("init() called with reader port %s, ref_filename %s\n",
			 reader_port, ref_filename);
	reader.open(reader_port);
	reader.load_refpreamble(ref_filename);
	gain_reset();
}
DLLEXPORT float gain_reset() {
	return reader.gain_ctrl(0.2);
}

static inline tag_sample1 ts_populate(char c) {
	tag_sample1 ret;
  memset(&ret.s[0], c, sizeof(tag_sample1));
	return ret;
}
static inline tag_sample1 en_sample(int bps, int symb_gray) {
	assert(bps % 2 == 0 && bps >= 2 && bps <= 8); //QPSK, 16QAM  64QAM, 256QAM
  int mo = 1 << bps;
	assert(symb_gray >= 0 && symb_gray < mo && "Symbol must in range [0..mo-1]");
#define REVERSE_IQ
#ifdef REVERSE_IQ
	unsigned char wgt[4][8] = {{ 0xf0, 0x0f,0, 0, 0, 0, 0, 0},
									{ 0xa0, 0x50, 0x0a, 0x05,0, 0, 0, 0},
									{0x40, 0x20, 0x10, 0x04, 0x02, 0x01, 0, 0},
									{ 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01}};
#else
	unsigned char wgt[4][8] = {{0x0f, 0xf0, 0, 0, 0, 0, 0, 0},
									{0x0a, 0x05, 0xa0, 0x50, 0, 0, 0, 0},
									{0x04, 0x02, 0x01, 0x40, 0x20, 0x10, 0, 0},
									{0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10}};
#endif
  const unsigned char *iwg = wgt[bps / 2 - 1];
  char send = 0;
  for (int i = 0; i < bps; ++i) {
		send += iwg[i] * (symb_gray >> i & 1);
	}
	return ts_populate(send); //todo
}
vector<tag_sample1> ts;
DLLEXPORT void tag_samp_append(int bps, int n_symb, const int *symb_gray, int period, int duty)
{
	LOGM("ts_append called with bps=%d, nsym=%d, symb=%p, period=%d, duty=%d\n{", bps, n_symb, symb_gray, period, duty);
	for (int i = 0; i < n_symb; ++i) {
		LOGM("%d, ", symb_gray[i]);
	} LOGM("}\n");

	assert(duty > 0 && period >= duty);
  tag_sample1 zero; memset(&zero.s[0], 0, sizeof(zero));
	for (int i = 0; i < n_symb; ++i) {
		auto x = en_sample(bps, symb_gray[i]);
    for (int j = 0; j < duty; ++j) { ts.push_back(x);}
    for (int j = duty; j < period; ++j) { ts.push_back(zero); };
	}
}

const int rxs_tol = 2000, time_tol = 2, txsr = 8e3, rxsr = 8e4;
DLLEXPORT int rx_samp2recv() {
	return ts.size() * (rxsr / txsr) + rxs_tol;
}
#if 0
DLLEXPORT void offline_rx(const char* filename,
				float snrthres, float* buffer) {
  ifstream dbf(filename, ios::binary);
	int samp2recv = rx_samp2recv();
	static float buf1[1<<20];
	reader.snr_preamble = snrthres;
	bool preamble_running;
	preamble_rx(samp2recv,
					(float*)buffer, &reader.snr_preamble, reader.refpreamble.size(),
					(float*)reader.refpreamble.data(), [&](size_t requested_4float)->float* {
						dbf.read((char*)buf1, (requested_4float * 4) * sizeof(float));
            return buf1;
					}, 1<<18, &preamble_running);
}
#endif

DLLEXPORT void test_tag()
{
	rpc_client tag("127.0.0.1", 9000);
	assert(tag.connect() && "connection timeout");
	tag.call<void>("tag_send", txsr, ts);
}
DLLEXPORT void car_move(float x, float y, float r) {
	rpc_client dev("127.0.0.1", 9000);
	assert(dev.connect() && "connection timeout");
	dev.call<void>("car_move", x, y, r);
}
DLLEXPORT void car_adjust_once() {
	rpc_client dev("127.0.0.1", 9000);
	assert(dev.connect() && "connection timeout");
	dev.call<void>("car_adjust_once");
}
DLLEXPORT float channel(float snrthres, float *buffer) {
	rpc_client tag("127.0.0.1", 9000);
	assert(tag.connect() && "connection timeout");
	int samp2recv = rx_samp2recv();
	auto reader_future = reader.start_preamble_receiving(samp2recv, snrthres, buffer);
	LOGM("reader is listening to preamble\n");
	auto f = tag.async_call<FUTURE>("tag_send", txsr, ts);
	auto ret =  reader_future.get();
	reader.stop_rx_receiving();
  f.wait();
  ts.clear(); return ret;
}
queue<future<rest_rpc::req_result> > result_queue;
rpc_client *calc = nullptr; //20ms protocol overhead
DLLEXPORT void  enque_calc() {
	if (calc == nullptr) {
    calc = new rpc_client("127.0.0.1", 7000);
		assert(calc->connect() && "connection timeout");
	}
	result_queue.push(calc->async_call<FUTURE>("test_rand"));
	bitset<2000> b;
	auto y = b << 3 | decltype(b)(55);

}
DLLEXPORT int deque_calc()  {
	auto x = std::move(result_queue.front());
	result_queue.pop();

	return x.get().as<int>();
}


#include <decode.h>
//
#pragma clang diagnostic ignored "-Warray-bounds"
abstract_dsm_modem* mo = nullptr;
DLLEXPORT int  simerr(const uint8_t* data, uint8_t* decoded, int size, uint64_t seed, float stdev, int L0) {
	auto wfm = mo->sim(data, size);
	__gnu_cxx::sfmt607_64 rng(seed); //seed must use another type of rng
	normal_distribution<float> noise(0, stdev / sqrt(2));

	writef(wfm, "simulated.b");
	transform(wfm.begin(), wfm.end(), wfm.begin(),
						[&noise, &rng](cf x) { return x + cf{noise(rng), noise(rng)}; });
	auto result = mo->viterbi(wfm.data(), size * 8, L0);
	if (decoded) copy(result.begin(), result.end(), decoded);
	int be = 0;
	for (int i = 0; i < size; ++i) {
		be += __builtin_popcount(data[i] ^ result[i]);
	} return be;

}
DLLEXPORT  void mod_init(mod_option a, const char* ref_file_name) {
	auto ref1 = read_all<cf>(ref_file_name);
	if (mo) delete mo, mo = nullptr;
#define D(P,PW,L,Q) do {if (a.p == P && a.pw == PW && a.l == L && a.q == Q) { mo = new dsm_modem<P,PW/L,L,Q>(ref1);  return; }}while(0)
	assert(0 && "unsupported mod_option");
}

