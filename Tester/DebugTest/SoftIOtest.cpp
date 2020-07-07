#include "stdio.h"
#include "softio.h"
#include "assert.h"

struct TestMemory_t {
	uint8_t a;
	uint8_t b;
	uint16_t c;
	uint32_t d;

	char siorx_buf[2048];
	char siotx_buf[2048];
	char data_buf[1024];
#define TestMemory_FifoInit(mem) do {\
	FIFO_STD_INIT(mem, siorx);\
	FIFO_STD_INIT(mem, siotx);\
	FIFO_STD_INIT(mem, data);\
} while(0)
	Fifo_t siorx;  // must be the first 
	Fifo_t siotx;
	Fifo_t data;
};

TestMemory_t pc;
TestMemory_t mcu;
SoftIO_t pcio;
SoftIO_t mcuio;

int main() {
	uint32_t tmp;
	
	printf("\n1. initialize fifo and test it:\n");
	TestMemory_FifoInit(pc);
	TestMemory_FifoInit(mcu);
	fifo_dump(pc.siorx);
	fifo_dump(pc.siotx);
	assert(pc.siorx.Length() == sizeof(pc.siorx_buf));
	fifo_dump(mcu.siorx);
	fifo_dump(mcu.siotx);
	assert(mcu.siorx.Length() == sizeof(mcu.siorx_buf));

	printf("\n2. mcu generate data, test fifo enque:\n");
	uint32_t data2push = 20;
	for (uint32_t i=0; i<data2push; ++i) fifo_enque(&mcu.data, i);
	fifo_dump(mcu.data);
	assert(fifo_count(&mcu.data) == data2push);

	printf("\n3. initialize softio:\n");
	softio_init(&pcio, &pc, sizeof(pc), &pc.siorx, &pc.siotx);
	softio_init(&mcuio, &mcu, sizeof(mcu), &mcu.siorx, &mcu.siotx);
	softio_dump(pcio);
	softio_dump(mcuio);

	printf("\n4. test simple memory read:\n");
	mcu.a = 0x0A;
	mcu.b = 0x0B;
	mcu.c = 0x0C;
	mcu.d = 0x0D;
	softio_delay_read(pcio, pc.a);
	softio_delay_read_between(pcio, pc.b, pc.d);
	softio_dump(pcio);
	tmp = fifo_count(&pc.siotx);
	fifo_move(&mcu.siorx, &pc.siotx, -1);
	fifo_dump(mcu.siorx);
	assert((uint32_t)fifo_count(&mcu.siorx) == tmp);
	softio_try_handle_all(mcuio);
	fifo_move(&pc.siorx, &mcu.siotx, -1);
	fifo_dump(pc.siorx);
	softio_try_handle_all(pcio);
	assert(pc.a == mcu.a);
	assert(pc.b == mcu.b);
	assert(pc.c == mcu.c);
	assert(pc.d == mcu.d);

	printf("\n5. test simple memory write:\n");
	pc.a = 0xAA;
	pc.b = 0xBB;
	pc.c = 0xCCCC;
	pc.d = 0xDDDDDDDD;
	softio_delay_write(pcio, pc.a);
	softio_delay_write_between(pcio, pc.b, pc.d);
	softio_dump(pcio);
	tmp = fifo_count(&pc.siotx);
	fifo_move(&mcu.siorx, &pc.siotx, -1);
	fifo_dump(mcu.siorx);
	assert((uint32_t)fifo_count(&mcu.siorx) == tmp);
	softio_try_handle_all(mcuio);
	fifo_move(&pc.siorx, &mcu.siotx, -1);
	fifo_dump(pc.siorx);
	softio_try_handle_all(pcio);
	assert(pc.a == mcu.a);
	assert(pc.b == mcu.b);
	assert(pc.c == mcu.c);
	assert(pc.d == mcu.d);

	printf("\n6. test fifo read:\n");
	tmp = fifo_count(&mcu.data);
	softio_delay_read_fifo(pcio, pc.data);
	softio_dump(pcio);
	fifo_move(&mcu.siorx, &pc.siotx, -1);
	fifo_dump(mcu.siorx);
	softio_try_handle_all(mcuio);
	fifo_move(&pc.siorx, &mcu.siotx, -1);
	fifo_dump(pc.siorx);
	softio_try_handle_all(pcio);
	assert(fifo_count(&pc.data) == tmp);
	assert(fifo_count(&mcu.data) == 0);
	fifo_dump(pc.data);

	printf("\n7. test fifo write:\n");
	tmp = fifo_count(&pc.data);
	softio_delay_write_fifo(pcio, pc.data);
	softio_dump(pcio);
	fifo_move(&mcu.siorx, &pc.siotx, -1);
	fifo_dump(mcu.siorx);
	softio_try_handle_all(mcuio);
	fifo_move(&pc.siorx, &mcu.siotx, -1);
	fifo_dump(pc.siorx);
	softio_try_handle_all(pcio);
	assert(fifo_count(&pc.data) == 0);
	assert(fifo_count(&mcu.data) == tmp);
	fifo_dump(mcu.data);

	printf("\n8. test fifo clear and reset:\n");
	tmp = mcu.data.write;
	assert(mcu.data.read != mcu.data.write);  // fifo not empty
	softio_delay_clear_fifo(pcio, pc.data);
	softio_dump(pcio);
	fifo_move(&mcu.siorx, &pc.siotx, -1);
	fifo_dump(mcu.siorx);
	softio_try_handle_all(mcuio);
	fifo_move(&pc.siorx, &mcu.siotx, -1);
	fifo_dump(pc.siorx);
	softio_try_handle_all(pcio);
	assert(mcu.data.read == mcu.data.write);
	assert(mcu.data.read == tmp);
	fifo_dump(mcu.data);

	softio_delay_reset_fifo(pcio, pc.data);
	softio_dump(pcio);
	fifo_move(&mcu.siorx, &pc.siotx, -1);
	fifo_dump(mcu.siorx);
	softio_try_handle_all(mcuio);
	fifo_move(&pc.siorx, &mcu.siotx, -1);
	fifo_dump(pc.siorx);
	softio_try_handle_all(pcio);
	assert(fifo_count(&mcu.data) == 0);
	fifo_dump(mcu.data);

}
