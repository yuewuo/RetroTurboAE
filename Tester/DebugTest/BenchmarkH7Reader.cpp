#include "reader-H7xx-ex.h"
using namespace std::chrono;
#define sleep_ms(n) std::this_thread::sleep_for(std::chrono::milliseconds(n))

ReaderH7_Mem_t h7mem;
SoftIO_t h7sio;
serial::Serial *com;

static size_t my_available() {
	size_t s = com->available();
	return s;
}

static size_t my_gets(char *buffer, size_t size) {
	size_t s = com->read((uint8_t*)buffer, size);
	com->flush();
	return s;
}

static size_t my_puts(char *buffer, size_t size) {
	size_t s = com->write((uint8_t*)buffer, size);
	com->flush();
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
	h7sio.gets = my_gets;
	h7sio.puts = my_puts;
	h7sio.available = my_available;

	printf("h7mem size is: %d\n", (int)sizeof(h7mem));

	softio_blocking(read_between, h7sio, h7mem.command, h7mem.guard_small_varibles);
	printf("pid: 0x%04X\n", h7mem.pid);

	softio_blocking(read, h7sio, h7mem.filter.flag);
	h7mem.filter.flag |= DEMOD_DOWNSAMPLE_FLAG_OUTPUT_8BYTE;
	softio_blocking(write, h7sio, h7mem.filter.flag);
	// softio_blocking(read_between, h7sio, h7mem.siorx, h7mem);

	// this will show adc_data, but do not use like this!!!!
	// softio_blocking(read, h7sio, h7mem.adc_data);
	// printf("fifo_count of h7mem.adc_data = %u\n", fifo_count(&h7mem.adc_data));

	auto start = high_resolution_clock::now();
	size_t count = 0;
	while(1) {
		softio_delay_flush_try(read_fifo, h7sio, h7mem.adc_data);
		softio_delay_flush_try(read_fifo, h7sio, h7mem.adc_data);
		softio_delay_flush_try(read_fifo, h7sio, h7mem.adc_data);
		softio_delay_flush_try(read_fifo, h7sio, h7mem.adc_data);
		softio_delay_flush_try(read_fifo, h7sio, h7mem.adc_data);  // this will try handle message after flush current request
		auto now = high_resolution_clock::now();
		auto dura = duration_cast<duration<double>>(now - start);
		count += fifo_count(&h7mem.adc_data);
		fifo_clear(&h7mem.adc_data);
		// printf("dura.count() = %f\n", dura.count());
		if (dura.count() > 1) break;
		sleep_ms(1);
	}
	auto now = high_resolution_clock::now();
	auto dura = duration_cast<duration<double>>(now - start);
	printf("count = %d, speed = %fmbps\n", (int)count, count * 8.0 / 1e6 / dura.count());

	com->close();
}
