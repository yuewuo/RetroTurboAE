#define ReaderH7Host_DEFINATION
#define ReaderH7Host_IMPLEMENTATION
#include "reader-H7xx-ex.h"
#define TagL4Host_DEFINATION
#define TagL4Host_IMPLEMENTATION
#include "tag-L4xx-ex.h"
#include "sysutil.h"
#define MQTT_IMPLEMENTATION
#include "mqtt.h"
#include "lua/lua.hpp"
#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include <chrono>

MongoDat mongodat;
MongoDat data_recorder_mongo;  // another mongo connection to reduce lock

#define VERSION_STR "RetroHost compiled at " __TIME__ ", " __DATE__ 
const char* MQTT_HOST = "localhost";
int MQTT_PORT = 1883;
const char* MQTT_CID_CHARSET = "0123456789abcdefghijklmnopqrstuvwxyz";
char TurboID[5];
MQTT hostmqtt;
atomic<bool> running;
void hostOnMessage(MQTT& mqtt, const struct mosquitto_message *message);

ReaderH7Host_t reader;
TagL4Host_t tag;
thread luathread;
char data_recorder_buf[4194304];  // about 9 second, 2^24 > 455000 * 9
Fifo_t data_recorder_fifo;
thread data_recorder;
atomic<bool> data_recorder_enabled;  // default is disabled, consume 455kB/s = 3.64mbps, that is a little bit too large. you can open it within lua
char record_filename[64];  // read-only
const char* reader_port = NULL;
const char* tag_port = NULL;
const char* MONGO_URL = "mongodb://localhost:27017";
const char* MONGO_DATABASE = "retroturbo";
bool silent_mode = false;  // if this is set to true, host will not respond to "query". see "execlua_new_processs.py" as a demo to use this feature


int main(int argc, char** argv) {
	MQTT::LibInit();
	MongoDat::LibInit();
	char c;
	const char* optformat = "H:P:C:U:D:R:T:hS";
	// initialize TurboID
	TurboID[4] = 0;
	randomize_mqtt_clientid(TurboID, 4, MQTT_CID_CHARSET);
	while ((c = getopt(argc, argv, optformat)) != (char)0xFF) {
		switch(c) {
			case 'H': MQTT_HOST = optarg; break;
			case 'P': MQTT_PORT = atoi(optarg); break;
			case 'C': 
				assert(strlen(optarg) == 4 && "MQTT_CLIENTID must be length of 4");
				for (int i=0; i<4; ++i) { assert(is_char_in_string(MQTT_CID_CHARSET, optarg[i]) && "not valid char"); TurboID[i] = optarg[i]; }
				break;
			case 'U': MONGO_URL = optarg; break;
			case 'D': MONGO_DATABASE = optarg; break;
			case 'R': reader_port = optarg; break;
			case 'T': tag_port = optarg; break;
			case 'S': silent_mode = true; break;
			case 'h':
			default:
				printf("usage: %s\n", optformat);
				printf("  H: MQTT_HOST (default: \"%s\")\n", MQTT_HOST);
				printf("  P: MQTT_PORT (default: %d)\n", MQTT_PORT);
				printf("  C: MQTT_CLIENTID, must be 4byte in [%s]\n", MQTT_CID_CHARSET);
				printf("  U: MONGO_URL (default: \"%s\")\n", MONGO_URL);
				printf("  U: MONGO_DATABASE (default: \"%s\")\n", MONGO_DATABASE);
				printf("  R: reader_port\n");
				printf("  T: tag_port\n");
				return -1;
		}
	}
	printf("TurboHost information:\n");
	printf("  TurboID: %s\n", TurboID);
	printf("  MQTT_HOST: %s\n", MQTT_HOST);
	printf("  MQTT_PORT: %d\n", MQTT_PORT);
	printf("  MONGO_URL: %s\n", MONGO_URL);
	printf("  MONGO_DATABASE: %s\n", MONGO_DATABASE);
	printf("  silent_mode: %s\n", silent_mode ? "yes" : "no");
	if (reader_port) printf("  reader_port: %s\n", reader_port);
	else printf("[Warning]: reader_port not provided\n");
	if (tag_port) printf("  tag_port: %s\n", tag_port);
	else printf("[Warning]: tag_port not provided\n");
	printf("\n");
	fflush(stdout);  // necessary for other application read from pipe
	
	mongodat.open(MONGO_URL, MONGO_DATABASE);
	printf("mongodb opened\n");
	running.store(true);

	// reader.verbose = true;
	if (tag_port) {
		tag.open(tag_port);
		Tag_Sample_t zero;
		tag.set_tx_default_sample(zero);
	}
	if (reader_port) {
		reader.open(reader_port);
		// pair<float, float> actual = reader.set_lptim2(8e6, 0.5);  // set to 8MHz
		// printf("lptim2 set to %f kHz, %f%% duty cycle\n", actual.first/1000, actual.second*100);
		reader.output_to_rx_data = false;  // default not output, to avoid overflow
	}
	printf("tag and reader opened\n");
	fifo_init(&data_recorder_fifo, data_recorder_buf, sizeof(data_recorder_buf));
	data_recorder_enabled.store(false);
	reader.rx_data_callback = [&](const filter_out_t* buffer, int length) {
		if (data_recorder_enabled.load()) {
			assert(fifo_copy_from_buffer(&data_recorder_fifo, (const char*)buffer, length*8U) == length*8U && "recorder fifo overflowed");
		}
	};
	data_recorder = thread([&]() {
		data_recorder_mongo.open(MONGO_URL, MONGO_DATABASE);
		record_filename[0] = '\0';
		mongoc_stream_t* upload_stream = NULL;
		int filesize = 0;
		struct timeval mytime;
		long last = 0;
		while (running.load()) {
			if (data_recorder_enabled.load()) {
				if (record_filename[0] == '\0') {  // the first time to open this
					time_t timep;
					time(&timep);
					char tmp_record_filename[64];
					strftime(tmp_record_filename, sizeof(tmp_record_filename), "%Y-%m-%d %H_%M_%S.rx_data", localtime(&timep));
					printf("record_filename: %s\n", tmp_record_filename);
					upload_stream = mongoc_gridfs_bucket_open_upload_stream(data_recorder_mongo.bucket, tmp_record_filename, NULL, &data_recorder_mongo.file_id, &data_recorder_mongo.error);
					assert(upload_stream && "record stream open failed");
					filesize = 0;
					gettimeofday(&mytime, NULL);
					last = 1000000 * mytime.tv_sec + mytime.tv_usec;
					strcpy(record_filename, tmp_record_filename);  // must after initialization done
				}
				if (!fifo_empty(&data_recorder_fifo)) {
					uint32_t copylen = fifo_count(&data_recorder_fifo);
					uint32_t len = __fifo_read_base_length(&data_recorder_fifo);
					if (copylen > len) copylen = len;  // needs continuous buffer
					// printf("record stream %d bytes\n", copylen);
					assert(copylen == mongoc_stream_write(upload_stream, __fifo_read_base(&data_recorder_fifo), copylen, -1) && "stream record to remote failed");
					filesize += copylen;
					data_recorder_fifo.read = (data_recorder_fifo.read + copylen) % data_recorder_fifo.Length();  // set pointer directly
				} else {
					gettimeofday(&mytime, NULL);
					long now = 1000000 * mytime.tv_sec + mytime.tv_usec;
					if (now - last > 500000) {  // send message at 500ms interval
						last = now;
						char buf[64];
						string idstr = MongoDat::OID2str(data_recorder_mongo.get_fileID());
						sprintf(buf, "%fMB %s (id: \"%s\")", filesize/1e6, record_filename, idstr.c_str());
						hostmqtt.publish((string("retroturbo/") + TurboID + "/record").c_str(), buf, strlen(buf));
					}
					this_thread::sleep_for(10ms);  // not more data, sleep to reduce CPU load
				}
			} else {
				if (record_filename[0] != '\0') {  // user just close it
					mongoc_stream_destroy(upload_stream);
					// add metadata here
					bson_t _metadata = BSON_INITIALIZER;
					bson_t* metadata = &_metadata;
					int Dimension = 4, Length = filesize / 8;
					UPLOAD_RECORD_ADD_METADATA(int16_t, "int16_t");
					BSON_APPEND_UTF8(metadata, "title", record_filename);
					BSON_APPEND_DOUBLE(metadata, "x_unit", 1/56.875);
					BSON_APPEND_UTF8(metadata, "x_label", "time(ms)");
					BSON_APPEND_DOUBLE(metadata, "y_unit", 1);
					BSON_APPEND_UTF8(metadata, "y_label", "Ia,Qa,Ib,Qb");
					// find 
					bson_t filter;
					bson_init(&filter);
					bson_oid_t id = data_recorder_mongo.get_fileID();
					BSON_APPEND_OID(&filter, "_id", &id);  // query with ID
					mongoc_gridfs_file_list_t* list = mongoc_gridfs_find_with_opts(data_recorder_mongo.gridfs, &filter, NULL);
					mongoc_gridfs_file_t *file = mongoc_gridfs_file_list_next(list);
					assert(file && "record just inserted not existed, strange");
					mongoc_gridfs_file_set_metadata(file, metadata);
					mongoc_gridfs_file_save(file);
					mongoc_gridfs_file_destroy(file);
					mongoc_gridfs_file_list_destroy(list);
					bson_destroy(&filter);
					bson_destroy(metadata);
					record_filename[0] = 0;
				}
				fifo_clear(&data_recorder_fifo);
				this_thread::sleep_for(10ms);
			}
		}
		data_recorder_mongo.close();
	});
	if (reader_port) {
		reader.start_rx_receiving();
	}

	hostmqtt.onLog = [&](MQTT& mqtt, int level, const char *str) {
		(void)mqtt; (void)level; (void)str;
		// printf("LOG: %s\n", str);  // log sometimes too much
	};
	hostmqtt.onConnect = [&](MQTT& mqtt, int result) {
		printf("hostmqtt connected: %d\n", result);
		fflush(stdout);  // necessary for other application read from pipe
		mqtt.subscribe("retroturbo/query");  // query retrohosts
		mqtt.subscribe((string("retroturbo/") + TurboID + "/info").c_str());
		mqtt.subscribe((string("retroturbo/") + TurboID + "/shutdown").c_str());
		mqtt.subscribe((string("retroturbo/") + TurboID + "/enable_record").c_str());
		mqtt.subscribe((string("retroturbo/") + TurboID + "/disable_record").c_str());
		mqtt.subscribe((string("retroturbo/") + TurboID + "/terminate").c_str());
		mqtt.subscribe((string("retroturbo/") + TurboID + "/do/#").c_str());
	};
	hostmqtt.onMessage = hostOnMessage;
	hostmqtt.start(TurboID, MQTT_HOST, MQTT_PORT);

	while (running.load()) {
		this_thread::sleep_for(100ms);
	}

	// reader.load_refpreamble("ref.refraw");
	// reader.open(argv[1]);
	// reader.gain_ctrl(0.2);
	// printf("AGC done\n");
	// reader.start_preamble_receiving(100, 10.0);
	// printf("reader is listening to preamble\n");
	// int ret = reader.wait_preamble_for(5);  // wait for 5s
	// reader.stop_preamble_receiving();
	// printf("reader complete with snr = %f, ret = %d\n", reader.snr_preamble, ret);
	// reader.close();

	if (luathread.joinable()) luathread.join();
	if (reader_port) reader.close();
	if (tag_port) tag.close();
	if (data_recorder.joinable()) data_recorder.join();
	mongodat.close();
	hostmqtt.stop();
	printf("stopped gracefully\n");
	MQTT::LibDeinit();
	MongoDat::LibDeinit();
}

string luascript;
string luacid;

#include "host_luaext.h"

static inline bool isDoCommand(const string& cid) {    // clientID must be 4 byte. do/xxxx/
	if (cid.size() != 7) return false;
	if (cid[0] != 'd' || cid[1] != 'o' || cid[2] != '/') return false;
	for (int i=3; i<7; ++i) {
		char c = cid[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
	}
	return true;
}

static mutex luaMutex;
static inline void exclusiveDoCommand(const char* script, const string& cid) {  // run lua command in a new thread
	if (luaMutex.try_lock()) {
		if (luathread.joinable()) luathread.join();
		luascript = script;
		luacid = cid;
		luathread = thread([&](){ 
			request_terminate_lua.store(false);  // nobody request termination until someone do it
			bson_t newobj;
			bson_init(&newobj);
			bson_oid_t oid;
			bson_oid_init(&oid, NULL);
			// printf("new _id: %s\n", MongoDat::OID2str(oid).c_str());
			BSON_APPEND_OID(&newobj, "_id", &oid);
			time_t timep; time(&timep);
			BSON_APPEND_TIMESTAMP(&newobj, "timestamp", (uint32_t)timep, 0);
			BSON_APPEND_UTF8(&newobj, "cid", luacid.c_str());
			BSON_APPEND_UTF8(&newobj, "sid", TurboID);
			BSON_APPEND_UTF8(&newobj, "code", luascript.c_str());
			mongoc_collection_t *collection = mongoc_database_get_collection(mongodat.database, "lua");
			assert(mongoc_collection_insert_one(collection, &newobj, NULL, NULL, &mongodat.error) && "cannot save lua code to database");
			mongoc_collection_destroy(collection);
			bson_destroy(&newobj);
			// printf("script: %s\n", luascript.c_str());
			lua_State *L = luaL_newstate();
			luaL_openlibs(L);
	#define LR(func) lua_register(L, #func, luafunc_##func)  // register functions here
			luaext_register
	#undef LR
			int ret = luaL_loadstring(L, luascript.c_str());
			if (!ret) {
				ret = lua_pcall(L, 0, LUA_MULTRET, 0);
			}
			printf("lua ret: %d\n", ret);
			if (ret != LUA_OK) {
				const char *msg = lua_tostring(L, -1);
				printf("%s\n", msg);
				size_t prefixlen = strlen("[string \"...\"]:");
				if (strlen(msg) > prefixlen) msg += prefixlen;
				hostmqtt.publish((string("retroturbo/") + TurboID + "/err/" + luacid).c_str(), msg);
				lua_pop(L, 1);  /* remove message */
			}
			char buf[3];
			sprintf(buf, "%d", ret);
			hostmqtt.publish((string("retroturbo/") + TurboID + "/ret/" + luacid).c_str(), buf);
			lua_close(L);
			luaMutex.unlock();
			fflush(stdout);  // make sure external recorder works fine
		});
	} else {
		hostmqtt.publish((string("retroturbo/") + TurboID + "/ret/" + cid).c_str(), "7");  // timeout
	}
}

void hostOnMessage(MQTT& mqtt, const struct mosquitto_message *message) {
	string topic = message->topic;
	int length = message->payloadlen;
	vector<char> payload;
	payload.reserve(length+1);
	for (int i=0; i<length; ++i) {
		payload[i] = ((char*)message->payload)[i];
	} payload[length] = '\0';  // add \0 at the end
	const char* str = payload.data();
	printf("topic: %s\n", topic.c_str());
	string prefix = string("retroturbo/") + TurboID + "/";
	if (topic == "retroturbo/query") {
		if (!silent_mode) mqtt.publish((string("retroturbo/reply/") + TurboID).c_str(), VERSION_STR);
	} else if (topic.compare(0, prefix.size(), prefix) == 0) {
		string subtopic = topic.substr(prefix.size());
		if (subtopic == "info") {
			
		} else if (subtopic == "shutdown") {
			running.store(false);
		} else if (isDoCommand(subtopic)) {
			string cid = subtopic.substr(3, 4);
			exclusiveDoCommand(str, cid);
		} else if (subtopic == "enable_record") {
			if (reader_port) data_recorder_enabled.store(true);
		} else if (subtopic == "disable_record") {
			if (reader_port) {
				data_recorder_enabled.store(false);
				hostmqtt.publish((string("retroturbo/") + TurboID + "/record_stop").c_str(), NULL, 0);
			}
		} else if (subtopic == "terminate") {
			request_terminate_lua.store(true);
		}
	}
}
