#include <rpc_server.h>
#include <rpc_client.hpp>
#define LOGM(...) fprintf(stderr, __VA_ARGS__)
#include <thread>
#include <chrono>
using namespace rest_rpc;
using namespace std;
using namespace std::chrono_literals;
using namespace rpc_service;
#include <random>
#include <string>
#include <decode.h>
using namespace Eigen;
struct ctd {
	int PL, PW, V, S; uint64_t ct;
	MatrixXcf H; // PW << V, S
	FullPivHouseholderQR<MatrixXcf> ct_solver;
	ctd (const MatrixXcf& ref, int _PL, int _PW, int _V, int _S)
					: PL(_PL), PW(_PW), V(_V), S(_S) { //
		ct = PL == 32 ?  0b10111110001110110011010010100000 :
				 PL == 16 ?  0b1111011001010000 :
				 PL == 8 ?  0b11101000: 1;
		H = (BDCSVD<MatrixXcf> (ref, ComputeThinU)).matrixU().leftCols(S);
		MatrixXcf co(PW * PL, S *  PL + 1);
		unsigned history[PL]; fill(history, history + PL, 0);
		for (int i = 0; i < PL; ++i) { for (int j = 0; j < PL; ++j) {
				unsigned  h = history[j] = (history[j] << 1 | (ct >> (i + j) % PL & 1)) & ((1 << V) - 1);
				co.block(i * PW, j * S, PW, S) = H.block(h * PW, 0, PW, S);
			} }
		co.block(0, S * PL, PW*PL,1).setConstant(1);
		ct_solver.compute(co);
	}
	/* channel training takes the waveform of training pattern to solve the corresponding ref and dc offset
	 *  A:  recorded channel training waveform
	 * return: pair(concatenated refs(PL * PW * (1<<V) ), dc offset)
	 * data in float always come in pairs (meant to be complex<float> but MsgPack does not support it.)
	 * */
	tuple<MatrixXcf, cf> channel_training(const vector<cf>& A) const {
		LOGM("rpc request with A.size() == %lu\n", A.size());
		assert(A.size() ==  PL * PW);
		LOGM("assert passed 1\n");
		auto s = ct_solver.solve(Map<const VectorXcf>((cf*)A.data(), PL * PW)).eval();
		assert(s.size() == PL * S + 1);
		LOGM("assert passed 2, H.rows() = %lu, H.cols() = %lu\n", H.rows(), H.cols());
		MatrixXcf mtr = (H * s.head(S * PL).reshaped(S,  PL)).eval(); // this statement causes bad_alloc??
		LOGM("assert will pass 3 mtr.rows() = %lu, mtr.cols() = %lu\n", mtr.rows(), mtr.cols());
		assert(mtr.size() == PW * PL << V);
		LOGM("assert passed 3\n");
		auto dc = s[PL * S];
		LOGM("rpc request will return dc = %.3f + %.3fj\n", real(dc), imag(dc));
		return make_tuple(mtr, dc);
	}
};


int main(int argc, char** argv) {
	if (argc != 7 && argc != 8) {
		printf("Usage ctd <port> <ref_file> <PL> <PW> <V> <S> <NLCD:16>\n");
		exit(3);
	}
	int PL = atoi(argv[3]), PW = atoi(argv[4]), V = atoi(argv[5]), S = atoi(argv[6]) ;
	int NLCD = argc >= 8 ? atoi(argv[7]) : 16;
	auto __ref = read_all<cf>(argv[2]); assert(__ref.size() % (PW << V) == 0); int n = __ref.size() / (PW << V);
	auto ref =  Map<const MatrixXcf>(__ref.data(), PW << V, n);

#ifdef SERVER
	ctd d(ref, PL, PW, V, S);
	rpc_server server(atoi(argv[1]), 1);
	server.register_handler("channel_training", [&d, PW, PL, V, NLCD](const rpc_conn&, const vector<float>& A) {
#define NV(T, v) vector<T>((T*)v.data(), (T*)(v.data() + v.size()))
		auto [c, dc] = d.channel_training(NV(cf, A));
		assert(V == 3 && "6412537");
		MatrixXcf 
			c0 = c.reshaped(PW << V, PL).transpose().eval(), // PL, PW, 1<<V
			c1 = c0.reshaped(PL * PW, 1<<V)(all, {6, 4, 1, 2, 5, 3, 7}).reshaped(PL, PW * 7), //PL, PW * 7
			c2 = (c1.transpose().eval() / (NLCD * 2 / PL)).replicate(1, NLCD * 2 / PL).eval();
		return make_tuple(NV(float, c2), real(dc), imag(dc));
		}) ;
	server.run();
#else
	//demo.
	assert(n == 32); LOGM("special ref 32\n");
	vector<cf> out(PW * PL, cf(0));
	cf *pout = out.data();
	assert(32 % PL == 0);
	MatrixXcf h0 = ref.reshaped(PW * PL << V, n / PL).rowwise().sum().reshaped(PW, PL << V); //combined ref
	writef(h0, "h0_refs.b");
	unsigned history[PL]; fill(history, history + PL, 0);
	//generate channel training pattern (cf decode.h/struct dsm_modem::encoder())
	unsigned ct = PL == 32 ?  0b10111110001110110011010010100000 :
		PL == 16 ?  0b1111011001010000 : PL == 8 ?  0b11101000: 1;
	for (int i = 0; i < PL; ++i) { for (int j = 0; j < PL; ++j) {
		unsigned  h = history[j] = (history[j] << 1 | (ct >> (i + j) % PL & 1)) & ((1 << V) - 1);
		Map<VectorXcf>(pout + i * PW, PW) +=  h0.col(j << V | h);
	} }

	//test with dc offset and noise
	__gnu_cxx::sfmt607_64 rng(233);
	normal_distribution<float> noise(0, 0.1);
	for (int i = 0; i < out.size(); ++i) out[i] += cf(1 + noise(rng), 2 + noise(rng));
	writef(out, "training_pattern.b");

	//do rpc, equivalant to auto [h0_recovered, dcre, dcim] = ctd(ref, PL, PW, V, S).channel_training(outf);
	vector<float> outf((float*)out.data(), (float*)(out.data() + out.size()));
#if 1
	rpc_client c("127.0.0.1", atoi(argv[1])); assert(c.connect() && "connection timeout");
	LOGM("before rpc call\n");
	auto [h0_recovered,dcre,dcim] = c.call<tuple<vector<float>, float, float>>("channel_training", outf);
#else 
	//local test
	auto [h0_recovered,dcre,dcim] = ctd(ref, PL, PW, V, S).channel_training(outf);

#endif 

	LOGM("after rpc call\n");
	writef(h0_recovered, "h0_recovered.b");
	printf("got dc %.3f + %.3fj\n", dcre, dcim);
#endif
}
