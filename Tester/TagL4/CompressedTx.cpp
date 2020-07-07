#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"

TagL4Host_t tag;

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: <portname>\n");
		return -1;
	}

	tag.verbose = true;
	tag.open(argv[1]);

	Tag_Sample_t zero;
	tag.set_tx_default_sample(zero);  // set default sample

	vector<string> compressed;
	compressed.push_back("0000000000000000:32");  // B0
	compressed.push_back("0F0F0F0F0F0F0F0F:32");  // B1
	compressed.push_back("FFFFFFFFFFFFFFFF:32");  // B3
	compressed.push_back("F0F0F0F0F0F0F0F0:128");  // B2
	compressed.push_back("0F0F0F0F0F0F0F0F:128");  // B1
	compressed.push_back("F0F0F0F0F0F0F0F0:128");  // B2
	compressed.push_back("0000000000000000:128");  // B0

	int length;
	tag.tx_send_compressed(8e3, compressed, length);
	printf("send %d samples from compressed string\n", length);

	tag.close();
}
