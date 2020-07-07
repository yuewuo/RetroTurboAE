#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include <chrono>
#include <thread>
using namespace std;
using namespace std::chrono;

const char *MONGO_URL, *MONGO_DATABASE, *MQTT_HOST, *ServerID, *ClientId, *MyID;
int MQTT_PORT;
MongoDat mongodat;
MQTT mqttcli_local;
MQTT mqttcli_remote;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE
		, &MQTT_HOST, &MQTT_PORT, &ServerID, &ClientId, &MyID);  // this will delete those parameters from argc and argv, for RetroHost to call
	
	printf("MONGO_URL: %s\n", MONGO_URL);
	printf("MONGO_DATABASE: %s\n", MONGO_DATABASE);
	printf("MQTT_HOST: %s\n", MQTT_HOST);
	printf("MQTT_PORT: %d\n", MQTT_PORT);
	printf("ServerID: %s\n", ServerID);
	printf("ClientId: %s\n", ClientId);
	printf("MyID: %s\n", MyID);

	if (argc < 3) {
		printf("usage: <collection> <id(12 byte hex = 24 char)> <code field> <remote host field> <remote port field> <timeout field> <remote server ID (auto search if not given and write back)>");
		return -1;
	}
	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];
	const char* code_field_str = argc > 3 ? argv[3] : "code";
	const char* remote_host_field_str = argc > 4 ? argv[4] : "remote_host";
	const char* remote_port_field_str = argc > 5 ? argv[5] : "remote_port";
	const char* timeout_field_str = argc > 6 ? argv[6] : "timeout";
	const char* remote_server_id_field_str = argc > 7 ? argv[7] : "remote_ID";

	// initialize library: database and MQTT service
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
	assert(record.has(code_field_str) && record[code_field_str].type() == BSON_TYPE_UTF8 && "code field not existed or error type");
	assert(record.has(remote_host_field_str) && record[remote_host_field_str].type() == BSON_TYPE_UTF8 && "remote host field not existed or error type");
	assert(record.has(remote_port_field_str) && record[remote_port_field_str].type() == BSON_TYPE_INT32 && "remote port field not existed or error type");
	assert(record.has(timeout_field_str) && record[timeout_field_str].type() == BSON_TYPE_DOUBLE && "timeout field not existed or error type");
	string code = record[code_field_str].value<string>();
	string remote_host = record[remote_host_field_str].value<string>();
	int remote_port = record[remote_port_field_str].value<int32_t>();
	double timeout = record[timeout_field_str].value<double>();
	string remote_server_id;
	if (record.has(remote_server_id_field_str)) {
		assert(record[remote_server_id_field_str].type() == BSON_TYPE_UTF8 && "server id error type");
		remote_server_id = record[remote_server_id_field_str].value<string>();
	}
	bool auto_find_remote_server_id = (remote_server_id.size() != 4);

	// vector<string> logs;
	// string error;
	int ret = -1;
	string retmsg;
	atomic<bool> returned;
	returned.store(false);
	volatile bool remote_server_replied = false;

	MQTT::LibInit();

	// start local mqtt for message proxying
	mqttcli_local.onConnect = [&](MQTT& mqtt, int result) {
		printf("local mqtt connected: %d\n", result); (void)mqtt;
	};
	mqttcli_local.start(MyID, MQTT_HOST, MQTT_PORT);

	// start remote mqtt for connection
	char RandomID[5]; RandomID[4] = 0;
	randomize_mqtt_clientid(RandomID, 4, "0123456789abcdefghijklmnopqrstuvwxyz");  printf("use random ID: %s\n", RandomID);
	mqttcli_remote.onConnect = [&](MQTT& mqtt, int result) {
		printf("remote mqtt connected: %d\n", result);
		mqtt.subscribe("retroturbo/#");
		mqtt.publish("retroturbo/query", "");
	};
	mqttcli_remote.onMessage = [&](MQTT& mqtt, const struct mosquitto_message *message) {
		string topic = message->topic;
		int length = message->payloadlen;
		vector<char> payload((char*)message->payload, (char*)message->payload + length);
		payload.push_back('\0');
		const char* str = payload.data();
		// printf("[remote]: %s\n", topic.c_str());
		// mqtt.publish((string("<your topic here>") + MyID + "may add your own ID").c_str(), "something published");
		if (topic.compare(0, strlen("retroturbo/"), "retroturbo/") != 0) return;
		topic = topic.substr(strlen("retroturbo/"));
		if (remote_server_replied == false && topic.compare(0, strlen("reply/"), "reply/") == 0 && topic.size() == strlen("reply/1234")) {
			string replied_id = topic.substr(strlen("reply/"));
			printf("replied_id: %s, remote_server_id: %s\n", replied_id.c_str(), remote_server_id.c_str());
			printf("replied_id == remote_server_id: %d %d, auto_find_remote_server_id: %d\n", (int)(replied_id == remote_server_id), (int)(replied_id.compare(remote_server_id)), (int)auto_find_remote_server_id);
			if (auto_find_remote_server_id) {
				remote_server_id = replied_id;
				remote_server_replied = true;
			} else if (replied_id == remote_server_id) {
				remote_server_replied = true;
			}
			if (remote_server_replied) {  // server replied, then send code to him
				printf("[system] sending code to host...\n");
				mqtt.publish((string("retroturbo/") + remote_server_id + "/do/" + RandomID).c_str(), code.c_str());
			}
		}
		if (remote_server_replied == true && topic.compare(0, 4, remote_server_id.c_str()) == 0 && topic[4] == '/') {
			string subtopic = topic.substr(5);
			// printf("subtopic: %s\n",subtopic.c_str());
			if (subtopic == string("log/") + RandomID) {
				printf("%s", str);
				mqttcli_local.publish((string("retroturbo/") + ServerID + "/log/" + ClientId).c_str(), str, length);
			} else if (subtopic == string("err/") + RandomID) {
				fprintf(stderr, "%s\n", str);
				mqttcli_local.publish((string("retroturbo/") + ServerID + "/err/" + ClientId).c_str(), str, length);
			} else if (subtopic == string("ret/") + RandomID) {
				ret = atoi(str);
				const char* retmsgs[7] = {"OK", "YIELD", "ERRRUN", "ERRSYNTAX", "ERRMEM", "ERRGCMM", "ERRERR"};
				if (ret >= 0 && ret < 7) retmsg = retmsgs[ret];
				else retmsg = "unknown return value";
				printf("[system] return %d (%s)\n", ret, retmsg.c_str());
				returned.store(true);
			}
		}
	};
	mqttcli_remote.start(RandomID, remote_host.c_str(), remote_port);

	// check timeout or returned
	auto start = high_resolution_clock::now();
	bool istimeout = true;
	while (duration_cast<duration<double>>(high_resolution_clock::now() - start).count() < timeout) {
		if (returned.load()) {
			istimeout = false; break;
		}
		this_thread::sleep_for(chrono::milliseconds(5));
	}

	double dura = duration_cast<duration<double>>(high_resolution_clock::now() - start).count();
	if (istimeout) {
		printf("timeout (%f > %f)...\n", dura, timeout);
	}
	// save result to database
	record["is_replied"] = (int32_t)remote_server_replied;
	record["is_timeout"] = (int32_t)istimeout;
	record["duration"] = dura;
	record["ret"] = (int32_t)ret;
	record["retmsg"] = retmsg;
	record.save();  // update record to database

	mongodat.close();
	mqttcli_remote.stop();
	mqttcli_local.stop();
	printf("stopped gracefully\n");
	MQTT::LibDeinit();
	MongoDat::LibDeinit();

	return 0;
}
