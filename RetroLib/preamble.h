
//
// Created by prwang on 2/14/2019.

#ifndef PASSIVEVLCSPHY_PREAMBLE_H
#define PASSIVEVLCSPHY_PREAMBLE_H

#include <functional>
using namespace std;

extern void preamble_rx(int pts, float *_out, volatile float *snr,
		int _ref_size,
		const float* preamble_ref,
		function<float*(size_t)> blockget, int max_data_once, volatile bool *preamble_running, volatile bool *close_at_end);

#endif //PASSIVEVLCSPHY_PREAMBLE_H
