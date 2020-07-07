#include "sysutil.h"
#include "mosquitto.h"
#include <mutex>

#ifndef __MQTT_H
#define __MQTT_H

struct MQTT {
	static void LibInit();
	static void LibDeinit();
	struct mosquitto* mosq;
	int qos;  // default 0
	MQTT();
	function<void(MQTT& mqtt, int level, const char *str)> onLog;
	function<void(MQTT& mqtt, const struct mosquitto_message *message)> onMessage;
	function<void(MQTT& mqtt, int result)> onConnect;
	function<void(MQTT& mqtt, int mid, int qos_count, const int *granted_qos)> onSubscribe;
	function<void(MQTT& mqtt, int err)> onError;
	atomic<bool> run;
	mutex lock;
	thread loop;
	int start(const char* clientID = NULL, const char* host = "localhost", int port = 1883);
	int subscribe(const char* topic);
	int publish(const char* topic, const char* message);
	int publish(const char* topic, const char* message, size_t length);
	int stop();
	static void __log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str);
	static void __message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);
	static void __connect_callback(struct mosquitto *mosq, void *userdata, int result);
	static void __subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos);
};

#endif

#ifdef MQTT_IMPLEMENTATION
#undef MQTT_IMPLEMENTATION

void MQTT::LibInit() {
	mosquitto_lib_init();
}

void MQTT::LibDeinit() {
	mosquitto_lib_cleanup();
}

MQTT::MQTT() {
	mosq = NULL;
	qos = 0;
	onError = [&](MQTT& mqtt, int err) {
		(void)mqtt;
		printf("mqtt error: %s\n", mosquitto_strerror(err));
		assert(err == MOSQ_ERR_SUCCESS && "MQTT ERROR");
	};  // default error handler is assert fail
}

int MQTT::start(const char* clientID, const char* host, int port) {
	int err;
	assert(mosq == NULL && "MQTT has started");
	mosq = mosquitto_new(clientID, true, this);	
	if (!mosq){
        mosquitto_lib_cleanup();
        printf("create mqtt client failed\n");
		exit(-1);
    }
	mosquitto_log_callback_set(mosq, MQTT::__log_callback);
    mosquitto_connect_callback_set(mosq, MQTT::__connect_callback);
    mosquitto_message_callback_set(mosq, MQTT::__message_callback);
	mosquitto_subscribe_callback_set(mosq, MQTT::__subscribe_callback);
	if ((err = mosquitto_connect(mosq, host, port, 10)) != MOSQ_ERR_SUCCESS){
        printf("unable to connect %d: %s\n", err, mosquitto_strerror(err));
		exit(-1);
    }
	// if ((err = mosquitto_loop_start(mosq)) != MOSQ_ERR_SUCCESS) {
	// 	printf("unable to start loop %d: %s\n", err, mosquitto_strerror(err));
	// 	exit(-1);
	// }
	mosquitto_threaded_set(mosq, true);
	run.store(true);
	loop = thread([&]() {
		int e;
		while (run) {
			e = mosquitto_loop(mosq, 100, 1);
			if (e != MOSQ_ERR_SUCCESS) {
				if (!!onError) onError(*this, e);
				break;  // end this thread
			}
		}
	});
	return 0;
}

int MQTT::stop() {
	// mosquitto_loop_stop(mosq, true);
	assert(run && "MQTT is not started");
	if (run) {
		run.store(false);
		if (loop.joinable()) loop.join();
	}
	return 0;
}

int MQTT::subscribe(const char* topic) {
	lock.lock();
	mosquitto_subscribe(mosq, NULL, topic, qos);
	lock.unlock();
	return 0;
}

int MQTT::publish(const char* topic, const char* message, size_t length) {
	lock.lock();
	mosquitto_publish(mosq, NULL, topic, length, message, qos, 0);
	lock.unlock();
	return 0;
}

int MQTT::publish(const char* topic, const char* message) {
	publish(topic, message, strlen(message));
	return 0;
}

void MQTT::__log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str) {
	(void)mosq;
	MQTT& mqtt = *(MQTT*)userdata;
	if (!!mqtt.onLog) mqtt.onLog(mqtt, level, str);
}

void MQTT::__message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
	(void)mosq;
	MQTT& mqtt = *(MQTT*)userdata;
	if (!!mqtt.onMessage) mqtt.onMessage(mqtt, message);
}

void MQTT::__connect_callback(struct mosquitto *mosq, void *userdata, int result) {
	(void)mosq;
	MQTT& mqtt = *(MQTT*)userdata;
	if (!!mqtt.onConnect) mqtt.onConnect(mqtt, result);
}

void MQTT::__subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos) {
	(void)mosq;
	MQTT& mqtt = *(MQTT*)userdata;
	if (!!mqtt.onSubscribe) mqtt.onSubscribe(mqtt, mid, qos_count, granted_qos);
}

#endif
