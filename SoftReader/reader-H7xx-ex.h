/*
 * This header library provides high-level functions to control reader-H7xx
 * take whatever you like and ignore others, by pre-define the function you need
 */

#define MEMORY_READER_H7XX
#define SOFTIO_USE_FUNCTION
#include "reader-H7xx.h"
#include "softio.h"
#include "assert.h"
#include "serial/serial.h"
#include <chrono>
#include <thread>
#include <string>
#include <mutex>
#include <vector>
#include <complex>
#include <queue>
#include "limits.h"
using namespace std;

#ifdef ReaderH7Host_DEFINATION
#undef ReaderH7Host_DEFINATION
struct ReaderH7Host_t {
	serial::Serial *com;
	bool verbose;
	string port;
	uint16_t pid;  // written after device is opened
	ReaderH7_Mem_t mem;
	SoftIO_t sio;
	mutex lock;
	ReaderH7Host_t();  // no parameter
	ReaderH7Host_t(const ReaderH7Host_t&);  // do not allow copy after com != NULL
	int open(const char* port);
	int close();
#define READERH7HOST_DUMP_FILTER 0x01
#define READERH7HOST_DUMP_FIFO 0x02
#define READERH7HOST_DUMP_TX 0x04
	int dump(int extra = 0);
	int set_dac_delay(uint16_t val);  // this will not set dac immediately
	int set_dac_volt_delay(float volt);
	float Absrx();  // get filtered signal strength
	float gain_ctrl(float sig_volt);  // high-level adjust of dac value
	/* Tx includes 3 timers: lptim1, lptim2 and tim4ch2
	 * lptim1 should be at 455kHz due to hardware constrain, however you can adjust the pulse width
	 * lptim2 is flexible at arbitrary frequency by adjust PLL3R, but ReaderH7 does NOT provide this API
	 * lptim2 and tim4 is configured to 200MHz frequency, and you could adjust the period and pulse width of PWM wave
	 * tim2 is for low speed interrupt which send tx data
	 */
	pair<float, float> set_lptim1(float frequency, float duty);  // 455kHz suggested
	pair<float, float> set_lptim2(float frequency, float duty);
	pair<float, float> set_tim4(float frequency, float duty);
	float set_tx_sample_rate(float frequency);  // set tx sample rate
	int set_tx_default_sample(Tx_Sample_t default_sample);

	// low-level api for sample receiving and bit stream transmitting
	volatile bool rx_running;
	thread rx_thread;
	char rx_data_buf[4194304];  // about 9 second, 2^24 > 455000 * 9
	Fifo_t rx_data;
	char current_rx_data[sizeof(ReaderH7_Mem_t::adc_data_buf)];
	function<void(const filter_out_t* buffer, int length)> rx_data_callback;
	volatile bool output_to_rx_data;
	int start_rx_receiving();
	vector<filter_out_t> wait_rx_receiving(int samples2recv);
	void wait_rx_receiving(filter_out_t* buffer, int samples2recv, int sleepms = 10);
	int stop_rx_receiving();
	volatile uint32_t tx_count;
	volatile uint32_t tx_underflow;
	volatile bool tx_async_started;
	mutex tx_lock;
	int tx_send_samples_async(const vector<Tx_Sample_t>& samples);  // this is thread safe
	int tx_send_samples_wait();
	int tx_send_samples(const vector<Tx_Sample_t>& samples);  // this is blocking API

	// high-level api for preamble listening and packet tx
	vector< complex<float> > refpreamble;
	int load_refpreamble(const char* filename);
	static int load_refpreamble(vector< complex<float> >& ref, const char* filename);
	int load_refpreamble(const void* buffer, size_t buflen);
	static int load_refpreamble(vector< complex<float> >& ref, const void* buffer, size_t buflen);
	volatile bool preamble_running;
	thread preamble_thread;
	vector< complex<float> > result_preamble;
	volatile float snr_preamble;
	float preamble_data_buf[32768];
	int start_preamble_receiving(int sample2recv, float snr, bool close_rx_receiving_when_completed = false);  // this will call start_rx_receiving() internally
	int wait_preamble_for(float seconds);  // return 0 if success, otherwise timeout. -1 means forever, but not recommended in production environment
	int stop_preamble_receiving(bool if_stop_rx_receiving = true);  // this will call stop_preamble_receiving() internally
};
#endif

#ifdef ReaderH7Host_IMPLEMENTATION
#undef ReaderH7Host_IMPLEMENTATION

#include "preamble.h"
#include "sysutil.h"
#include <fstream>
#include <iterator>

ReaderH7Host_t::ReaderH7Host_t(): com(NULL) {
	verbose = false;
	rx_running = false;
	tx_async_started = false;
	fifo_init(&rx_data, rx_data_buf, sizeof(rx_data_buf));
	preamble_running = false;
	output_to_rx_data = true;
}

ReaderH7Host_t::ReaderH7Host_t(const ReaderH7Host_t& host) {
	assert(host.com == NULL && "opened host is not allowed to copy");
	com = host.com;
	verbose = host.verbose;
	rx_running = host.rx_running;
	tx_async_started = host.tx_async_started;
	fifo_init(&rx_data, rx_data_buf, sizeof(rx_data_buf));
	preamble_running = host.preamble_running;
	output_to_rx_data = host.output_to_rx_data;
	rx_data_callback = host.rx_data_callback;
}

int ReaderH7Host_t::open(const char* _port) {
	assert(com == NULL && "device has been opened");
	// open com port
	lock.lock();
	port = _port;
	if (verbose) printf("opening device \"%s\"\n", port.c_str());
	com = new serial::Serial(port.c_str(), 115200, serial::Timeout::simpleTimeout(1000));
	assert(com->isOpen() && "port is not opened");
	while (com->available()) { uint8_t _c; com->read(&_c, 1); }  // clear the buffer of com
	// setup softio controller
	SOFTIO_QUICK_INIT(sio, mem, ReaderH7_Mem_FifoInit);
	sio.gets = [&](char *buffer, size_t size)->size_t {
		size_t s = com->read((uint8_t*)buffer, size);
		com->flush();
		return s;
	};
	sio.puts = [&](char *buffer, size_t size)->size_t {
		size_t s = com->write((uint8_t*)buffer, size);
		com->flush();
		return s;
	};
	sio.available = [&]()->size_t {
		return com->available();
	};
	sio.callback = [&](void* softio, SoftIO_Head_t* head)->void {
		assert(softio);
		if (head->type == SOFTIO_HEAD_TYPE_READ) {  // read response
			if (softio_is_variable_included(sio, *head, mem.tx.count)) tx_count = mem.tx.count;
			if (softio_is_variable_included(sio, *head, mem.tx.underflow)) tx_underflow = mem.tx.underflow;
		}
	};
	// initialize device and verify it
	softio_blocking(read, sio, mem.version);
	assert(mem.version == MEMORY_READER_H7XX_VERSION && "version not match");
	softio_blocking(read, sio, mem.mem_size);
	assert(mem.mem_size == sizeof(mem) && "memory size not equal, this should NOT occur");
	softio_blocking(read, sio, mem.pid);
	pid = mem.pid;
	lock.unlock();
	return 0;
}

int ReaderH7Host_t::close() {
	assert(com && "device not opened");
	tx_send_samples_wait();
	stop_rx_receiving();
	lock.lock();
	softio_wait_all(sio);
	com->close();
	delete com;
	com = NULL;
	lock.unlock();
	return 0;
}

int ReaderH7Host_t::dump(int extra) {
	lock.lock();
	printf("--- reading %d bytes ", (int)((char*)(void*)(&(mem.guard_small_varibles)) - (char*)(void*)(&(mem.command)) + sizeof(mem.guard_small_varibles)));
	su_time(softio_blocking, read_between, sio, mem.command, mem.guard_small_varibles);
	printf("[basic information]\n");
	printf("  1. command: 0x%02X [%s]\n", mem.command, COMMAND_STR(mem.command));
	printf("  2. flags: 0x%02X [", mem.flags);
		if (mem.flags & FLAG_MASK_HALT) printf(" FLAG_MASK_HALT");
		if (mem.flags & FLAG_MASK_DISABLE_UART3_OUT) printf(" FLAG_MASK_DISABLE_UART3_OUT");
		printf(" ]\n");
	printf("  3. verbose_level: 0x%02X\n", mem.verbose_level);
	printf("  4. mode: 0x%02X [%s]\n", mem.mode, MODE_STR(mem.mode));
	printf("  5. status: 0x%02X [%s]\n", mem.status, STATUS_STR(mem.status));
	printf("  6. pid: 0x%04X\n", mem.pid);
	printf("  7. version: 0x%08X\n", mem.version);
	printf("  8. mem_size: %d\n", (int)mem.mem_size);
	printf("  9. dac_set: %d, dac_now: %d\n", (int)mem.dac_set, (int)mem.dac_now);
	printf("[statistics information]\n");
	printf("  1. siorx_overflow: %u\n", (unsigned)mem.siorx_overflow);
	printf("  2. siotx_overflow: %u\n", (unsigned)mem.siotx_overflow);
	printf("  3. usart3_tx_overflow: %u\n", (unsigned)mem.usart3_tx_overflow);
	printf("  4. adc_data_overflow: %u\n", (unsigned)mem.adc_data_overflow);
if (extra & READERH7HOST_DUMP_FILTER) {
	printf("--- reading %d bytes ", (int)sizeof(mem.filter));
	su_time(softio_blocking, read, sio, mem.filter);
	printf("[Filter Information]\n");
	printf("  1. Ia_out: %hd\n", mem.filter.Ia_out);
	printf("  2. Qa_out: %hd\n", mem.filter.Qa_out);
	printf("  3. Ib_out: %hd\n", mem.filter.Ib_out);
	printf("  4. Qb_out: %hd\n", mem.filter.Qb_out);
	printf("  5. Ia[0~16]: "); for (int i=0; i<16; ++i) printf("%hd ", mem.filter.Ia[i]); printf("\n");
	printf("  6. Qa[0~16]: "); for (int i=0; i<16; ++i) printf("%hd ", mem.filter.Qa[i]); printf("\n");
	printf("  7. Ib[0~16]: "); for (int i=0; i<16; ++i) printf("%hd ", mem.filter.Ib[i]); printf("\n");
	printf("  8. Qb[0~16]: "); for (int i=0; i<16; ++i) printf("%hd ", mem.filter.Qb[i]); printf("\n");
	printf("[Application Information]\n");
	printf("  1. adc_data_push_packets_count: %d\n", mem.adc_data_push_packets_count);
}
if (extra & READERH7HOST_DUMP_FIFO) {
	printf("[Fifo Information]\n");
	softio_protected_dump_remote_fifo("  1. ", sio, mem.usart3_tx);
	softio_protected_dump_remote_fifo("  2. ", sio, mem.testio);
	softio_protected_dump_remote_fifo("  3. ", sio, mem.adc_data);
}
if (extra & READERH7HOST_DUMP_TX) {
	printf("[Tx Information]\n");
	su_time(softio_blocking, read, sio, mem.tx);
	printf("  1. lptim1: period = %d, pulse = %d\n", (int)mem.tx.period_lptim1, (int)mem.tx.pulse_lptim1);
	float freq_lptim1 = 29.12e6 / ((float)mem.tx.period_lptim1 + 1);
	float duty_lptim1 = freq_lptim1 / 29.12e6 * ((float)mem.tx.pulse_lptim1 + 1);
	printf("             frequency = %f kHz, duty = %f%%\n", freq_lptim1/1000, duty_lptim1*100);
	printf("  2. lptim2: period = %d, pulse = %d\n", (int)mem.tx.period_lptim2, (int)mem.tx.pulse_lptim2);
	float freq_lptim2 = 200e6 / ((float)mem.tx.period_lptim2 + 1);
	float duty_lptim2 = freq_lptim2 / 200e6 * ((float)mem.tx.pulse_lptim2 + 1);
	printf("             frequency = %f kHz, duty = %f%%\n", freq_lptim2/1000, duty_lptim2*100);
	printf("  3. tim4: period = %d, pulse = %d\n", (int)mem.tx.period_tim4, (int)mem.tx.pulse_tim4);
	float freq_tim4 = 200e6 / ((float)mem.tx.period_tim4 + 1);
	float duty_tim4 = freq_tim4 / 200e6 * ((float)mem.tx.pulse_tim4 + 1);
	printf("             frequency = %f kHz, duty = %f%%\n", freq_tim4/1000, duty_tim4*100);
	printf("  3. sample_rate: period = %d\n", (int)mem.tx.period_tim2);
	float freq_tim2 = 200e6 / ((float)mem.tx.period_tim2 + 1);
	printf("             frequency = %f kHz\n", freq_tim2/1000);
}
	lock.unlock();
	return 0;
}

int ReaderH7Host_t::set_dac_delay(uint16_t val) {
	lock.lock();
	mem.dac_set = val;
	softio_delay_flush_try(write, sio, mem.dac_set);
	lock.unlock();
	return 0;
}

int ReaderH7Host_t::set_dac_volt_delay(float volt) {
	volt = min(max(volt, 0.f), 2.0f);
	return set_dac_delay(volt / 3.3 * 65536);
}

float ReaderH7Host_t::Absrx() {
	lock.lock();
	softio_blocking(read_between, sio, mem.filter.Ia_out, mem.filter.Qb_out);
	lock.unlock();
	int u1 = mem.filter.Ia_out * (int)mem.filter.Ia_out + mem.filter.Qa_out * (int)mem.filter.Qa_out;
	int u2 = mem.filter.Ib_out * (int)mem.filter.Ib_out + mem.filter.Qb_out * (int)mem.filter.Qb_out;
	int energy = u1 > u2 ? u1 : u2;
	float ret = sqrtf(energy) / 32768.f * 3.3f;
	return ret;
}

float ReaderH7Host_t::gain_ctrl(float sig_volt) {
	float dac_min(0), dac_max(2);
	set_dac_volt_delay(dac_max);
	this_thread::sleep_for(10ms);  // wait for dac set and filter response
	float val = Absrx();
	if (val < sig_volt) {
		printf("WARN: gain not achievable, now signal %f\n", val);
		return dac_max;
	}
	float mid = 0;
	while (dac_max - dac_min > 0.02)
	{
		mid = (dac_max + dac_min) / 2;
		set_dac_volt_delay(mid);
		this_thread::sleep_for(10ms);  // wait for dac set and filter response
		val = Absrx();
		if (val < sig_volt) {
			dac_min = mid;
		} else {
			dac_max = mid;
		}
	}
	set_dac_volt_delay(mid); 
	this_thread::sleep_for(10ms);  // wait for dac set and filter response
	return mid;
}

void __rx_receiving_thread(ReaderH7Host_t* ptr, int restore_count) {  // request specified packets per second
	int count = 0;
	auto last_count = chrono::high_resolution_clock::now();
	auto start = chrono::high_resolution_clock::now();
	ReaderH7Host_t& host = *ptr;
	while (host.rx_running) {
		auto now = chrono::high_resolution_clock::now();
		double dura = chrono::duration_cast<chrono::duration<double>>(now - start).count();
		host.lock.lock();
		while (host.sio.available() > __FIFO_GET_LENGTH(host.sio.rx))
			softio_flush_try_handle_all(host.sio);
		if (dura > 0.005) {  // restore count every 5ms, I don't know why but if PC doesn't send data, MCU will only have <1Mbps throughput...... FUCK
			host.mem.adc_data_push_packets_count = restore_count;
			softio_delay_flush_try(write, host.sio, host.mem.adc_data_push_packets_count);
			start = chrono::high_resolution_clock::now();
			softio_delay(read, host.sio, host.mem.adc_data_overflow);  // read it here
			if (host.mem.adc_data_overflow) {  // may not get the value immediately but that's OK
				printf("error: mem.adc_data_overflow = %d\n", (int)host.mem.adc_data_overflow);
			} assert(host.mem.adc_data_overflow == 0);
		}
		// printf("delta: %d\n", target_packet - packet);
		// if ((target_packet - packet) * 254 > sizeof(ReaderH7_Mem_t::adc_data_buf)) {
		// 	printf("warning: possible buffer overflow may occur, due to system heavy loaded\n");
		// }
		uint32_t hascnt = (fifo_count(&host.mem.adc_data) / 8) * 8;  // 8byte aligned
		if (hascnt) {
			count += hascnt;
			assert(fifo_move_to_buffer(host.current_rx_data, &host.mem.adc_data, hascnt) == hascnt);
			if (host.rx_data_callback) host.rx_data_callback((filter_out_t*)host.current_rx_data, hascnt/8);  // call hook function here
			if (host.output_to_rx_data) {
				assert(fifo_copy_from_buffer(&host.rx_data, host.current_rx_data, hascnt) == hascnt && "local fifo overflow");  // local fifo overflow
			}
		}
		// packet = target_packet;
		host.lock.unlock();
		double count_dura = chrono::duration_cast<chrono::duration<double>>(now - last_count).count();
		if (count_dura > 1) {
#ifdef TEST_MCU_STREAM_VERBOSE_SPEED
			printf("count = %d, speed = %fmbps\n", count, count * 8.0 / 1e6 / count_dura);
#endif
			count = 0;
			last_count = now;
		}
		this_thread::sleep_for(1ms);  // 1ms
	}
	// stop receiving
	printf("stop receiving\n");
	host.lock.lock();
	softio_delay(read, host.sio, host.mem.adc_data_overflow);  // get overflow count here. it should be 0 at this moment
	host.mem.filter.flag &= ~DEMOD_DOWNSAMPLE_FLAG_OUTPUT_8BYTE;
	softio_blocking(write, host.sio, host.mem.filter.flag);
	host.mem.adc_data_push_packets_count = 0;
	softio_blocking(write, host.sio, host.mem.adc_data_push_packets_count);
	// printf("mem.adc_data_overflow = %d\n", (int)host.mem.adc_data_overflow);
	if (host.mem.adc_data_overflow) {
		printf("error: mem.adc_data_overflow = %d\n", (int)host.mem.adc_data_overflow);
	} assert(host.mem.adc_data_overflow == 0);
	softio_blocking(reset_fifo, host.sio, host.mem.adc_data);
	host.lock.unlock();
	fifo_clear(&host.mem.adc_data);
}

int ReaderH7Host_t::start_rx_receiving() {
	assert(!rx_thread.joinable() && "thread exists");
	// reset rx
	lock.lock();
	softio_blocking(read, sio, mem.filter.flag);
	mem.filter.flag &= ~DEMOD_DOWNSAMPLE_FLAG_OUTPUT_8BYTE;
	softio_blocking(write, sio, mem.filter.flag);  // remote stop output to fifo
	softio_blocking(reset_fifo, sio, mem.adc_data);  // reset remote fifo
	mem.adc_data_overflow = 0;
	softio_blocking(write, sio, mem.adc_data_overflow);  // clear overflow count
	mem.adc_data_push_packets_count = 56875*8/254/2;  // 500ms tolerance
	softio_blocking(write, sio, mem.adc_data_push_packets_count);
	fifo_clear(&mem.adc_data);  // clear local fifo
	mem.filter.flag |= DEMOD_DOWNSAMPLE_FLAG_OUTPUT_8BYTE;
	softio_blocking(write, sio, mem.filter.flag);  // remote restart output to fifo
	// do not use query // for (int i=0; i<8; ++i) softio_delay_flush_try(read_fifo, sio, mem.adc_data);  // buffer 8 requests
	lock.unlock();
	// then start new thread for retreiving data
	rx_running = true;  // must set this before starting thread, to avoid unexpected situation
	rx_thread = thread(__rx_receiving_thread, this, mem.adc_data_push_packets_count);
	return 0;
}

int ReaderH7Host_t::stop_rx_receiving() {
	if (rx_running) {
		rx_running = false;
		if (rx_thread.joinable()) rx_thread.join();
	}
	return 0;
}

vector<filter_out_t> ReaderH7Host_t::wait_rx_receiving(int samples2recv) {
	vector<filter_out_t> filter_out;
	filter_out.resize(samples2recv);
	wait_rx_receiving(filter_out.data(), samples2recv, 500);  // 500 ms interval is enough
	return filter_out;
}

void ReaderH7Host_t::wait_rx_receiving(filter_out_t* buffer, int samples2recv, int sleepms) {
	char* buf = (char*)(void*)buffer;  // get raw data, it is tranmistted in low-endian
	int byte2recv = samples2recv * 8;
	assert(byte2recv / 8 == samples2recv && "overflow occured");
	int bias = 0;
	while (byte2recv) {
		int got = fifo_move_to_buffer(buf + bias, &rx_data, byte2recv);  // get at most byte2recv bytes
		byte2recv -= got;
		bias += got;
		if (verbose) printf("got %d bytes, remain %d bytes\n", got, byte2recv);
		if (byte2recv) std::this_thread::sleep_for(std::chrono::milliseconds(sleepms));
	}
}

int ReaderH7Host_t::load_refpreamble(const char* filename) {
	return load_refpreamble(refpreamble, filename);
}

int ReaderH7Host_t::load_refpreamble(vector< complex<float> >& ref, const char* filename) {
	ifstream input(filename, ios::binary);
	vector<unsigned char> buffer(istreambuf_iterator<char>(input), {});
	printf("refpreamble file \"%s\" length: %d\n", filename, (int)buffer.size());
	return load_refpreamble(ref, (void*)buffer.data(), buffer.size());
}

int ReaderH7Host_t::load_refpreamble(const void* buffer, size_t buflen) {
	return load_refpreamble(refpreamble, buffer, buflen);
}

int ReaderH7Host_t::load_refpreamble(vector< complex<float> >& ref, const void* buffer, size_t buflen) {
	int refsize = buflen / 2 / sizeof(float);
	ref.resize(refsize);
	memcpy((void*)ref.data(), buffer, buflen);
	return 0;
}

int ReaderH7Host_t::start_preamble_receiving(int sample2recv, float snr, bool close_rx_receiving_when_completed) {
	if (!rx_running) start_rx_receiving();
	assert(refpreamble.size() && "no preamble ref provided");
	result_preamble.clear();
	result_preamble.resize(sample2recv);
	snr_preamble = snr;
	preamble_running = true;
	preamble_thread = thread(preamble_rx, sample2recv, (float*)result_preamble.data(), &snr_preamble, 
		refpreamble.size(), (float*)refpreamble.data(), [&](size_t requested_4float)->float* {
			size_t n = requested_4float * 4;  // float count
			short* rsv = (short*)&preamble_data_buf[n] - n;  // about the middle
			wait_rx_receiving((filter_out_t*)rsv, requested_4float);
			for (size_t i = 0; i < n; ++i) preamble_data_buf[i] = float(rsv[i]);
			return preamble_data_buf;
		}, sizeof(preamble_data_buf) / 4, &preamble_running, close_rx_receiving_when_completed ? &output_to_rx_data : NULL);
	while (snr_preamble) {  // wait for thread open
		this_thread::sleep_for(10ms);
	}
	return 0;
}

int ReaderH7Host_t::wait_preamble_for(float seconds) {
	assert(preamble_running);  // must be running now
	auto start = chrono::high_resolution_clock::now();
	while (1) {
		if (snr_preamble) return 0;
		auto now = chrono::high_resolution_clock::now();
		double dura = chrono::duration_cast<chrono::duration<double>>(now - start).count();
		if (seconds >= 0 && dura >= seconds) return -1;  // timeout
		this_thread::sleep_for(10ms);
	}
	return 0;
}

int ReaderH7Host_t::stop_preamble_receiving(bool if_stop_rx_receiving) {
	if (preamble_running) {
		preamble_running = false;
		if (preamble_thread.joinable()) preamble_thread.join();
	}
	if (if_stop_rx_receiving) stop_rx_receiving();
	return 0;
}

#define __READER_H7_SET_TIM_COMPUTE(clock) \
	float period_target = clock / frequency - 0.5; \
	assert(period_target >= 0 && period_target < 65536 && "invalid frequency"); \
	uint16_t period = period_target; \
	float frequency_real = clock / ((float)period + 1); \
	float pulse_target = clock / frequency_real * duty - 0.5; \
	uint16_t pulse = pulse_target; \
	float duty_real = frequency_real / clock * ((float)pulse + 1) ; \

pair<float, float> ReaderH7Host_t::set_lptim1(float frequency, float duty) {
	__READER_H7_SET_TIM_COMPUTE(29.12e6);
	lock.lock();
	mem.tx.period_lptim1 = period;
	mem.tx.pulse_lptim1 = pulse;
	softio_blocking(write_between, sio, mem.tx.period_lptim1, mem.tx.pulse_lptim1);
	lock.unlock();
	return make_pair(frequency_real, duty_real);
}

pair<float, float> ReaderH7Host_t::set_lptim2(float frequency, float duty) {
	__READER_H7_SET_TIM_COMPUTE(200e6);
	lock.lock();
	mem.tx.period_lptim2 = period;
	mem.tx.pulse_lptim2 = pulse;
	softio_blocking(write_between, sio, mem.tx.period_lptim2, mem.tx.pulse_lptim2);
	lock.unlock();
	return make_pair(frequency_real, duty_real);
}

pair<float, float> ReaderH7Host_t::set_tim4(float frequency, float duty) {
	__READER_H7_SET_TIM_COMPUTE(200e6);
	lock.lock();
	mem.tx.period_tim4 = period;
	mem.tx.pulse_tim4 = pulse;
	softio_blocking(write_between, sio, mem.tx.period_tim4, mem.tx.pulse_tim4);
	lock.unlock();
	return make_pair(frequency_real, duty_real);
}

float ReaderH7Host_t::set_tx_sample_rate(float frequency) {
	float period_target = 200e6 / frequency - 0.5;
	assert(period_target >= 0 && period_target <= UINT_MAX && "invalid frequency");
	uint32_t period = period_target;
	float frequency_real = 200e6 / (period + 1.f);
	lock.lock();
	mem.tx.period_tim2 = period;
	softio_blocking(write, sio, mem.tx.period_tim2);
	lock.unlock();
	return frequency_real;
}

int ReaderH7Host_t::set_tx_default_sample(Tx_Sample_t default_sample) {
	mem.tx.default_sample = default_sample;
	lock.lock();
	softio_blocking(write, sio, mem.tx.default_sample);
	lock.unlock();
	return 0;
}

int ReaderH7Host_t::tx_send_samples_async(const vector<Tx_Sample_t>& samples) {
	if (samples.size() == 0) return 0;
	assert(samples.size() < mem.tx_data.Length() && "long tx not implemented");
	tx_send_samples_wait();  // wait only if previous tx is async
	tx_lock.lock();
	lock.lock();
	mem.tx.count = 0;
	mem.tx.underflow = 0;
	softio_delay(write, sio, mem.tx.count);  // first disable previous tx output
	softio_delay(write, sio, mem.tx.underflow);
	mem.tx.count = samples.size();
	fifo_clear(&mem.tx_data);  // clear local buffer
	softio_delay_flush_try(clear_fifo, sio, mem.tx_data);  // buffer clear request
	lock.unlock();
	for (int i=0; i<(int)samples.size(); ++i) fifo_enque(&mem.tx_data, *(char*)&samples[i]);
	bool writecount = true;
	while (!fifo_empty(&mem.tx_data)) {
		int partlen = fifo_count(&mem.tx_data);
		if (partlen > 254) partlen = 254;
		lock.lock();
		softio_delay_flush_try(write_fifo_part, sio, mem.tx_data, partlen);
		if (writecount) {  // to optimize tx delay
			softio_delay_flush_try(write, sio, mem.tx.count);
			writecount = false;
		}
		lock.unlock();
		int sleepcnt = 0;
		while (softio_buffered_count(sio) > sio.length / 2) {  // do not fullfill the transaction buffer
			this_thread::sleep_for(1ms);  // for 10kS/s Tx, 10ms is just one packet so that is OK
			++sleepcnt;
			if (sleepcnt > 10) {
				sleepcnt = 0;
				printf("tx wait too long ( > 10ms ), may be system overloaded\n");
			}
		}
	}
	// push data done
	tx_async_started = true;
	tx_lock.unlock();
	return 0;
}

int ReaderH7Host_t::tx_send_samples_wait() {
	tx_lock.lock();
	if (tx_async_started) {
		while (1) {
			tx_count = -1;
			lock.lock();
			softio_delay_flush_try(read, sio, mem.tx.count);
			lock.unlock();
			wait_variable_not_minus_1(sio, tx_count, lock);
			// printf("tx_count = %d\n", tx_count);
			if (tx_count == 0) break;
		}
		tx_underflow = -1;
		lock.lock();
		softio_delay_flush_try(read, sio, mem.tx.underflow);  // read it here
		lock.unlock();
		wait_variable_not_minus_1(sio, tx_underflow, lock);
		if (mem.tx.underflow) {
			printf("error: mem.tx.underflow = %d\n", (int)mem.tx.underflow);
		} assert(mem.tx.underflow == 0);
		tx_async_started = false;
	}
	tx_lock.unlock();
	return 0;
}

int ReaderH7Host_t::tx_send_samples(const vector<Tx_Sample_t>& samples) {
	tx_send_samples_async(samples);
	tx_send_samples_wait();
	return 0;
}

#endif
