#ifndef M_SEQUENCE_H
#define M_SEQUENCE_H

#include <vector>
#include <assert.h>
#include "stdint.h"
using std::vector;

// see this in https://www.gaussianwaves.com/2018/09/maximum-length-sequences-m-sequences/
// or Chinese version https://blog.csdn.net/neufeifatonju/article/details/81069039
static const char* primitive_polynomials[] = {
    "10",  // x + 1
    "210",  // x^2 + x + 1
    "310",  // x^3 + x + 1
    "410",  // x^4 + x + 1
    "520",  // x^5 + x^2 + 1
    "610",  // x^6 + x + 1
    "710",  // x^7 + x + 1
    "87210",  // x^8 + x^7 + x^2 + x + 1
    "940",  // x^9 + x^4 + 1
    "A30",  // x^10 + x^3 + 1
    "B20",  // x^11 + x^2 + 1
    "C6410",  // x^12 + x^6 + x^4 + x + 1
    "D4310",  // x^13 + x^4 + x^3 + x + 1
    "EA610",  // x^14 + x^10 + x^6 + x + 1
    "F10",  // x^15 + x + 1
    "GC310",  // x^16 + x^12 + x^3 + x + 1
    "H30",  // x^17 + x^3 + 1
};

vector<bool> generate_m_sequence(int m_order) {
    assert(m_order >= 1 && m_order <= 17 && "can only generate those orders");
    int seq_length = (1 << m_order) - 1;
    vector<bool> vec; vec.resize(seq_length);
    // construct basic LFSR architecture
    uint32_t coeffs = 0x000;  // for `m_order`, it has `m_order` multiplier in the Galois LFSR structure
    const char* s = primitive_polynomials[m_order - 1];
    while (*s) {
        char c = *(s++); int idx = c >= '0' && c <= '9' ? c - '0' : c - 'A' + 10;
        coeffs |= (1 << idx);
    }
    // then output the structure by shifting out
    uint32_t registers = 0x001;  // for `m_order`, it has `m_order` registers in the Galois LFSR structure
    for (int i=0; i<seq_length; ++i) {
        bool output_bit = (registers >> (m_order-1)) & 0x01;
        registers = (registers << 1) ^ (output_bit ? coeffs : 0);
        vec[i] = output_bit;
    }
    return vec;
}

#endif
