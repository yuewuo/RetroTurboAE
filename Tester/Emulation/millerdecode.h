//
// Created by prwang on 7/19/2019.
//

#ifndef RETROTURBO_MILLERDECODE_H
#define RETROTURBO_MILLERDECODE_H
#define sample_rate  20000
#ifdef __cplusplus
extern "C" {
#endif
// #define sched_yield() void(0)
#include "mycomplex.h"

#ifndef STM32F7XX
#define printf_uart(...) printf(__VA_ARGS__)
#define reader_ctrl_fprintf(f, a, ...) fprintf(f, a, __VA_ARGS__)
#define reader_ctrl_fprint(f, a) fprintf(f, a)
#define CTRL_FLOW_PHY_MSG stdout
/* Response status */
#define RES_NO 0
#define RES_NORMAL 1
#define RES_ERROR 2
#else
#include "reader_phy.h"
#include "reader_ctrl.h"
#include "packet.h"
#endif

enum states {
	P01,
	P11,
	P1100x,
	P_110x,
	P_11000x,
	n_states,
};
enum rates {
	R125,
	R250,
	R500,
	R1000,
	n_rates
};

#define preamble_ref_size  1120
#define data_ref_size (sample_rate*12/125)

extern const float preamble_ref[preamble_ref_size];
extern const float refscat[n_rates][data_ref_size];
void miller_decode_init(const cf *sample_buffer, const volatile int *sample_size);
extern "C" int miller_decode_one(int n_bits, unsigned char *byte_out, double *SNR, int *Offset, int *Mf, double *G_scale_re, double *G_scale_im, double *D, double *G_dc_re, double *G_dc_im, double *Dp);
int decode_config(const char *cfg);
int decode_set_rate(int rate);
#ifdef __cplusplus
}
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <assert.h>

template<class ...Ts> constexpr size_t va_size(Ts... a) { return sizeof...(a); }
#define sscanf_multiple(str, format, ...)  for (int rc = 0, rcd = 0; (int)va_size(__VA_ARGS__) == sscanf((str) + rc, format "%n", __VA_ARGS__,  &rcd); rc += rcd)
#ifdef STM32F7XX
#define LOGM(...) printf_uart(__VA_ARGS__)
#define sched_yield() void(0)
#include "reader_phy.h"
#else
#define LOGM(...) fprintf(stderr, __VA_ARGS__)

#include <tuple>
#include <vector>

using namespace std;

#endif

template<class T>
inline T replace_element(T x) { return x; };

template<class T, T a, T b, T... others>
inline T replace_element(T x) { return x == a ? b : replace_element<T, others...>(x); };

struct miller_decode {
protected:
	float c_prefix_range = 0.05, c_d0 = 9.6,
					c_snr_threshold = 1.15, c_prefix_time = 0.15, cf_dump_samples = 0, cf_sample_all_anyway = 0, cf_print_transitions = 0,
					c_scale_learn_rate = 0, c_dc_learn_rate = 0;//-c c_prefix_range=0.1;c_prefix_time=0.14;
	int sample_per_bit, c_rate;
	cf g_scale = C0, g_dc = C0;
	int g_data_pos = 1e9;
public:
	int config(const char *c_str) {
		char key[200], val[200], c1[400];
		strncpy(c1, c_str, 395);
		int c1size = strlen(c1), cinv = strspn(c1, "abcdefghijklmnopqrstuvwxyz=_0.123456789;");
		if (cinv != c1size) {
			reader_ctrl_fprintf(CTRL_FLOW_PHY_MSG, "invalid char at %d\n", cinv);
			return RES_ERROR;
		}
		for (int i = 0; i < c1size; ++i) {
			c1[i] = replace_element<char, '=', ' ', ';', '\n'>(c1[i]);
		}
#define set_var_float(VAR, rmin, rmax)\
  if (strcmp(key, #VAR) == 0) {\
    float tmp = strtof(val, 0);\
    if (tmp < (rmin) || tmp > (rmax)){ \
      reader_ctrl_fprintf(CTRL_FLOW_PHY_MSG, "%s out of range(%f, %f)", #VAR, float(rmin), float(rmax));\
      return RES_ERROR; \
    } \
    VAR = tmp; \
    reader_ctrl_fprintf(CTRL_FLOW_PHY_MSG, "%s set to %f, ", #VAR, VAR);\
  }
		sscanf_multiple(c1, "%38s %38s\n", key, val) {
			set_var_float(c_snr_threshold, 1.0001, 100);
			set_var_float(c_d0, -166, 166);
			set_var_float(c_prefix_range, 0.002, 1);
			set_var_float(c_prefix_time, c_prefix_range + (float) preamble_ref_size / sample_rate, 1);
			set_var_float(cf_dump_samples, 0, 1);
			set_var_float(cf_sample_all_anyway, 0, 1);
			set_var_float(cf_print_transitions, 0, 1);
			set_var_float(c_scale_learn_rate, 0, 1);
			set_var_float(c_dc_learn_rate, 0, 1);
		}
		return RES_NORMAL;
#undef set_var_float
	}

public:
	void set_rate(int rate) {
		const int _sample_per_bit[] = {160, 80, 40, 20, 10};
		sample_per_bit = _sample_per_bit[c_rate = rate];
		refscat = ::refscat[rate];
		reader_ctrl_fprintf(CTRL_FLOW_PHY_MSG, "coding set to %d (sample_per_bit = %d)", c_rate, sample_per_bit);
	}

	void dump_samples(int n_bits) {
		int max_samples = sample_rate * (c_prefix_time + c_prefix_range) + sample_per_bit * (n_bits + 4) + 20;
		if (cf_sample_all_anyway != 0 || cf_dump_samples != 0) {
			sample_wait(max_samples);
		}
		if (cf_dump_samples != 0) {
			printf_uart("samples:\n[");
			for (int i = 0; i < max_samples; ++i)
				printf_uart("%.3f,%.3f;", sample_buffer[i].re, sample_buffer[i].im);
			printf_uart("]\n");
		}
	}


	int get_prefix_samp(double *SNR, int *Offset, int *Mf, double *G_scale_re, double *G_scale_im, double *D, double *G_dc_re, double *G_dc_im) {
		int offset = sample_rate * c_prefix_time;
		int o, mf = 0, range = c_prefix_range * sample_rate;
		float snr = 0;
		const cf *samp_start = sample_buffer + offset - preamble_ref_size;//!!!;
		LOGM("preamble: ofst=%d, range=%d, sample_buffer = %p\n", offset, range, sample_buffer);
		sample_wait(offset + range + 1);

		if (offset <= preamble_ref_size + range) {
			reader_ctrl_fprint(CTRL_FLOW_PHY_MSG, "overflow, c_prefix_time and c_prefix_range is not set correctly\n");
			return RES_NO;
		}
		for (o = -range; o <= range; ++o) {
			const cf *data = samp_start + o;
			cf sx = C0, sxy = C0;
			float sxx = 0, syy = 0, sy = 0;
			for (int i = 0; i < preamble_ref_size; ++i) {
				sx = cadd(data[i], sx);
				sy += preamble_ref[i];
				sxx += csqr(data[i]);
				sxy = cadd(sxy, cfmul(cconj(data[i]), preamble_ref[i]));
				syy += sqr(preamble_ref[i]);
			}

			float tnc = -1.f / preamble_ref_size;
			float Sxx = sxx + tnc * csqr(sx),
							Syy = syy + tnc * sqr(sy);
			cf Sxy = cadd(sxy, cfmul(cconj(sx), (sy * tnc))),
							scale = cfmul(Sxy, 1. / Sxx), dc = cfmul(cadd(ccmul(scale, sx), cf{-sy, 0}), tnc);

			float snr1 = Syy / (Syy - Sxx * csqr(scale));
			if (snr < snr1) {
				snr = snr1, mf = o, g_scale = scale, g_dc = dc;
			}
		}

#define WLd4 (299792458.f / (455.015e3f * 4))
		float d = c_d0 - atan2f(g_scale.im, g_scale.re) / M_PI * WLd4;
		d = d < 0 ? d + WLd4 : (d > WLd4 ? d - WLd4 : d);
		*SNR = (double)snr;
		*Offset = offset;
		*Mf = mf;
		*G_scale_re = (double)g_scale.re;
		*G_scale_im = (double)g_scale.im;
		*D = (double)d;
		*G_dc_re = (double)g_dc.re;
		*G_dc_im = (double)g_dc.im;
		reader_ctrl_fprintf(CTRL_FLOW_PHY_MSG, ", SOFSNR=%.8f(@%d + %d, scale=%.4f+%.4fj, d = %.4f m),"
																					 " dc = %.4f+%.4fj", snr, offset, mf, g_scale.re, g_scale.im, d,
												g_dc.re, g_dc.im);
		if (snr < c_snr_threshold)
			return -1;
		return g_data_pos = offset + mf;
	}

	static constexpr int max_bits = 200, delay_bits = 16;
protected:
	struct path {
		float metric;
		unsigned choice;
		cf scale, dc;
		bool operator<(const path &other) const { return metric < other.metric; }
		cf view(const cf &sample) const { return cadd(ccmul(sample, scale), dc); }
	} f[max_bits][n_states];

	template<class T>
	static const T &min_multiple(const T &a) { return a; }

	template<class T, class... Tp>
	static const T &min_multiple(const T &a, const Tp &... bs) {
		const T &b = min_multiple(bs...);
		return a < b ? a : b;
	}

	const float *refscat;

	void sample_wait(int idx) { while (*sample_size < idx) { sched_yield(); }}

public:
	bool bit_out[max_bits];
	static constexpr int E[5][2] = {[P01] = {P11, P_110x}, [P11] = {P1100x, P_110x}, [P1100x] = {P11, P01}, [P_110x] = {
					P_11000x, P01}, [P_11000x] = {P11, P01}};
#define eid(e) (unsigned)(&e - E[0])
#define ref(e) &refscat[eid(e) * sample_per_bit]
	float dp(int n_bits) {
		memset(bit_out, 0, sizeof(bit_out));
		for (path *i = f[0]; i < f[max_bits]; ++i) { i->metric = FLT_MAX; }
		//init state, send 0 (real 11)
		//finish state, send 0 again, use bits_opt[n + 1][0]
		f[0][P11] = path{0, 0, g_scale, g_dc};
		const cf *data = sample_buffer + g_data_pos;
		for (int i = 0; i <= n_bits; ++i, data += sample_per_bit) {
			if (i > delay_bits) {
				 bit_out[i - delay_bits - 1] = min_multiple(f[i][P01], f[i][P11], f[i][P_110x], f[i][P_11000x], f[i][P1100x]).choice >> delay_bits & 1;
			}
			for (int j = 0; j < n_states; ++j) {
				const path &from = f[i][j];
				if (cf_print_transitions && from.metric < 1e15) {
					LOGM("= (pos %d, state %d) %f %08x\n", i, j,  from.metric, from.choice);
				}
				for (int k = 0; k < 2; ++k) {
					const int &e = E[j][k];
					cf scale_grad = C0, dc_grad = C0;
					float new_metric = from.metric;
					float const *ref = ref(e);
					for (int l = 0; l < sample_per_bit; ++l) {
						cf err = cadd(from.view(data[l]), cf{-ref[l], 0});
						new_metric += csqr(err);
						dc_grad = cadd(dc_grad, err);
						scale_grad = cadd(scale_grad, ccmul(cconj(data[l]), err));
					}

					if (cf_print_transitions && from.metric < 1e15) {
						LOGM("> (pos %d, state %d) %f %08x, eid = %d\n", i + 1, e, new_metric, from.choice << 1 | k, eid(e));
					}

					path &next = f[i + 1][e],
									new_path{new_metric, from.choice << 1 |  k,
													 cadd(from.scale, cfmul(scale_grad, -c_scale_learn_rate / (float) sample_per_bit)),
													 cadd(from.dc, cfmul(dc_grad, -c_dc_learn_rate / (float) sample_per_bit))};
					if (new_path < next) { next = new_path; }
				}
			}
		}
#if 0
		auto &final = bit_opt[n_bits + 1][0];
		for (int i = 1; i <= delay_bits; ++i) {
			bit_out[n_bits - i] =  final.choice >> i & 1;
		}
		return final.metric;
#endif
		auto const &final = min_multiple(f[n_bits + 1][P11], f[n_bits + 1][P_11000x], f[n_bits + 1][P1100x]);
		for (int i = delay_bits; i >= 1; --i) {
			bit_out[n_bits - i] = final.choice >> i & 1;
		}
		return final.metric;
	}
	const cf *sample_buffer;
	const volatile int *sample_size;
};

constexpr int miller_decode::E[5][2];
extern miller_decode decoder;
#endif
#endif //RETROTURBO_MILLERDECODE_H
