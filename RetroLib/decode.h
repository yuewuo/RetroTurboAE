//
// Created by prwang on 7/18/2019.
//
#pragma once
//todo finally goto lib.dll
#ifdef __cplusplus
extern"C" {
#endif
#include <stdint.h>
struct mod_option{ int p, pw, l, q; };
void mod_init(mod_option a, const char* ref_file_name);
int  simerr(const uint8_t* data, uint8_t* decoded, int size, uint64_t seed, float stdev, int L0);
#ifdef __cplusplus
}

#include <complex>
#include <vector>
using namespace std;
#define LOGM(...) fprintf(stderr, __VA_ARGS__)
using cf = complex<float>;
#include <array>
#include <atomic>
#include <bitset>
#include <cfloat>
#include <chrono>
#include <ext/random>
#include <fstream>
#include <memory>
#define EIGEN_DONT_PARALLELIZE
#define EIGEN_STACK_ALLOCATION_LIMIT 0
//#define EIGEN_NO_DEBUG
#include <Eigen/Core>
#include <Eigen/QR>
#include <Eigen/SVD>

using namespace std;
using namespace Eigen; //this file use only column major matrix
#define LOGM(...) fprintf(stderr, __VA_ARGS__)
struct abstract_dsm_modem {
	abstract_dsm_modem() = default;
	virtual ~abstract_dsm_modem() = default;
	virtual vector<uint8_t> viterbi(const cf* data, int len, int L0) const = 0 ;
	virtual vector<cf> sim(const uint8_t* __data, int size) const  = 0;
};
static inline constexpr uint64_t mask(int B, int spacing = 1)
{ uint64_t r = 0; for (int i = 0; i < B; ++i) r = r << spacing | 1; return r; }
template<int L> static constexpr inline  uint64_t rol(uint64_t x, int i) { return (x << i | (x >> (L - i) & mask(i))) & mask(L); }
template<int L, int B>
struct bitset1 {
	static_assert(B > 0 && L > 0);
	static constexpr int nb = (L * B + 63)/ 64;
	uint64_t data[nb];
	bitset1& operator+=(const uint64_t newseg) {
		for (int i = nb - 1; i >= 0; --i) {
			data[i] = data[i] << B | (i ? data[i - 1] >> (64 - B) : newseg) & mask(B);
		}
		return *this;
	}
	bitset1 operator+(const uint64_t seg) const {
		bitset1 ret = *this;
		return ret += seg;
	}
	uint64_t operator[](unsigned i) const {
		unsigned ii = i * B;
		return ((64 - B < ii % 64 ? data[ii / 64 + 1] << 64 - (ii % 64) : 0)
					 | data[ii / 64] >> ii % 64) & mask(B);
	}
	bitset1() : data{} { }
	constexpr static bitset1 Zero() { return bitset1(); }
	uint64_t get(int L0 = L) const { return data[0] & mask(L0 * B); }
	bitset1<B, L> transpose() const {
		bitset1<B, L> ret = bitset1<B, L>::Zero();
		for (int i = 0; i < L; ++i) {
			for (int j = 0; j < B; ++j) {
				int s = i * B + j, d = j * L + i;
				ret.data[d / 64] |= (data[s / 64] >> s % 64 & 1) << d % 64;
			}
		}
		return ret;
	}

};


template<int B>
struct bitio {
	uint8_t* dat; int cbit;
	explicit bitio(uint8_t* _dat) : dat(_dat), cbit(0){ }
	uint64_t swap(uint64_t x = 0) {
		int k = cbit >> 3, l = cbit & 7;
		uint64_t rs = 0;
		for (int i = 0; i < B; ++i) {
			int bit = dat[k] >> l & 1, xbit = x >> i & 1;
			rs |= bit << i;
			dat[k] ^= (bit ^ xbit) << l;
			if (++l == 8) { l = 0; k++; }
		}
		cbit = k << 3 | l;
		return rs;
	}
};
template<class T> void writef(T x, const char* f) {
	(ofstream((f), ios::binary)).write((const char*)(x).data(), x.size() * sizeof(x[0]));
};

template<int P = 2, int T = 20, int L = 16,  int Q = 2, int K = 8, int S = 4, int V = 3>
struct dsm_modem : abstract_dsm_modem {
	vector<uint8_t> viterbi(const cf* data, int len, int L0) const override {
		bitset1<V, P * Q> history[L];   // array index cur := from cur, update cur + 1
		using path_t = bitset1<L, P * Q>;
		pair<float, path_t> f[2][1uLL << L0 * P * Q], global_metric[1ULL << P * Q];
		seg_t J_all[L][1 << P * Q];
		//channel training
		dref_t h =  (H * ct_solver.solve(fil_get<P * L>(data).reshaped(K * P * L, 1)).reshaped(S, P * L)).reshaped(K, P * L << V);
		h = h0;//todo
		//writef((Phi*h).eval(), "h_est.b");
		data += ((P * L + 4) * PW);  // ...
		assert(len % (P * Q * L) == 0);
		fill(J_all[0], J_all[L], seg_t::Zero());
		fill(history, history + L, bitset1<V, P * Q>::Zero());
		for (uint64_t u = 0; u < (1ULL << L0 * P * Q); ++u) { f[0][u].first = -FLT_MAX; }


		//vector<cf> out(PW * (4 + P * L + n + 1) - T); cf *pout = out.data();
		float d0 = Map<const Matrix<cf, Dynamic, 1>>(data, PW * (len / (L * P * Q) + 2) - 2 * T).squaredNorm();
		f[0][0] = make_pair(-d0, path_t::Zero());
		int n1 = len / (P * Q);
		vector<uint8_t> ret((len + 7) / 8);
		bitio<P * Q> bo(ret.data());
		for (int cur = 0; cur < n1 + L - 1; ++cur) {
			auto y = fil_get(data); data += T;
			assert(y.norm() < 1000);
			float o_m = -FLT_MAX; int o_sym = -1;
			const int l_now = (cur + 1) % L;
			auto const& hl = h.template block<K, P << V>(0, l_now * P << V);
			auto& r = f[cur & 1], &w = f[~cur  & 1];
			for (uint64_t u = 0; u < (1ULL << L0 * P * Q); ++u) { w[u].first = -FLT_MAX; }
			for (int symbol = 0; symbol < (cur < n1 ? 1 << P * Q : 1); ++symbol) {
				//region preprocess m1, m2
				auto const& J = J_all[l_now][symbol] = getJ((history[l_now] + symbol).transpose(), hl);
				float m2[L][1 << P * Q], m1;
				m1 =  (J.adjoint() * y).value().real() * 2 - (J.adjoint() * cross[0] * J).value().real();
				//LOGM("[cur=%d,symbol=%d]m1=%f, jnorm=%f, ynorm=%f\n", cur,symbol,m1, J.norm(), y.norm());
				for (int i = 1; i < L; ++i) {
					auto jxi = (J.adjoint() * cross[i]).eval();
					for (uint64_t j = 0; j < (1ull << P * Q); ++j) {
						m2[i][j] = -2 * (jxi * J_all[(l_now + L - i) % L][j]).value().real();
					}
				} //endregion
				//region update for each candidate
				assert((1ULL << L0 * P * Q) == 256);
				for (uint64_t u = 0; u < (1ULL << L0 * P * Q); ++u) {
					float m = r[u].first + m1;
					path_t v = r[u].second + symbol;
					for (int i = 1; i < L; ++i) {
						m += m2[i][v[i]];
					}
					auto& tar = w[v.get(L0)];
					if (tar.first < m) {
						tar = make_pair(m, v);
						LOGM("[%08x] %08x -> %08x (symbol = %02x), val %f+%f=%f\n", cur,  u, v.get(L0), symbol, r[u].first, m - r[u].first, m);
					}
					// L - 1 : cur - L + 2
					if (o_m < m) { o_m = m; o_sym = v[L - 1]; }
				}
				//endregion
			}
			LOGM("\n[%08x] o_sym = %02x, o_m = %f\n", cur, o_sym, o_m);
			if (cur >= L - 1) {
				history[(cur + 1) % L] += o_sym;
				bo.swap(degray(o_sym));
			}
		}
		return ret;
	}
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	//region matched_filter
	constexpr static int PW = L * T;
	Matrix<cf, PW, K> Phi;
	array<Matrix<cf, K, K>, L> cross;
	template<int n = 1> Matrix<cf, K, n> fil_get(const cf* data) const
	{ return Phi.adjoint() * Map<const Matrix<cf, PW, n> >(data); }
	//endregion

	//region discrete_refs
	static_assert(P == 1 || P == 2);
	Matrix<cf, K << V, S> H; // K, 1<<V, S
	FullPivHouseholderQR<Matrix<cf, K * P * L, S * P * L>> ct_solver;

	static constexpr uint64_t ct =
					P * L == 32 ?  0b10111110001110110011010010100000 :
					P * L == 16 ?  0b1111011001010000 :
					P * L == 8 ?  0b11101000: 1;
	auto ct_wfm () const {
		Matrix<cf, Dynamic, S * P * L> co(K * P * L, S * P * L);
		array<bitset1<V, 1>, P * L> history{};
		for (int i = 0; i < P * L; ++i) { for (int j = 0; j < P * L; ++j) {
				auto h = (history[j] += ct >> (i + j) % (P * L) & 1).get();
				co.template block<K, S>(i * K, j * S) = H.template block<K, S>(h * K, 0);
			} } return co;
	}

	constexpr static auto getJ(const bitset1<P * Q, V>& his, const Matrix<cf, K, P << V>& h ) {
		seg_t J = seg_t::Zero();
		for (int i = 0; i < Q; ++i) { //traces_T(V, Q, P) //h.col(1<<V, P, L)
			float gain = (float(1 << i) / ((1 << Q) - 1));
			for (int j = 0; j < P; ++j) {
				int idx = j << V | (his[j * Q + i]);
				assert(idx >= 0 && idx < (P << V));
				J += h.col(j << V | (his[j * Q + i])) * gain;
				assert(J.norm() < 1000);
			}
		}
		assert(J.norm() < 1000);
		return J;
	}


	using dref_t = Matrix<cf, K, P * L << V>; // K, 1<<V, P, L
	// endregion

	dref_t h0;

	explicit dsm_modem(const vector<cf>& __ref) {
		assert(__ref.size() % (PW << V) == 0);
		int n = __ref.size() / (PW << V);
		auto ref =  Map<const MatrixXcf>(__ref.data(), PW, n << V);
		BDCSVD<MatrixXcf> svd1(ref, ComputeThinU | ComputeThinV);
		// result: U = (T * L)  * (K + unused) .., V = (K)* (dynamic << V)
		Phi = svd1.matrixU().leftCols(K);
		for (int i = 0; i < L; ++i) { cross[i] = Phi.topRows(PW - i * T).adjoint() * Phi.bottomRows(PW - i * T); }
		auto r22 = DiagonalMatrix<cf, K>(svd1.singularValues().head(K)) * svd1.matrixV().leftCols(K).adjoint();

		LOGM("\n");

		BDCSVD<MatrixXcf> svd2(r22.reshaped(K << V, n), ComputeThinU);
		H = svd2.matrixU().leftCols(S);
		ct_solver.compute(ct_wfm());
		if (n == 32) {
			LOGM("special ref 32\n");
			assert(32 % (L * P) == 0);
			assert(P == 2);
			h0 = r22/*P = 1 not supported. .reshaped(2 / P * L << V, 16 * P).topRows(L << V)*/
							.reshaped(K * L * P << V, 16 * P / (L * P)).rowwise().sum().reshaped(K, L * P << V);
			writef((Phi * h0).eval(), "h0_refs.b");
		} else h0 = decltype(h0)::Constant(NAN);

	}
	vector<cf> sim(const uint8_t* __data, int size) const  override {
		assert(size && size * 8 % (L * P * Q) == 0); int n = size * 8 / (L * P * Q);
		vector<uint8_t> _data (__data, __data + size); bitio<P * Q> bi(_data.data());

		array<bitset1<V, 1>, P * L> history_ct{};
		using outmap_t = Map<Matrix<cf, PW, 1>>;
		vector<cf> out(PW * (4 + P * L + n + 2) - 2 * T); cf *pout = out.data();
		bitset1<V, P * Q> history[L]{};
		for (int i = 0; i < P * L; ++i) { for (int j = 0; j < P * L; ++j) {//requires L outside P
				auto h = (history_ct[j] += ct >> (i + j) % (P * L) & 1).get();
				outmap_t(pout + i * PW) += Phi * h0.col(j << V | h);
			} }
		pout += PW * (P * L + 4);
		for (int p = 0;  p < n; ++p) {
			for (int i = 0; i < L; ++i) {
				auto const& hl = h0.template block<K, P << V>(0, i * P << V);
				outmap_t(&pout[p * PW + i * T]) += Phi * getJ((history[i] += gray(bi.swap())).transpose(), hl);
			}
		}
		for (int i = 0; i < L - 1; ++i) {
			auto const& hl = h0.template block<K, P << V>(0, i * P << V);
			outmap_t(&pout[n * PW + i * T]) += Phi * getJ((history[i] + 0).transpose(), hl);
		}
		return out;
	}


	constexpr static typename enable_if<P == 2, uint8_t>::type sample8(int symb) {
		constexpr int bps = P * Q;
		//assert(symb >= 0 && symb < mo && "Symbol must in range [0..mo-1]");
		constexpr  uint8_t wgt[4][8] = {{ 0xf0, 0x0f,0, 0, 0, 0, 0, 0},
																		{ 0xa0, 0x50, 0x0a, 0x05,0, 0, 0, 0},
																		{0x40, 0x20, 0x10, 0x04, 0x02, 0x01, 0, 0},
																		{ 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01}};
		const uint8_t *iwg = wgt[bps / 2 - 1];
		char send = 0;
		for (int i = 0; i < bps; ++i) {
			send += uint8_t(iwg[i] * (symb >> i & 1));
		} return send;
	}
	static_assert(T % 10 == 0); static constexpr int tagT = T / 10, tagPW = PW / 10;
	vector<array<uint8_t, 16>> encoder(const uint8_t* __data, int size,  int tagD) {
		assert(size && size * 8 % (L * P * Q) == 0); int n = size * 8 / (L * P * Q);
		vector<uint8_t> _data (__data, __data + size); bitio<P * Q> bi(_data.data());

		vector<array<uint8_t, 16>> ret(tagPW * (L * P + 4 + n - 1) - tagT);
		fill(&ret[0][0], &ret[ret.size()][0], 0);

		array<uint8_t, 16>* pout = &ret[0];
		assert(P == 2);
		//channal training. should use same Q(concerning 8/64 order, not full brightness)
		for (int t = 0; t < P * L; ++t) {
			assert(16 % L == 0);
			for (int jj = 0; jj < L; jj++) {
				uint8_t s1 = ct >> ((t + 2 * jj) % (P * L)) & 3,
								s2 = sample8(((s1 >> 1 & 1) << Q | (s1 & 1)) * mask(Q));
				for (int j = jj; j < 16;  j += L) for (int k = 0; k < tagD; ++k)
						pout[k + t * tagPW][j] = s2;
			}
		}
		pout += tagPW * (4 + P * L);
		//payload
		for (int t  = 0; t < n; ++t) {
			for (int i = 0; i < L; ++i) {
				uint8_t s2 = sample8(gray(bi.swap()));
				for (int j = i; j < 16; j += L) for (int k = 0; k < tagD; ++k)
						pout[k + i * tagT + t * tagPW][j] = s2;
			}
		} return ret;
	}
private:
	//offsets = { .training_start = 0, .training_length = P * L // synchronized. .data_start = P * L + 4, };
public:
	static constexpr uint64_t gray(uint64_t x) { return (x & ~mask(P, Q)) >> 1 ^ x; }
	static constexpr uint64_t degray(uint64_t x) { uint64_t y = x; while ((x = (x & ~mask(P, Q)) >> 1)) y ^= x; return y; }

	using seg_t = Matrix<cf, K, 1>;

	~dsm_modem() override = default;
};

extern abstract_dsm_modem* mo;
template<class T> static vector<T> read_all(const char* filename) {
	ifstream ifs(filename, ios::binary);
	ifs.seekg(0, ios_base::end);
	int sz = ifs.tellg();
	assert(sz % sizeof(T) == 0 && "File size should be a multiple of type size");
	vector<T> ret; ret.resize(sz / sizeof(T));
	ifs.seekg(0, ios_base::beg);
	ifs.read((char*)ret.data(), sz);
	return ret;
}
#endif


