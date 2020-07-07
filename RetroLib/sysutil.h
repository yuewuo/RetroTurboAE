#ifndef __sysutil_H
#define __sysutil_H

/* SysUtil header-only library
 * some small utility functions
 */

#include<sys/time.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "assert.h"
#include <random>
#include <ctime>
#include <atomic>
#include <thread>
#include <functional>
#include <complex>
using namespace std;

inline long get_time_now() {
	struct timeval time;
	gettimeofday( &time, NULL );
	return 1000000 * time.tv_sec + time.tv_usec;
}

#define su_time(func, ...) do {\
	long start = get_time_now();\
	func(__VA_ARGS__);\
	long end = get_time_now();\
	printf("(excuting \"%s\" consumes %d us)\n", #func, (int)(end-start));\
} while(0)

#define su_time_ret(func, ...) [](){\
	long start = get_time_now();\
	auto __ret = func(__VA_ARGS__);\
	long end = get_time_now();\
	printf("(excuting \"%s\" consumes %d us)\n", #func, (int)(end-start));\
	return __ret;\
}()

inline void randomize_mqtt_clientid(char* buf, int length, const char* charset) {
	int charset_len = strlen(charset);
	assert(charset_len > 0 && "charset must not empty");
	struct timeval time;
	gettimeofday( &time, NULL );
	mt19937 gen(1000000 * time.tv_sec + time.tv_usec);
	uniform_int_distribution<unsigned> dis(0, charset_len-1);
	for (int i=0; i<length; ++i) {
		buf[i] = charset[dis(gen)];
	}
}

inline bool is_char_in_string(string str, char c) {
	return str.find(c) != string::npos;
}

#define wait_variable_not_minus_1(sio, var, lock) while((int)var == -1) { \
	lock.lock(); \
	softio_wait_one(sio); \
	lock.unlock(); \
	this_thread::sleep_for(1ms); \
}

static inline void HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(int& argc, char** &argv,
		const char** MONGO_URL=NULL, const char** MONGO_DATABASE=NULL, const char** MQTT_HOST=NULL, int* MQTT_PORT=NULL,
		const char** ServerID=NULL, const char** ClientID=NULL, const char** MyID=NULL) {
	static char MyIDbuf[5];
	if (MQTT_HOST) *MQTT_HOST = "localhost";
	if (MQTT_PORT) *MQTT_PORT = 1883;
	if (MONGO_URL) *MONGO_URL = "mongodb://localhost:27017";
	if (MONGO_DATABASE) *MONGO_DATABASE = "retroturbo";
	if (ServerID) *ServerID = "";
	if (ClientID) *ClientID = "";
	if (MyID) *MyID = NULL;
	int i=1;
	for (; i<argc; ++i) {
		if (strncmp(argv[i], "~H", 2) == 0) {
			if (MQTT_HOST) *MQTT_HOST = argv[i] + 2;
		} else if (strncmp(argv[i], "~P", 2) == 0) {
			if (MQTT_PORT) *MQTT_PORT = atoi(argv[i] + 2);
		} else if (strncmp(argv[i], "~U", 2) == 0) {
			if (MONGO_URL) *MONGO_URL = argv[i] + 2;
		} else if (strncmp(argv[i], "~D", 2) == 0) {
			if (MONGO_DATABASE) *MONGO_DATABASE = argv[i] + 2;
		} else if (strncmp(argv[i], "~S", 2) == 0) {  // server's mqtt ID
			if (ServerID) *ServerID = argv[i] + 2;
		} else if (strncmp(argv[i], "~C", 2) == 0) {  // client's mqtt ID, for your convenience to send message to it
			if (ClientID) *ClientID = argv[i] + 2;
		} else if (strncmp(argv[i], "~M", 2) == 0) {  // client's mqtt ID, for your convenience to send message to it
			if (MyID) *MyID = argv[i] + 2;
		} else break;
	}
	if (MyID && !(*MyID)) {  // generate id for myself
		MyIDbuf[4] = '\0';
		randomize_mqtt_clientid(MyIDbuf, 4, "0123456789abcdefghijklmnopqrstuvwxyz");
		*MyID = MyIDbuf;
	}
	if (i != 1) {  // find some parameters, need to remove them
		for (int j=1; j<argc-(i-1); ++j) {
			argv[j] = argv[j+(i-1)];
		} argc -= (i-1);
	}
}

#endif
