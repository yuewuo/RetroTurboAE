// given the longest effect length (in bit), output valid miller

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "assert.h"
#include <set>
using namespace std;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: <effect_length>\n");
		return -1;
	}

	int effect_length = atoi(argv[1]);
	assert(effect_length <= 16 && "cannot process too large number");
	int cnt = 1 << effect_length;
	printf("cnt: %d, should be %d output (because of different current state and last bit)\n", cnt, 4 * cnt);

	set<int> arrs;

	for (int i=0; i<cnt; ++i) {
		for (int _previous_logic_bit=0; _previous_logic_bit<2; ++_previous_logic_bit) {
			for (int _signal=0; _signal<2; ++_signal) {
				int output = 0;
				int previous_logic_bit = _previous_logic_bit;
				int signal = _signal;
				printf("(%d%d)", previous_logic_bit, signal);
				for (int k=0; k<effect_length; ++k) {
					printf("%d", (i >> k) & 1);
				} printf(":");
				for (int k=0; k<effect_length; ++k) {
					int bit = (i >> k) & 1;
					if (bit == 0) {
						if (previous_logic_bit == 0) {
							signal = 1 - signal;
							output |= signal << (2*k);
							output |= signal << (2*k+1);
						}
						else {
							output |= signal << (2*k);
							output |= signal << (2*k+1);
						}
					} else {
						output |= signal << (2*k);
						signal = 1 - signal;
						output |= signal << (2*k+1);
					}
					previous_logic_bit = bit;
				}
				for (int k=0; k<2*effect_length; ++k) {
					printf("%d", (output >> k) & 1);
				}
				arrs.insert(output);
				printf("\n");
			}
		}
	}
	printf("has %d different elements\n", (int)arrs.size());

}
