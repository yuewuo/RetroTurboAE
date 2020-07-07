#define MEMORY_READER_H7XX
#include "reader-H7xx.h"
#include "softio.h"
#include "assert.h"
#include "serial/serial.h"
#include <chrono>
#include <thread>
#define sleep_ms(n) std::this_thread::sleep_for(std::chrono::milliseconds(n))

ReaderH7_Mem_t h7mem;
SoftIO_t h7sio;
serial::Serial *com;

static size_t available() {
	size_t s = com->available();
	printf("available %d\n", (int)s);
	return s;
}

static size_t gets(char *buffer, size_t size) {
	size_t s = com->read((uint8_t*)buffer, size);
	com->flush();
	printf("gets %p, %d, return %d\n", buffer, (int)size, (int)s);
	return s;
}

static size_t puts(char *buffer, size_t size) {
	size_t s = com->write((uint8_t*)buffer, size);
	com->flush();
	printf("puts %p, %d, return %d\n", buffer, (int)size, (int)s);
	return s;
}

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("usage: ./ReaderH7SoftIO <portname>\n");
		return -1;
	}

	com = new serial::Serial(argv[1], 115200, serial::Timeout::simpleTimeout(1000));
	assert(com->isOpen());
	SOFTIO_QUICK_INIT(h7sio, h7mem, ReaderH7_Mem_FifoInit);
	h7sio.gets = gets;
	h7sio.puts = puts;
	h7sio.available = available;

	printf("\n1. testing blocking read:\n");
	softio_blocking(read, h7sio, h7mem.pid);
	// softio_blocking_read(h7sio, h7mem.pid);
	printf("pid: 0x%04X\n", h7mem.pid);
	
	printf("\n2. testing multi-issue delay read:\n");
	softio_delay(read, h7sio, h7mem.version);  // this will NOT send request unless softio_flush(softio) is called!
	// use softio_delay_flush(...) instead for requests that consumes much time on the peer side
	softio_delay(read, h7sio, h7mem.status);
	softio_wait_delayed(h7sio);
	printf("version: 0x%08X, status: 0x%02X\n", h7mem.version, h7mem.status);
	assert(h7mem.version == MEMORY_READER_H7XX_VERSION);

	printf("\n3. testing blocking write:\n");
	h7mem.resv = 0x8E;
	softio_blocking(write, h7sio, h7mem.resv);
	h7mem.resv = 0x00;
	softio_blocking(read, h7sio, h7mem.resv);
	assert(h7mem.resv == 0x8E);
	printf("OK\n");

	printf("\n4. testing multi-issue delay write and read:\n");
	h7mem.resv = 0x5D;
	softio_delay_flush(write, h7sio, h7mem.resv);
	h7mem.resv = 0x3A;
	softio_delay(write, h7sio, h7mem.resv);  // the order is remained with multi-issue
	for (int i=0; i<32; ++i) {  // larger than softio default issue queue size, to test whether it works properly
		softio_delay(read, h7sio, h7mem.resv);
	}
	softio_wait_delayed(h7sio);
	assert(h7mem.resv == 0x3A);
	printf("OK\n");

	printf("\n5. testing fifo write (ONLY blocking is allowed, to keep the data integrity !!! ):\n");
	softio_blocking(clear_fifo, h7sio, h7mem.testio);
	fifo_clear(&h7mem.testio);
	for (int i=0; i<900 && !fifo_full(&h7mem.testio); ++i) fifo_enque(&h7mem.testio, i);
	printf("fifo_count(&h7mem.testio) = %d\n", fifo_count(&h7mem.testio));
	while (fifo_count(&h7mem.testio) != 0) {
		softio_blocking(write_fifo, h7sio, h7mem.testio);
	}
	Fifo_t tmp = h7mem.testio;
	softio_blocking(read, h7sio, h7mem.testio);  // copy the peer fifo_t, only used to test length
	assert(fifo_count(&h7mem.testio) == 900);
	h7mem.testio = tmp;

	printf("\n6. testing fifo read multi-issue:\n");
	for (int i=0; i<10; ++i) {
		softio_delay_flush_try(read_fifo, h7sio, h7mem.testio);  // this will try handle message after flush current request
		// sleep_ms(1);
	}
	softio_wait_delayed(h7sio);
	printf("fifo_count(&h7mem.testio) = %d\n", fifo_count(&h7mem.testio));
	assert(fifo_count(&h7mem.testio) == 900);

	com->close();
}
