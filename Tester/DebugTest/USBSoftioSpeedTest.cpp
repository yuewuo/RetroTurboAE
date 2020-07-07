#define MEMORY_READER_H7XX
#include "reader-H7xx.h"
#include "softio.h"
#include "assert.h"
#include "serial/serial.h"
#include <chrono>
#include <thread>
using namespace std::chrono;
#define sleep_ms(n) std::this_thread::sleep_for(std::chrono::milliseconds(n))

ReaderH7_Mem_t h7mem;
SoftIO_t h7sio;
serial::Serial *com;

static size_t available() {
	size_t s = com->available();
	return s;
}

static size_t gets(char *buffer, size_t size) {
	size_t s = com->read((uint8_t*)buffer, size);
	com->flush();
	return s;
}

static size_t puts(char *buffer, size_t size) {
	size_t s = com->write((uint8_t*)buffer, size);
	com->flush();
	return s;
}

int main(int argc, char** argv) {

	printf("This program requires specialized firmware which always fullfill testio\n");
	printf("You can download the firmware for STM23H743ZI at https://github.com/wuyuepku/RetroTurbo/releases/tag/r0.1.2\n");

	if (argc != 2) {
		printf("usage: ./USBSoftioSpeedTest <portname>\n");
		return -1;
	}

	com = new serial::Serial(argv[1], 115200, serial::Timeout::simpleTimeout(1000));
	assert(com->isOpen());
	SOFTIO_QUICK_INIT(h7sio, h7mem, ReaderH7_Mem_FifoInit);
	h7sio.gets = gets;
	h7sio.puts = puts;
	h7sio.available = available;

	printf("com->available() = %d\n", (int)com->available());  // Oh!! this will be non-zero sometimes!
	while (com->available()) {
		com->read(com->available());
	}

	printf("\n1. testing fifo reading speed:\n");
	auto start = high_resolution_clock::now();
	size_t count = 0;
	while(1) {
		softio_delay_flush_try(read_fifo, h7sio, h7mem.testio);  // this will try handle message after flush current request
		auto now = high_resolution_clock::now();
		auto dura = duration_cast<duration<double>>(now - start);
		count += fifo_count(&h7mem.testio);
		fifo_clear(&h7mem.testio);
		// printf("dura.count() = %f\n", dura.count());
		if (dura.count() > 1) break;
		// sleep_ms(1);
	}
	printf("count = %d, speed = %fmbps\n", (int)count, count * 8.0 / 1e6);

	com->close();
}
