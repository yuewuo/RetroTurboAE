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
	tag.dump(~0);  // dump all
	tag.close();
}
