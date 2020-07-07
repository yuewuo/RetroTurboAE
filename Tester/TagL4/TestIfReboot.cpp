/*
 * The new tag comes with rechargeable battery, and this program is used to test whether MCU 
 * is rebooted after break USB connection.
 */

#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"

TagL4Host_t tag;

int main(int argc, char** argv) {
	if (argc != 2 && argc != 3) {
		printf("usage: <portname> <write_value:666>\n");
		return -1;
	}

	tag.verbose = true;
	tag.open(argv[1]);

    int write_value = 666;
    if (argc == 3) write_value = atoi(argv[2]);

    softio_blocking(read, tag.sio, tag.mem.siorx_overflow);
    printf("before write: %d\n", tag.mem.siorx_overflow);

    tag.mem.siorx_overflow = write_value;
    softio_blocking(write, tag.sio, tag.mem.siorx_overflow);
    softio_blocking(read, tag.sio, tag.mem.siorx_overflow);
    printf("after write: %d\n", tag.mem.siorx_overflow);

	tag.close();
}

