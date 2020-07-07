/*
 * This program is to generate M-sequence, for the ease of develop LCD reference and simulator
 */

#include "stdio.h"
#include "stdlib.h"
#include "m_sequence.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage: <M sequence order, should be 1~12>\n");
        return -1;
    }

    int m_order = atoi(argv[1]);
    vector<bool> vec = generate_m_sequence(m_order);

    // verbose it
    for (size_t i=0; i<vec.size(); ++i) {
        printf("%d", (int)vec[i]);
    }

    return 0;
}
