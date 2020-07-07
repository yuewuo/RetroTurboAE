#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"

const char *MONGO_URL, *MONGO_DATABASE, *MQTT_HOST, *ServerID, *ClientId, *MyID;
int MQTT_PORT;
MongoDat mongodat;
MQTT mqttcli;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE
		, &MQTT_HOST, &MQTT_PORT, &ServerID, &ClientId, &MyID);  // this will delete those parameters from argc and argv, for RetroHost to call
	// HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv);  // or if you don't care those parameters, just call this
	
	printf("MONGO_URL: %s\n", MONGO_URL);
	printf("MONGO_DATABASE: %s\n", MONGO_DATABASE);
	printf("MQTT_HOST: %s\n", MQTT_HOST);
	printf("MQTT_PORT: %d\n", MQTT_PORT);
	printf("ServerID: %s\n", ServerID);
	printf("ClientId: %s\n", ClientId);
	printf("MyID: %s\n", MyID);
	printf("\nuser defined parameters below:\n");
	for (int i=1; i<argc; ++i) {
		printf("%s\n", argv[i]);
	}
	
	if (argc != 3) {
		printf("usage: <collection> <id(12 byte hex = 24 char)>");
		return -1;
	}
	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];

	// initialize library: database and MQTT service
	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);
	MQTT::LibInit();
	mqttcli.onConnect = [&](MQTT& mqtt, int result) {
		printf("mqtt connected: %d\n", result);
		mqtt.subscribe("something");  // subscribe something here
	};
	mqttcli.onMessage = [&](MQTT& mqtt, const struct mosquitto_message *message) {
		string topic = message->topic;
		int length = message->payloadlen;
		vector<char> payload((char*)message->payload, (char*)message->payload + length);
		mqtt.publish((string("<your topic here>") + MyID + "may add your own ID").c_str(), "something published");
	};
	mqttcli.start(MyID, MQTT_HOST, MQTT_PORT);

	// you can operate database file like these, first get object from database
	BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
	// then print it with extended json format
	printf("%s\n", MongoDat::dump(record).c_str());
	// basic query
	printf("%s", record.has("number") ? "record has field \"number\"" : "record don't have field \"number\"");
	if (record.has("number")) printf(", it's type is %d\n", record["number"].type()); else printf("\n");
	int number = 0;
	if (record["number"].existed() && record["number"].type() == BSON_TYPE_INT32) printf("number = %d\n", number = record["number"].value<int32_t>());
	// basic modify
	record["number"] = number + 1;  // equal to record["o_number"].operator=<int32_t>(2);
	record["text"] = "compiled at " __DATE__ " " __TIME__;
	printf("%s\n", MongoDat::dump(record).c_str());

	record["do"]["not"]["existed"]["path"]["can"]["also"]["be"]["built"].build_up();
	record["do.not.existed.other.thing"].build_up(); // nested representation for more flexibility
	record["do.not.existed.path"].remove();  // this will keep "do.not.existed.other.thing"
	printf("%s\n", MongoDat::dump(record).c_str());

	vector<int> intarr; intarr.push_back(1);
	vector<double> doublearr; doublearr.push_back(233);
	vector<string> strarr; strarr.push_back("yes"); strarr.push_back("hello world");
	record["arr"].remove();
	record["arr"].build_array();
	record["arr"].append(intarr);
	record["arr"].append(doublearr);
	record["arr"].append(strarr);
	printf("%s\n", MongoDat::dump(record).c_str());
	printf("%s\n", record["arr"][2].value<string>().c_str());
	printf("%s\n", record["arr"][3].value<string>().c_str());

	record.save();  // update record to database
	record.remove();

	mongodat.close();
	mqttcli.stop();
	printf("stopped gracefully\n");
	MQTT::LibDeinit();
	MongoDat::LibDeinit();

	return 0;
}
