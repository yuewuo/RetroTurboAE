
//
// Created by prwang on 2/14/2019.

#include "preamble.h"

#include <iostream>
#include <Eigen/Core>
#include <Eigen/Eigenvalues>
#include <Eigen/LU>
#include <soxr/soxr.h>

#include <Eigen/Cholesky>
#include <pffft/pffft.h>

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <list>
#include <fstream>
#include <cassert>
#include <deque>
#include <queue>
#include <memory>
#include <algorithm>
#include <functional>

using namespace std;
using namespace Eigen;

/// \brief maintaining A * A' where A is a time window with small dimension N
template<int N>
class Lxx_sx {
public:
	using Lxx_t = Matrix<float, N, N>;
	using sx_t  = Matrix<float, 1, N, RowMajor>;
	explicit Lxx_sx(ssize_t _size) : size(_size) {}

	using ini_t = Matrix<float, Dynamic, N, RowMajor>;

	void reset(const ini_t &_ini);
	void feed(const ini_t &ini, Lxx_t *out_lxx, sx_t *out_sx);
private:
	static Lxx_t F(const Matrix<float, 1, N, RowMajor> &y);
	ssize_t size;
	ini_t data;
	Lxx_t lxx;
	sx_t sx;
};

struct FFT_filter {
	int size, size2;
	float *H, *X, *b;
	PFFFT_Setup *s;
	ArrayXf acc;

	FFT_filter(const float *begin, const float *end);
	~FFT_filter();
	void step(const float *begin, const float *end, float *output);
	void step_silent(const float *begin, const float *end);
static void test();
private:
	void fillX(const float *begin, const float *end);
};

template<int N, int K>
class Lxy {
	FFT_filter *state[K][N];
	int size;
public:
	Matrix<float, 1, K, RowMajor> sy;
	float syy, syy_centered;
	using ini_t = Matrix<float, Dynamic, N, RowMajor>;
	Lxy(int _size, const Matrix<float, Dynamic, K, RowMajor> &_y);

	void reset(const ini_t &_ini);
	void feed(const ini_t &_ini, Matrix<float, N, K, ColMajor> *output);
	~Lxy();
};

class LinReg {
	Lxx_sx<4> lxx_sx;
	Lxy<4, 2> lxy;
	Matrix<float, 1, 4, RowMajor>* sx_buf;
	Matrix<float, 4, 4> *lxx_buf;
	Matrix<float, 4, 2, ColMajor> *lxy_buf;
	using M42f =  Matrix<float, 4, 2>;
	M42f* ph_out; Matrix<float, 3, 2>* rot_out;
	float* r2;
	int size;
public:
	LinReg(int _size, const complex<float> *ref);
	using IQmat = Matrix<float, Dynamic, 4, RowMajor>;
	~LinReg();

	struct feed_result {
		IQmat rawdata;
		Matrix<float, 4, 2> ph;
		Matrix<float, 3, 2> rot;
		float snr; int idx;
	};

	//The following 2 funcs assumes _d contains data of "size"!

	void reset(IQmat && d);
	static Matrix<float, 2, 1> dir(const Matrix<float, 2, 2>& ei);
	feed_result feed(IQmat&& d);
	static void test();
};

template<int N>
void Lxx_sx<N>::reset(const Matrix<float, Dynamic, N, RowMajor> &_ini)
{
	data = _ini;
	assert(data.rows() == size);
	lxx = data.transpose() * data;
	sx = data.colwise().sum();
}
template<int N>
void Lxx_sx<N>::feed(const Matrix<float, Dynamic, N, RowMajor> &ini, Matrix<float, N, N> *out_lxx, Matrix<float, 1, N, RowMajor> *out_sx)
{
	assert(ini.rows() == size);
	auto sx1 = sx;
	auto lxx1 = lxx;
	for (ssize_t i = 0; i < size; ++i) {
		out_sx[i] = sx1 += ini.row(i) - data.row(i);
		out_lxx[i] = lxx1 += F(ini.row(i)) - F(data.row(i));
	}
	reset(ini);
}
template<int N>
Matrix<float, N, N> Lxx_sx<N>::F(const Matrix<float, 1, N, RowMajor> &y) { return y.transpose() * y; }

FFT_filter::FFT_filter(const float *begin, const float *end)
{
	size = int(end - begin);
	assert(size > 0);
	size2 = 32; //minimum required by pffft library
	while (size2 < size * 2) { size2 <<= 1; }
	X = (float *) pffft_aligned_malloc(size2 * sizeof(float));
	b = (float *) pffft_aligned_malloc(size2 * sizeof(float));
	s = pffft_new_setup(size2, PFFFT_REAL);
	fillX(begin, end);
	H = X;

	X = (float *) pffft_aligned_malloc(size2 * sizeof(float));
	acc = ArrayXf::Zero(size);
}

FFT_filter::~FFT_filter()
{
	pffft_aligned_free(H);
	pffft_aligned_free(X);
	pffft_aligned_free(b);
	pffft_destroy_setup(s);
}

void FFT_filter::step(const float *begin, const float *end, float *output)
{
	fillX(begin, end);
	fill(b, b + size2, 0);
	pffft_zconvolve_accumulate(s, X, H, b, 1.f / size2);
	pffft_transform(s, b, X, nullptr, PFFFT_BACKWARD);

		Map<ArrayXf> out(output, size);
		out = acc + Map<ArrayXf>(X, size);
	
	copy(X + size, X + size * 2, acc.data());
}

void FFT_filter::step_silent(const float *begin, const float *end)
{
	fillX(begin, end);
	fill(b, b + size2, 0);
	pffft_zconvolve_accumulate(s, X, H, b, 1.f / size2);
	pffft_transform(s, b, X, nullptr, PFFFT_BACKWARD);

	copy(X + size, X + size * 2, acc.data());
}

template<int N, int K>
Lxy<N,K>::Lxy(int _size, const Matrix<float, Dynamic, K, RowMajor> &_y)
		: size(_size), sy(_y.colwise().sum()),
		syy(_y.squaredNorm()),
		syy_centered(syy - sy.squaredNorm() / _size)
{
	assert(_y.rows() == size);
	Matrix<float, Dynamic, K, ColMajor> y = _y.colwise().reverse();
	for (int i = 0; i < K; ++i) {
		float *pt = y.data() + i * size;
		for (int j = 0; j < N; ++j) {
			state[i][j] = new FFT_filter(pt, pt + size);
		}
	}
}

template<int N, int K>
void Lxy<N,K>::reset(const Matrix<float, Dynamic, N, RowMajor> &_ini)
{
	Matrix<float, Dynamic, N, ColMajor> ini = _ini;
	for (int i = 0; i < K; ++i) {
		for (int j = 0; j < N; ++j) {
			const float *pin = ini.data() + j * size;
			state[i][j]->step_silent
			(pin, pin + size);
		}
	}
}
template<int N, int K>
void Lxy<N,K>::feed(const Matrix<float, Dynamic, N, RowMajor> &_ini, Matrix<float, N, K, ColMajor> *output)
{
	Matrix<float, Dynamic, N, ColMajor> ini = _ini;
	Matrix<float, Dynamic, N * K, ColMajor> res1(size, N * K);
	for (int i = 0; i < K; ++i) {
		for (int j = 0; j < N; ++j) {
			const float *pin = ini.data() + j * size;
			state[i][j]->step(pin, pin + size, res1.data() + (i * N + j) * size);
		}
	}
	Matrix<float, Dynamic, N * K, RowMajor> res2 = res1;
	for (int i = 0; i < size; ++i) {
		output[i] = Map<Matrix<float, N, K, ColMajor>>(res2.data() + N * K * i);
	}
}
template<int N, int K>
Lxy<N,K>::~Lxy()
{
	for (int i = 0; i < K; ++i) {
		for (int j = 0; j < N; ++j) {
			delete state[i][j];
		}
	}
}

LinReg::LinReg(int _size, const complex<float> *ref) :
		lxx_sx(_size),
		lxy(_size, Map<const Matrix<float, Dynamic, 2, RowMajor>>((const float *) (ref), _size, 2)),
		sx_buf(new Matrix<float, 1, 4, RowMajor>[_size]),
		lxx_buf(new Matrix<float, 4, 4>[_size]),
		lxy_buf(new M42f[_size]),
		ph_out(new M42f[_size]),
		rot_out(new Matrix<float,3,2>[_size]),
		r2(new float[_size]),
		size(_size) {  }
using IQmat = Matrix<float, Dynamic, 4, RowMajor>;
LinReg::~LinReg()
{
	delete[] lxx_buf;
	delete[] lxy_buf;
	delete[] sx_buf;
	delete[] ph_out;
	delete[] rot_out;
	delete[] r2;
}

void LinReg::reset(IQmat && d)
{
	lxx_sx.reset(d);
	lxy.reset(d);
}
Matrix<float, 2, 1> LinReg::dir(const Matrix<float, 2, 2>& ei)
{
	SelfAdjointEigenSolver<Matrix<float, 2, 2>> s;
	s.computeDirect(ei);
	auto e = s.eigenvalues();
	return s.eigenvectors().col( isnan(e(1)) || e(0) > e(1) ? 0 : 1 );
}
LinReg::feed_result LinReg::feed(IQmat&& d)
{
	lxx_sx.feed(d, lxx_buf, sx_buf);
	lxy.feed(d, lxy_buf);
	for (int i = 0; i < size; ++i) {
		//ph
		auto centered = lxx_buf[i] - sx_buf[i].transpose() * sx_buf[i] / size;
		M42f ph;
		ph << dir(centered.block<2, 2>(0, 0)), Matrix<float, 2, 1>::Zero(),
		Matrix<float, 2, 1>::Zero(), dir(centered.block<2, 2>(2, 2));
		ph_out[i] = ph;

		auto sx2 = sx_buf[i] * ph; // 1 * 2
		Matrix<float, 3, 3> lxx3;
		lxx3 << ph.transpose() * lxx_buf[i] * ph, sx2.transpose(),
				sx2, size;
		Matrix<float, 3, 2> lxy3;
		lxy3 << ph.transpose() * lxy_buf[i], lxy.sy;
		lxx3 = lxx3.selfadjointView<Eigen::Upper>();
		//rot_out[i] = lxx3.inverse() * lxy3;
		rot_out[i] = lxx3.ldlt().solve(lxy3);
		r2[i] = lxy.syy - (lxy3.transpose() * rot_out[i]).trace();
		if (isnan(r2[i])) {
			cerr << "WARNING: NOW r2 "<< r2[i] << "\nlxx is\n" << lxx_buf[i] << "\n\n";
			cerr << "centered lxx is\n" << centered << "\n\n";
			cerr << "lxy is\n" << lxy_buf[i] << "\n\n";
			cerr << "ph_out is\n" << ph_out[i] << "\n\n";
			cerr << "rot_out is\n" << rot_out[i] << "\n\n";
			r2[i] = 2333;
		}
	}
	auto i = int(min_element(r2, r2 + size) - r2);
	return LinReg::feed_result { d, ph_out[i], rot_out[i], lxy.syy_centered / r2[i], i };
};

#include "pffft/pffft.c"

#define LOGM(...) fprintf(stderr, __VA_ARGS__)

//
// Created by prwang on 2/14/2019.
//

template<class T>
class SafeQueue {
public:
	void enqueue(T&& t)
	{
		std::lock_guard<std::mutex> lock(m);
		q.push(t);
		c.notify_one();
	}

	T dequeue()
	{
		std::unique_lock<std::mutex> lock(m);
		while (q.empty()) {
			c.wait(lock);
		}
		T val = q.front();
		q.pop();
		return val;
	}

private:
	std::queue<T> q;
	mutable std::mutex m;
	std::condition_variable c;
};

using Sample_t = Matrix<float, 1, 4, RowMajor>;
struct Fn {
	ssize_t end, __ref__size;
	using queue_t = SafeQueue<LinReg::IQmat>;
	queue_t *q;
	LinReg::IQmat curr;
	soxr_t soxr;
	soxr_error_t error;
	function<float*(size_t)> blockget;
#define assertsoxr()  if (error) { LOGM("soxr error %s\n", soxr_strerror(error)); assert(0 && "SOXR_ERR"); } 
	static const int sphy_maxsp =  2048;

	LinReg::IQmat buf_internal;
	static size_t resample_feeder(void* _state, soxr_cbuf_t * _data,
			size_t requested_4float)
	{
		Fn* s = reinterpret_cast<Fn*>(_state);
		// s->get_rx_data_blocked((float*)(*_data = s->data_buf), requested_len * 4);
		*_data = (soxr_cbuf_t)s->blockget(requested_4float);
		return requested_4float;
	}
	Fn(ssize_t _ref_size, queue_t *_q, function<float*(size_t)> _blockget, int max_data_once) 
		: end(0), __ref__size(_ref_size), q(_q), curr(__ref__size, 4),
		buf_internal(sphy_maxsp * 10, 4)
	{
		blockget = _blockget;  // save the function
		soxr = soxr_create(56875, 80000, 4,         /* Input rate, output rate, # of channels. */
				&error,  NULL, NULL, NULL);
		assertsoxr();
		error = soxr_set_input_fn(soxr, &resample_feeder, (void*)this, max_data_once);
		assertsoxr();
	}
	~Fn() {
		soxr_delete(soxr);
	}
	void operator()()
	{
		int N = soxr_output(soxr, buf_internal.data(), sphy_maxsp);
		error = soxr_error(soxr);
		assertsoxr();
		for (int i = 0; i < N; ++i) {
			curr.row(end++) = buf_internal.row(i);
			if (end >= __ref__size) {
				q->enqueue(move(curr));
				new(&curr) LinReg::IQmat(__ref__size, 4);
				end = 0;
			}
		}
	}
};

using queue_t = SafeQueue<LinReg::IQmat>;
void FFT_filter::test()
{
	float h[3] = {1, 2, 1};
	FFT_filter f(h, h + 3);
	float x[] = {1, 2, 1, 0, 0, 1, 3, 3, 1, 0, 0, 0};
	float y[] = {1, 4, 6, 4, 1, 1, 5, 10, 10, 5, 1, 0};
	for (int i = 0; i < 4; ++i) {
		float z[3];
		f.step(x + i * 3, x + (i + 1) * 3, z);
		for (int j = 0; j < 3; ++j) {
			assert(0.001 > fabs(z[j] - y[j + i * 3]));
		}
	}
}

void FFT_filter::fillX(const float *begin, const float *end)
{
	assert((end - begin) == size);
	copy(begin, end, b);
	fill(b + size, b + size2, 0);
	pffft_transform(s, b, X, nullptr, PFFFT_FORWARD);
}

#if 0
void LinReg::test()
{
	const int n = 100, wnd = 10;
	float t1[n][4] = {
#include "inc_data/t1.csv.test"
	};
	complex<float> ref[wnd]
	{
#include "inc_data/b.csv.test"
	};

	LinReg t(wnd, ref);
	for (int i = 0; i < n / wnd; ++i) {
		auto m = IQmat(wnd, 4);
		memcpy(m.data(), t1 + i * wnd, sizeof(float) * 4 * wnd);
		if (i == 0) {
			t.reset(move(m));
		} else {
			printf("%f\n", t.feed(move(m)).snr);
		}
	}
}
#endif

#if 0
extern "C" DLLEXPORT  void test() { LinReg::test(); }
#if 0
extern "C" {
	extern const float  preamble_ref[0][2];
	extern const float  preamble_ref_end[0][2];
};
const int _ref_size = (preamble_ref_end - preamble_ref);
#endif 
#endif

#if 0
	extern "C" DLLEXPORT
#endif
void preamble_rx(int pts, float *_out, volatile float *snr,
		int _ref_size,
		const float* preamble_ref,
		function<float*(size_t)> blockget, int max_data_once, volatile bool *preamble_running, volatile bool *close_at_end)
{
	LOGM("ref_size=%d\n", _ref_size);
	LinReg linreg(_ref_size, reinterpret_cast<const complex<float> *>(preamble_ref));
	struct Proj {
		Matrix<float, 4, 2> phrot;
		Matrix<float, 1, 2> bias;
		float *out = nullptr; 
		int remaining;
		Proj(const Matrix<float, 4, 2>& ph, const Matrix<float, 3, 2> &rot,
				float *_out, int _remaining)
			: phrot(ph * rot.block<2, 2>(0, 0)), bias(rot.block<1, 2>(2, 0)),
			out(_out) ,  remaining(_remaining){

				cerr << "phase matrix: \n" << ph << "\n\n rotation matrix:\n"
					<< rot << "\n";
			}
		bool operator()(const LinReg::IQmat &x)
		{
			auto r = (x * phrot).rowwise() + bias;
			int d = x.rows();
			using w_t = Map<Matrix<float, Dynamic, 2, RowMajor>>;
			if (remaining > d) {
				w_t(out, d, 2) = r;
				out += 2 * d; remaining -= d;
				return true;
			} else {
				w_t(out, remaining, 2) = r.block(0, 0, remaining, 2);
				out += 2 * remaining; remaining = 0;
				return false;
			}
		}
		Proj() = default;
	};
	Proj p;
	deque<LinReg::feed_result> annotated;
	// assert(SoftPHY::devices.size() == 1);
	// SoftPHY& s = *SoftPHY::devices.begin()->second;
	// assert(0 == s.start_rx_data_transfer());

	SafeQueue<LinReg::IQmat> Q;
	atomic<bool> flag(true);

	Fn f(_ref_size, &Q, blockget, max_data_once);
	thread th{[&f, &flag]() {
		while (flag) {
			f();
		}
	}};

	float snrthres = *snr;
	LOGM("snrthres%f\n", snrthres);
	for (size_t _ = 0;; _++) {
		auto t = Q.dequeue();
		if (!_) {
			linreg.reset(move(t));
		} else {
			annotated.push_back(linreg.feed(move(t)));
			assert(annotated.size() <= 3);
#define S(i) (annotated[i].snr)
			if (annotated.size() == 3) {
				*snr = 0; //sync tx and rx
				if (S(1) >= S(2) && S(1) >= S(0) && S(1) >= snrthres) {
					new(&p) Proj(annotated[1].ph, annotated[1].rot, _out, pts);
					fprintf(stderr, "preamble found with snr %f\n", S(1));
					*snr = S(1);
					break;
				} else if (!*preamble_running) {
					printf("preamble listening stopped\n");
					flag.store(false);
					th.join();
					if (close_at_end) *close_at_end = false;
					return;
				} else {
					annotated.pop_front();
					fprintf(stderr, "snr %f %dms = %dblks\n", S(1), (int)clock(), (int)_);
				}
			}
		}
	}
	int idx = annotated[1].idx, nb1 = _ref_size - idx - 1;
	assert(idx <= _ref_size);
	//assert(pts >= _ref_size);
	if (p(annotated[0].rawdata.block(idx + 1, 0, nb1, 4)) && 
			p(annotated[1].rawdata) && p(annotated[2].rawdata)) {
		while (p(Q.dequeue()));
	}
	flag.store(false);
	th.join();
	if (close_at_end) *close_at_end = false;
	// assert(0 == s.stop_rx_data_transfer());
}
