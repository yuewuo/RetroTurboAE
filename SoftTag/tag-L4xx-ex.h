/*
 * This header library provides high-level functions to control tag-L4xx
 * take whatever you like and ignore others, by pre-define the function you need
 */

#define MEMORY_TAG_L4XX
#define SOFTIO_USE_FUNCTION
#include "tag-L4xx.h"
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

#ifdef TagL4Host_DEFINATION
#undef TagL4Host_DEFINATION
struct TagL4Host_t {
	serial::Serial *com;
	bool verbose;
	string port;
	uint16_t pid;  // written after device is opened
	TagL4_Mem_t mem;
	SoftIO_t sio;
	mutex lock;
	TagL4Host_t();  // no parameter
	TagL4Host_t(const TagL4Host_t&);  // do not allow copy after com != NULL
	int open(const char* port);
	int close();
#define TAGL4HOST_DUMP_TX 0x01
	int dump(int extra = 0);
	float set_tx_sample_rate(float frequency);  // set tx sample rate
	int set_tx_default_sample(Tag_Sample_t default_sample);

// low level API for tx sending, with callback function
	volatile uint32_t tx_count;
	volatile uint32_t tx_underflow;
	volatile bool tx_running;
	thread tx_thread;
	mutex tx_lock;
	queue<Tag_Sample_t> tx_samples;  // must lock tx_lock before read/write
	void tx_send_samples_background_thread(size_t sent_cnt, size_t count, function<void(Tag_Sample_t*, size_t)>);
	float tx_send_samples_start(float frequency, size_t count = UINT_MAX, function<void(Tag_Sample_t*, size_t)> = NULL);  // start new thread for sending
	int tx_send_samples_append(const vector<Tag_Sample_t>& samples);
	int tx_send_samples_wait();
	int tx_send_samples_stop();

// high level API for tx sending, blocking API
	float tx_send_samples(float frequency, const vector<Tag_Sample_t>& samples);
	float tx_send_compressed(float frequency, const vector<string>& compressed, int& length);

	float tx_repeat_samples(float frequency, const vector<Tag_Sample_t>& samples, int NLCD, int count, int interval);
	float tx_repeat_compressed(float frequency, const vector<string>& samples, int NLCD, int count, int interval, int& length);

};
#endif


#ifndef TAG_HELPER_FUNCTIONS
#define TAG_HELPER_FUNCTIONS

static inline Tag_Sample_t tag_invert_sample_from_str(string line) {  // ignore other char
	vector<char> chars;
	for (size_t i=1; i<line.size(); i+=2) {
		char H = line[i-1], L = line[i];
#define IS_HEX_VALID_CHAR_TMP(x) ((x>='0'&&x<='9')||(x>='a'&&x<='z')||(x>='A'&&x<='Z'))
		if (!IS_HEX_VALID_CHAR_TMP(H) || !IS_HEX_VALID_CHAR_TMP(L)) break;  // ignore
#undef ASSERT_HEX_VALID_CHAR_TMP
#define c2bs(x) (x>='0'&&x<='9'?(x-'0'):((x>='a'&&x<='f')?(x-'a'+10):((x>='A'&&x<='F')?(x-'A'+10):(0))))
		char b = (c2bs(H) << 4) | c2bs(L);
#undef c2bs
		chars.push_back(b);
	}
	Tag_Sample_t sample;
	auto it = chars.rbegin();
	for (int i=TAG_L4XX_SAMPLE_BYTE-1; i>=0 && it != chars.rend(); --i, ++it) {
		sample.s[i] = *it;
	}
	return sample;
}

static inline vector<pair<Tag_Sample_t, int>> compressed_string_to_compressed_samples(const vector<string>& compressed) {
	vector<pair<Tag_Sample_t, int>> compressed_samples;
	for (size_t i=0; i<compressed.size(); ++i) {
		string line = compressed[i];
		int cnt = 1;
		if (line.find(':') != string::npos) {
			cnt = atoi(line.c_str() + line.find(':') + 1);
		}
		compressed_samples.push_back(make_pair(tag_invert_sample_from_str(line), cnt));  // the sample is inverted so that shorter string could also work well
	}  // this part tested
	return compressed_samples;
}

static inline vector<Tag_Sample_t> compressed_string_to_samples(const vector<string>& compressed) {
	vector<pair<Tag_Sample_t, int>> compressed_samples = compressed_string_to_compressed_samples(compressed);
	vector<Tag_Sample_t> samples;
	for (auto it=compressed_samples.begin(); it!=compressed_samples.end(); ++it) {
		samples.insert(samples.end(), it->second, it->first);  
	}
	return samples;
}

static inline vector<string> compressed_samples_to_compressed_string(const vector<pair<Tag_Sample_t, int>>& samples, int NLCD) {
	vector<string> ret;
	for (auto it=samples.begin(); it!=samples.end(); ++it) {
		const Tag_Sample_t& last = it->first;
		string line;
		for (int j=0; j<NLCD; ++j) {
			char strbuf[3]; sprintf(strbuf, "%02X", (uint8_t)last.le(j)); line = strbuf + line;
		}
		{ char strbuf[16]; sprintf(strbuf, ":%d", it->second); line += strbuf;}
		ret.push_back(line);
	}
	return ret;
}

static inline vector<string> samples_to_compressed_string(const vector<Tag_Sample_t>& samples, int NLCD) {
	vector<string> ret;
	if (!samples.empty()) {
		Tag_Sample_t last = samples[0];
		int equcnt = 0;
		for (size_t i=0; i<samples.size(); ++i) {
			if (last.equal(samples[i], NLCD)) {
				equcnt += 1;
			} else {
				string line;
				for (int j=0; j<NLCD; ++j) { 
					char strbuf[3]; sprintf(strbuf, "%02X", (uint8_t)last.le(j)); line = strbuf + line;
				}
				{ char strbuf[16]; sprintf(strbuf, ":%d", equcnt); line += strbuf;}
				ret.push_back(line);
				last = samples[i];
				equcnt = 1;
			}
		}
		string line;
		for (int j=0; j<NLCD; ++j) {
			char strbuf[3]; sprintf(strbuf, "%02X", (uint8_t)last.le(j)); line = strbuf + line;
		}
		{ char strbuf[16]; sprintf(strbuf, ":%d", equcnt); line += strbuf;}
		ret.push_back(line);
	}
	return ret;
}
#endif

#ifdef TagL4Host_IMPLEMENTATION
#undef TagL4Host_IMPLEMENTATION

#include "sysutil.h"
#include <fstream>
#include <iterator>

TagL4Host_t::TagL4Host_t(): com(NULL) {
	verbose = false;
	tx_running = false;
}

TagL4Host_t::TagL4Host_t(const TagL4Host_t& host) {
	assert(host.com == NULL && "opened host is not allowed to copy");
	com = host.com;
	verbose = host.verbose;
	tx_running = host.tx_running;
}

int TagL4Host_t::open(const char* _port) {
	assert(com == NULL && "device has been opened");
	// open com port
	lock.lock();
	port = _port;
	if (verbose) printf("opening device \"%s\"\n", port.c_str());
	com = new serial::Serial(port.c_str(), 115200, serial::Timeout::simpleTimeout(1000));
	assert(com->isOpen() && "port is not opened");
	while (com->available()) { uint8_t _c; com->read(&_c, 1); }  // clear the buffer of com
	// setup softio controller
	SOFTIO_QUICK_INIT(sio, mem, TagL4_Mem_FifoInit);
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
			if (softio_is_variable_included(sio, *head, mem.tx_count)) tx_count = mem.tx_count;
			if (softio_is_variable_included(sio, *head, mem.tx_underflow)) tx_underflow = mem.tx_underflow;
		}
	};
	// initialize device and verify it
	softio_blocking(read, sio, mem.version);
	assert(mem.version == MEMORY_TAG_L4XX_VERSION && "version not match");
	softio_blocking(read, sio, mem.mem_size);
	assert(mem.mem_size == sizeof(mem) && "memory size not equal, this should NOT occur");
	softio_blocking(read, sio, mem.pid);
	pid = mem.pid;
	lock.unlock();
	return 0;
}

int TagL4Host_t::close() {
	assert(com && "device not opened");
	tx_send_samples_stop();
	lock.lock();
	softio_wait_all(sio);
	com->close();
	delete com;
	com = NULL;
	lock.unlock();
	return 0;
}

int TagL4Host_t::dump(int extra) {
	lock.lock();
	printf("--- reading %d bytes ", (int)((char*)(void*)(&(mem.repeat_idx)) - (char*)(void*)(&(mem.command)) + sizeof(mem.repeat_idx)));
	su_time(softio_blocking, read_between, sio, mem.command, mem.repeat_idx);
	printf("[basic information]\n");
	printf("  1. command: 0x%02X [%s]\n", mem.command, COMMAND_STR(mem.command));
	printf("  2. flags: 0x%02X [", mem.flags);
		if (mem.flags & FLAG_MASK_HALT) printf(" FLAG_MASK_HALT");
		if (mem.flags & FLAG_MASK_DISABLE_LPUART1_OUT) printf(" FLAG_MASK_DISABLE_LPUART1_OUT");
		printf(" ]\n");
	printf("  3. verbose_level: 0x%02X\n", mem.verbose_level);
	printf("  4. mode: 0x%02X [%s]\n", mem.mode, MODE_STR(mem.mode));
	printf("  5. status: 0x%02X [%s]\n", mem.status, STATUS_STR(mem.status));
	printf("  6. pid: 0x%04X\n", mem.pid);
	printf("  7. version: 0x%08X\n", mem.version);
	printf("  8. mem_size: %d\n", (int)mem.mem_size);
	printf("  9. PIN_EN9: %d\n", (int)mem.PIN_EN9);
	printf(" 10. PIN_PWSEL: %d\n", (int)mem.PIN_PWSEL);
	printf(" 11. PIN_D0: %d\n", (int)mem.PIN_D0);
	printf(" 12. PIN_RXEN: %d\n", (int)mem.PIN_RXEN);
	printf(" 13. NLCD: %d\n", (int)mem.NLCD);

	printf("[statistics information]\n");
	printf("  1. siorx_overflow: %u\n", (unsigned)mem.siorx_overflow);
	printf("  2. lpuart1_tx_overflow: %u\n", (unsigned)mem.lpuart1_tx_overflow);
if (extra & TAGL4HOST_DUMP_TX) {
	printf("[Tx Information]\n");
	su_time(softio_blocking, read_between, sio, mem.period_lptim2, mem.default_sample);
	printf("  1. sample_rate: period = %d\n", (int)mem.period_lptim2);
	float freq_lptim2 = 32e3 / ((float)mem.period_lptim2 + 1);
	printf("             frequency = %f kHz\n", freq_lptim2/1000);
	printf("  2. count: %d, count_add: %d\n", (int)mem.tx_count, (int)mem.tx_count_add);
	printf("  3. tx_underflow: %d\n", (int)mem.tx_underflow);
	printf("  4. default sample: ");
	for (int i=0; i<TAG_L4XX_SAMPLE_BYTE; ++i) printf("%02X ", mem.default_sample.s[i]);
	printf("\n");
}
	lock.unlock();
	return 0;
}

float TagL4Host_t::set_tx_sample_rate(float frequency) {
	float period_target = 32e3 / frequency - 0.5;
	assert(period_target >= 0 && period_target <= UINT_MAX && "invalid frequency");
	uint32_t period = period_target;
	float frequency_real = 32e3 / (period + 1.f);
	lock.lock();
	mem.period_lptim2 = period;
	softio_blocking(write, sio, mem.period_lptim2);
	lock.unlock();
	return frequency_real;
}

int TagL4Host_t::set_tx_default_sample(Tag_Sample_t default_sample) {
	mem.default_sample = default_sample;
	lock.lock();
	softio_blocking(write, sio, mem.default_sample);
	lock.unlock();
	return 0;
}

void TagL4Host_t::tx_send_samples_background_thread(size_t sent_cnt, size_t count, function<void(Tag_Sample_t*, size_t)> callback) {
	vector<Tag_Sample_t> samples;
	size_t getlen;
	softio_delay_flush_try(read, sio, mem.tx_count);
	while (tx_running && sent_cnt < count) {
		// tag always has (mem.tx_count - 1) samples in buffer
		// printf("mem.tx_count: %d\n", mem.tx_count);
		lock.lock();
		softio_flush_try_handle_all(sio);
		lock.unlock();
		getlen = (mem.tx_data.Length() - 1) / TAG_L4XX_SAMPLE_BYTE + count - sent_cnt - mem.tx_count;  // maximum write without overflow
		getlen = min(getlen, count - sent_cnt);
		if (getlen) {
			if (getlen > mem.tx_data.Length() / TAG_L4XX_SAMPLE_BYTE * 0.75) {
				if (verbose) printf("tx fifo used > 3/4, may be system overloaded\n");
			}
			samples.reserve(getlen);
			callback(samples.data(), getlen);
			sent_cnt += getlen;
			assert(fifo_copy_from_buffer(&mem.tx_data, (const char*)samples.data(), getlen * TAG_L4XX_SAMPLE_BYTE) == getlen * TAG_L4XX_SAMPLE_BYTE);
			if (verbose) printf("[%d/%d] stream %d samples\n", (int)sent_cnt, (int)count, (int)getlen);
			lock.lock();
			while (!fifo_empty(&mem.tx_data)) {
				softio_delay(write_fifo, sio, mem.tx_data);  // fill the remote fifo
			}
			softio_delay_flush_try(read_between, sio, mem.tx_underflow, mem.tx_count);
			lock.unlock();
		} else {  // 0 to sleep
			this_thread::sleep_for(chrono::milliseconds(1));
			softio_blocking(read_between, sio, mem.tx_underflow, mem.tx_count);
		}
		assert(mem.tx_underflow == 0 && "tx overflow occurs, may be system overloaded or frequency too high");
	}
	// waiting for stop
	if (verbose) printf("tx background thread waiting for stop...\n");
	while (1) {
		lock.lock();
		softio_blocking(read, sio, mem.tx_count);
		lock.unlock();
		if (mem.tx_count == 0) break;
		if (verbose) printf("[%d/%d] waiting %d samples\n", (int)(sent_cnt - (mem.tx_count - 1)), (int)count, (int)mem.tx_count);
		this_thread::sleep_for(chrono::milliseconds(5));
	}
	lock.lock();
	softio_blocking(read, sio, mem.tx_underflow);
	lock.unlock();
	assert(mem.tx_underflow == 0);
}

float TagL4Host_t::tx_send_samples_start(float frequency, size_t count, function<void(Tag_Sample_t*, size_t)> callback) {
	assert(!tx_thread.joinable() && "thread exists");
	// use default callback
	if (!callback) {
		callback = [&](Tag_Sample_t* buf, size_t len) {
			size_t has_len = 0;
			tx_lock.lock();
			while (has_len < len && !tx_samples.empty()) {
				buf[has_len] = tx_samples.front();
				tx_samples.pop();
				++has_len;
			}
			tx_lock.unlock();
			for (size_t i=has_len; i<len; ++i) {
				buf[i] = mem.default_sample;
			}
		};
	}
	// reset tx
	float frequency_real = set_tx_sample_rate(frequency);  // set frequency
	mem.tx_count = 0;
	mem.tx_underflow = 0;
	lock.lock();
	softio_blocking(write_between, sio, mem.tx_underflow, mem.tx_count);  // clear previous count and underflow count
	softio_blocking(reset_fifo, sio, mem.tx_data);  // reset fifo
	mem.NLCD = TAG_L4XX_SAMPLE_BYTE;  // set NLCD to default one
	softio_blocking(write, sio, mem.NLCD);
	lock.unlock();
	fifo_clear(&mem.tx_data);
	if (verbose) printf("tx streaming frequency: %f kHz\n", frequency_real/1e3);
	// first fill the fifo
	size_t sent_cnt = 0;  // the length of sent, sample
	vector<Tag_Sample_t> samples;
	size_t getlen = fifo_remain(&mem.tx_data) / TAG_L4XX_SAMPLE_BYTE;
	getlen = min(getlen, count - sent_cnt);
	samples.reserve(getlen);
	callback(samples.data(), getlen);
	sent_cnt += getlen;
	assert(fifo_copy_from_buffer(&mem.tx_data, (const char*)samples.data(), getlen * TAG_L4XX_SAMPLE_BYTE) == getlen * TAG_L4XX_SAMPLE_BYTE);
	lock.lock();
	while (!fifo_empty(&mem.tx_data)) {
		softio_delay_flush_try(write_fifo, sio, mem.tx_data);  // fill the remote fifo
	}
	mem.tx_count = count;
	softio_blocking(write, sio, mem.tx_count);  // write count variable to start transmitting
	lock.unlock();
	// new background thread
	tx_thread = thread([&](size_t a, size_t b, function<void(Tag_Sample_t*, size_t)> c){ 
		this->tx_send_samples_background_thread(a, b, c); 
	}, sent_cnt, count, callback);
	tx_running = true;
	return frequency_real;
}

int TagL4Host_t::tx_send_samples_stop() {
	if (tx_running) {
		tx_running = false;
		if (tx_thread.joinable()) tx_thread.join();
	}
	return 0;
}

int TagL4Host_t::tx_send_samples_append(const vector<Tag_Sample_t>& samples) {
	tx_lock.lock();
	for (size_t i=0; i<samples.size(); ++i) {
		tx_samples.push(samples[i]);
	}
	tx_lock.unlock();
	return 0;
}

int TagL4Host_t::tx_send_samples_wait() {
	while (mem.tx_count) {
		this_thread::sleep_for(chrono::milliseconds(10));
	}
	return 0;
}

float TagL4Host_t::tx_send_samples(float frequency, const vector<Tag_Sample_t>& samples) {
	tx_send_samples_append(samples);
	float frequency_real = tx_send_samples_start(frequency, samples.size());
	tx_send_samples_wait();
	tx_send_samples_stop();
	return frequency_real;
}

float TagL4Host_t::tx_send_compressed(float frequency, const vector<string>& compressed, int& length) {
	if (compressed.empty()) return frequency;
	vector<Tag_Sample_t> samples = compressed_string_to_samples(compressed);
	length = (int)samples.size();
	return tx_send_samples(frequency, samples);
}

float TagL4Host_t::tx_repeat_samples(float frequency, const vector<Tag_Sample_t>& samples, int NLCD, int count, int interval) {
	assert(samples.size() * NLCD < __FIFO_GET_LENGTH(&mem.tx_data) && "cannot repeat message: too large");
	// must first stop any send mechanism
	mem.repeat_count = 0;
	lock.lock();
	softio_blocking(write, sio, mem.repeat_count);
	lock.unlock();
	while (1) {
		lock.lock();
		softio_blocking(read, sio, mem.repeat_state);
		lock.unlock();
		if (mem.repeat_state == REPEAT_STATE_NONE) break;
		if (verbose) printf("waiting repeat to end\n");
		this_thread::sleep_for(chrono::milliseconds(5));
	}
	tx_send_samples_stop();  // this makes sure mem.tx_count is 0
	// set up the repeat
	// first push the data there
	float frequency_real = set_tx_sample_rate(frequency);  // set frequency
	mem.tx_count = 0;
	mem.tx_underflow = 0;
	lock.lock();
	softio_blocking(write_between, sio, mem.tx_underflow, mem.tx_count);  // clear previous count and underflow count
	softio_blocking(reset_fifo, sio, mem.tx_data);  // reset fifo
	lock.unlock();
	fifo_clear(&mem.tx_data);
	if (verbose) printf("tx streaming frequency: %f kHz\n", frequency_real/1e3);
	// first store data in fifo
	for (auto it=samples.begin(); it!=samples.end(); ++it) {
		for (int i=0; i<NLCD; ++i) fifo_enque(&mem.tx_data, it->s[TAG_L4XX_SAMPLE_BYTE - NLCD + i]);
	}
	lock.lock();
	while (!fifo_empty(&mem.tx_data)) {
		softio_delay_flush_try(write_fifo, sio, mem.tx_data);  // copy to remote fifo
	}
	mem.NLCD = NLCD;
	softio_blocking(write, sio, mem.NLCD);
	mem.repeat_interval = interval;
	softio_blocking(write, sio, mem.repeat_interval);
	mem.repeat_count = count;
	softio_blocking(write, sio, mem.repeat_count);  // write count variable to start transmitting
	lock.unlock();
	return frequency_real;
}

float TagL4Host_t::tx_repeat_compressed(float frequency, const vector<string>& compressed, int NLCD, int count, int interval, int& length) {
	if (compressed.empty()) return frequency;
	vector<Tag_Sample_t> samples = compressed_string_to_samples(compressed);
	length = (int)samples.size();
	return tx_repeat_samples(frequency, samples, NLCD, count, interval);
}

#endif
