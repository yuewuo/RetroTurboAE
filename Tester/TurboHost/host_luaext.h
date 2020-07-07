
#define luaext_register \
	LR(sleepms); LR(shutdown); LR(log); LR(load_preamble_ref); LR(set_record); LR(log_binary_mongoID); LR(run); LR(draw_record_mongoID); \
	LR(mongo_get_one_as_jsonstr); LR(mongo_update_one_with_jsonstr); LR(mongo_replace_one_with_jsonstr);  LR(mongo_create_one_with_jsonstr);  \
	LR(tag_send); LR(reader_gain_control); LR(reader_set_gain); LR(reader_start_preamble); LR(reader_wait_preamble); LR(reader_stop_preamble); LR(reader_info); LR(reader_save_preamble); \
	LR(generate_random_data); LR(list_all_documents_id); \
	LR(tag_set_en9); LR(tag_set_pwsel); \
	LR(may_terminate); LR(user_terminated); \
	LR(save_to_file); LR(get_from_file);

#define READER_NEEDED if (!reader_port) luaL_error(L, "reader is not available, restart your host with reader enabled");
#define TAG_NEEDED if (!tag_port) luaL_error(L, "tag is not available, restart your host with tag enabled");

static atomic<bool> request_terminate_lua;
static int luafunc_may_terminate(lua_State* L) {  // this function allows termination from outside
	if (request_terminate_lua.load()) {  // somebody wants to terminate current running lua script, just do it
		int n = lua_gettop(L);
		if (n >= 1) {
			size_t length;
			const char* msg = luaL_tolstring(L, 1, &length);
			hostmqtt.publish((string("retroturbo/") + TurboID + "/log/" + luacid).c_str(), msg, length);
		}
		luaL_error(L, "user terminated");
	}
	return 0;
}

static int luafunc_user_terminated(lua_State* L) {
	lua_pushboolean(L, request_terminate_lua.load());
	return 1;
}

static int luafunc_sleepms(lua_State* L) {
	int ms = luaL_checkinteger(L, 1);
	auto start = std::chrono::high_resolution_clock::now();
	int now_ms = 0;
	while((now_ms = (int)((std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start).count() * 1000))) < ms) {
		this_thread::sleep_for(std::chrono::milliseconds(ms - now_ms));
	}
	return 0;
}

static int luafunc_shutdown(lua_State* L) {
	running.store(false);
	lua_pushinteger(L, 0);
	return 1;
}

static int luafunc_log(lua_State* L) {
	size_t length;
	// const char* msg = luaL_checklstring(L, 1, &length);
	const char* msg = luaL_tolstring(L, 1, &length);
	hostmqtt.publish((string("retroturbo/") + TurboID + "/log/" + luacid).c_str(), msg, length);
	lua_pushinteger(L, 0);
	return 1;
}

static inline string my_luaL_getstring(lua_State* L, int num) {
	size_t length;
	const char* _str = luaL_checklstring(L, num, &length);
	string str;
	str.resize(length);
	for (int i=0; i<(int)length; ++i) str[i] = _str[i];
	return str;
}

static int luafunc_list_all_documents_id(lua_State* L) {
	string collection_str = my_luaL_getstring(L, 1);
	int n = lua_gettop(L);
	vector<bson_oid_t> ids;
	if (n >= 2) {
		string query_str = my_luaL_getstring(L, 2);
		bson_t *query = bson_new_from_json((const uint8_t*)query_str.c_str(), -1, &mongodat.error);
		if (query == NULL) luaL_error(L, "invalid json");
		ids = mongodat.get_all_id(collection_str.c_str(), query);
		bson_destroy(query);
	} else ids = mongodat.get_all_id(collection_str.c_str());
	lua_createtable(L, ids.size(), 0);
	for (size_t i=0; i<ids.size(); ++i) {
		lua_pushstring(L, MongoDat::OID2str(ids[i]).c_str());
		lua_rawseti(L, -2, i+1);
	}
	return 1;
}

static int luafunc_reader_save_preamble(lua_State* L) {
	READER_NEEDED
	// generate filename
	time_t timep;
	time(&timep);
	char tmp_record_filename[64];
	strftime(tmp_record_filename, sizeof(tmp_record_filename), "%Y-%m-%d %H_%M_%S.preamble", localtime(&timep));
	// upload file
	mongodat.upload_record(tmp_record_filename, (float*)reader.result_preamble.data(), 2, reader.result_preamble.size(), NULL, "Preamble Record", 1/80.0, "time(ms)", 1, "I,Q");
	bson_oid_t id = mongodat.get_fileID();
	string id_str = MongoDat::OID2str(id);
	lua_pushlstring(L, id_str.c_str(), id_str.size());
	return 1;
}

static int luafunc_reader_info(lua_State* L) {
	READER_NEEDED
	lua_pushnumber(L, reader.snr_preamble);
	lua_pushboolean(L, reader.preamble_running);
	lua_pushinteger(L, reader.result_preamble.size());
	return 3;
}

static int luafunc_reader_stop_preamble(lua_State* L) {
	READER_NEEDED
	if (!reader.preamble_running) luaL_error(L, "preamble is not running, start it first");
	reader.stop_preamble_receiving(false);  // don't close rx
	return 0;
}

static int luafunc_reader_wait_preamble(lua_State* L) {
	READER_NEEDED
	if (!reader.preamble_running) luaL_error(L, "preamble is not running, start it first");
	float second2wait = luaL_checknumber(L, 1);
	reader.wait_preamble_for(second2wait);
	lua_pushboolean(L, reader.snr_preamble != 0);  // this means preamble has been received
	return 1;
}

static int luafunc_reader_start_preamble(lua_State* L) {
	READER_NEEDED
	if (reader.preamble_running) luaL_error(L, "preamble is still running, don't start again");
	if (reader.refpreamble.size() == 0) luaL_error(L, "no ref loaded");
	int point2recv = luaL_checkinteger(L, 1);
	float snr = luaL_checknumber(L, 2);
	fifo_clear(&reader.rx_data);
	reader.output_to_rx_data = true;
	reader.start_preamble_receiving(point2recv, snr, true);
	return 0;
}

static int luafunc_reader_gain_control(lua_State* L) {
	READER_NEEDED
	if (reader.preamble_running) luaL_error(L, "preamble is still running, close it before adjust gain");
	float ratio = luaL_checknumber(L, 1);
	printf("ratio: %f\n", ratio);
	float now = reader.gain_ctrl(ratio);
	lua_pushnumber(L, now);
	return 1;
}

static int luafunc_reader_set_gain(lua_State* L) {
	READER_NEEDED
	if (reader.preamble_running) luaL_error(L, "preamble is still running, close it before adjust gain");
	float volt = luaL_checknumber(L, 1);
	reader.set_dac_volt_delay(volt);
	return 0;
}

static int luafunc_tag_set_en9(lua_State* L) {
	TAG_NEEDED
	int val = luaL_checkinteger(L, 1);
	tag.mem.PIN_EN9 = !!val;
	softio_blocking(write, tag.sio, tag.mem.PIN_EN9);
	return 0;
}

static int luafunc_tag_set_pwsel(lua_State* L) {
	TAG_NEEDED
	int val = luaL_checkinteger(L, 1);
	tag.mem.PIN_PWSEL = !!val;
	softio_blocking(write, tag.sio, tag.mem.PIN_PWSEL);
	return 0;
}

static int luafunc_tag_send(lua_State* L) {
	TAG_NEEDED
	string collection_str = my_luaL_getstring(L, 1);
	string id_str = my_luaL_getstring(L, 2);
	string frequency_field_str = my_luaL_getstring(L, 3);
	string arr_field_str = my_luaL_getstring(L, 4);
	if (!MongoDat::isPossibleOID(id_str.c_str())) luaL_error(L, "the second parameter must be a valid ID");
	if (collection_str.empty()) luaL_error(L, "collection name cannot be empty");
	if (frequency_field_str.empty()) luaL_error(L, "frequency_field name cannot be empty");
	if (arr_field_str.empty()) luaL_error(L, "arr_field name cannot be empty");
	BsonOp record = mongodat.get_bsonop(collection_str.c_str(), id_str.c_str());
	if (!record.existed()) luaL_error(L, "cannot find document");
	BsonOp arr = record[arr_field_str.c_str()];
	if (!arr.existed() || arr.type() != BSON_TYPE_ARRAY) luaL_error(L, "target must be array");
	BsonOp frequency = record[frequency_field_str.c_str()];
	if (!frequency.existed() || frequency.type() != BSON_TYPE_DOUBLE) luaL_error(L, "frequency must be double");
	vector<string> compressed; int arr_length = arr.count();
	for (int i=0; i<arr_length; ++i) {
		compressed.push_back(arr[i].value<string>());
		// printf("%s\n", compressed.back().c_str());
	}
	int count = 0;
	float real_frequency = tag.tx_send_compressed(frequency.value<double>(), compressed, count);
	record.remove();
	lua_pushinteger(L, count);
	lua_pushnumber(L, real_frequency);
	return 2;
}

static int luafunc_mongo_create_one_with_jsonstr(lua_State* L) {
	string collection_str = my_luaL_getstring(L, 1);
	string newobj_str = my_luaL_getstring(L, 2);   // a json string
	bson_t *newobj = bson_new_from_json((const uint8_t*)newobj_str.c_str(), -1, &mongodat.error);
	if (newobj == NULL) luaL_error(L, "invalid json");
	bson_iter_t iter;
	string id_str;
	if (bson_iter_init(&iter, newobj) && bson_iter_find(&iter, "_id")) {
		// id existed, do not change
		const bson_oid_t *oid = bson_iter_oid(&iter);
		id_str = MongoDat::OID2str(*oid);
	} else {
		bson_oid_t oid;
		bson_oid_init(&oid, NULL);
		id_str = MongoDat::OID2str(oid);
		// printf("new _id: %s\n", MongoDat::OID2str(oid).c_str());
		BSON_APPEND_OID(newobj, "_id", &oid);
	}
	mongoc_collection_t *collection = mongoc_database_get_collection(mongodat.database, collection_str.c_str());
	bson_t reply;
	bool success = mongoc_collection_insert_one(collection, newobj, NULL, &reply, &mongodat.error);
	if (success) {
		lua_pushlstring(L, id_str.c_str(), id_str.size());
	} else {
		lua_pushnil(L);
	}
	bson_destroy(&reply);
	mongoc_collection_destroy(collection);
	bson_destroy(newobj);
	return 1;
}

static int luafunc_mongo_replace_one_with_jsonstr(lua_State* L) {
	string collection_str = my_luaL_getstring(L, 1);
	string query_str = my_luaL_getstring(L, 2);   // a json string
	string newobj_str = my_luaL_getstring(L, 3);   // a json string
	bson_t *query = bson_new_from_json((const uint8_t*)query_str.c_str(), -1, &mongodat.error);
	if (query == NULL) luaL_error(L, "invalid json");
	bson_t *newobj = bson_new_from_json((const uint8_t*)newobj_str.c_str(), -1, &mongodat.error);
	if (newobj == NULL) luaL_error(L, "invalid json");
	mongoc_collection_t *collection = mongoc_database_get_collection(mongodat.database, collection_str.c_str());
	bson_t reply;
	bool success = mongoc_collection_replace_one(collection, query, newobj, NULL, &reply, &mongodat.error);
	if (success) {
		lua_pushboolean(L, true);
	} else {
		lua_pushnil(L);
	}
	bson_destroy(&reply);
	mongoc_collection_destroy(collection);
	bson_destroy(newobj);
	bson_destroy(query);
	return 1;
}

static int luafunc_mongo_get_one_as_jsonstr(lua_State* L) {
	string collection_str = my_luaL_getstring(L, 1);
	string query_str = my_luaL_getstring(L, 2);   // a json string
	string opts_str = my_luaL_getstring(L, 3);   // a json string
	bson_t *query = bson_new_from_json((const uint8_t*)query_str.c_str(), -1, &mongodat.error);
	if (query == NULL) luaL_error(L, "invalid json");
	bson_t *opts = bson_new_from_json((const uint8_t*)opts_str.c_str(), -1, &mongodat.error);
	if (opts == NULL) luaL_error(L, "invalid json");
	// printf("%s\n", MongoDat::dump(*query).c_str());
	// printf("%s\n", MongoDat::dump(*opts).c_str());
	mongoc_collection_t *collection = mongoc_database_get_collection(mongodat.database, collection_str.c_str());
	mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
	const bson_t *doc;
	if (mongoc_cursor_next(cursor, &doc)) {
		string jsonstr = MongoDat::dump(*doc);
		lua_pushlstring(L, jsonstr.c_str(), jsonstr.size());
	} else {
		// luaL_error(L, "query none");
		lua_pushnil(L);
	}
	mongoc_cursor_destroy(cursor);
	mongoc_collection_destroy(collection);
	bson_destroy(opts);
	bson_destroy(query);
	return 1;
}

static int luafunc_mongo_update_one_with_jsonstr(lua_State* L) {
	string collection_str = my_luaL_getstring(L, 1);
	string query_str = my_luaL_getstring(L, 2);  // a json string
	string update_str = my_luaL_getstring(L, 3);  // a json string
	bson_t *query = bson_new_from_json((const uint8_t*)query_str.c_str(), -1, &mongodat.error);
	if (query == NULL) luaL_error(L, "invalid json");
	bson_t *update = bson_new_from_json((const uint8_t*)update_str.c_str(), -1, &mongodat.error);
	if (update == NULL) luaL_error(L, "invalid json");
	mongoc_collection_t *collection = mongoc_database_get_collection(mongodat.database, collection_str.c_str());
	mongoc_find_and_modify_opts_t *opts = mongoc_find_and_modify_opts_new();
	mongoc_find_and_modify_opts_set_update(opts, update);
	mongoc_find_and_modify_opts_set_flags(opts, MONGOC_FIND_AND_MODIFY_UPSERT);  // create if not existed
	bson_t reply;
	bson_t *fields = BCON_NEW("_id", BCON_INT32(1));  // only get result of _id, small overhead for reply
	mongoc_find_and_modify_opts_set_fields(opts, fields);
	bool success = mongoc_collection_find_and_modify_with_opts(collection, query, opts, &reply, &mongodat.error);
	if (success) {
		bson_iter_t iter;
		bson_iter_t baz;
		if (bson_iter_init(&iter, &reply) && bson_iter_find_descendant(&iter, "value._id", &baz)) {
			const bson_oid_t * id = bson_iter_oid(&baz);
			string id_str = MongoDat::OID2str(*id);
			lua_pushlstring(L, id_str.c_str(), id_str.size());
		} else lua_pushnil(L);  // failed
	} else {
		lua_pushnil(L);  // failed
	}
	bson_destroy(&reply);
	bson_destroy(fields);
	mongoc_find_and_modify_opts_destroy(opts);
	mongoc_collection_destroy(collection);
	bson_destroy(update);
	bson_destroy(query);
	return 1;
}

static int luafunc_load_preamble_ref(lua_State* L) {
	READER_NEEDED
	string _filename = my_luaL_getstring(L, 1);
	const char* filename = _filename.c_str();  // only this is zero-terminated string
	if (MongoDat::isPossibleOID(filename)) {  // try it as OID first
		bson_oid_t id = MongoDat::parseOID(filename);
		if (mongodat.is_gridfs_file_existed(id)) {  // only if file exist, try it
			vector<char> buf = mongodat.get_binary_file(id);
			// printf("buf size: %d\n", buf.size());
			reader.load_refpreamble((const void*)buf.data(), buf.size());
			return 0;
		}
	}
	ifstream test;
	test.open(filename, ios::in);
	if (!test) {  // file not existed
		luaL_error(L, "file \"%s\" not existed", filename);
	}
	test.close();
	reader.load_refpreamble(filename);
	return 0;
}

static int luafunc_set_record(lua_State* L) {
	READER_NEEDED
	bool ifopen = luaL_checkinteger(L, 1);
	data_recorder_enabled.store(ifopen);
	if (ifopen) {
		while (record_filename[0] == '\0') this_thread::sleep_for(10ms);  // wait for started
	} else{
		while (record_filename[0] != '\0') this_thread::sleep_for(10ms);  // wait for stopped
		hostmqtt.publish((string("retroturbo/") + TurboID + "/record_stop").c_str(), NULL, 0);
	}
	string filename = record_filename;
	string id = MongoDat::OID2str(data_recorder_mongo.get_fileID());
	lua_pushlstring(L, id.c_str(), id.size());
	lua_pushlstring(L, filename.c_str(), filename.size());
	return 2;
}

static int luafunc_log_binary_mongoID(lua_State* L) {
	string topic = my_luaL_getstring(L, 1);
	string mongoID = my_luaL_getstring(L, 2);
	if (!MongoDat::isPossibleOID(mongoID.c_str())) luaL_error(L, "invalid mongoID, must be 12byte hex (24 char)");
	bson_oid_t id = MongoDat::parseOID(mongoID.c_str());
	if (!mongodat.is_gridfs_file_existed(id)) luaL_error(L, "mongoID not existed");
	vector<char> buf = mongodat.get_binary_file(id);
	hostmqtt.publish((string("retroturbo/") + TurboID + "/binary/" + topic).c_str(), buf.data(), buf.size());  // transfer binary data
	return 0;
}

static int luafunc_draw_record_mongoID(lua_State* L) {
	string mongoID = my_luaL_getstring(L, 1);
	if (!MongoDat::isPossibleOID(mongoID.c_str())) luaL_error(L, "invalid mongoID, must be 12byte hex (24 char)");
	bson_oid_t id = MongoDat::parseOID(mongoID.c_str());
	if (!mongodat.is_gridfs_file_existed(id)) luaL_error(L, "mongoID not existed");
	const bson_t* metadata = mongodat.metadata_record(id);
	string title = getstringfrombson(metadata, "title", "");
	string x_label = getstringfrombson(metadata, "x_label", "");
	string y_label = getstringfrombson(metadata, "y_label", "");
	string type = getstringfrombson(metadata, "type", "");
	int dimension = getint32frombson(metadata, "dimension", -1);
	float x_unit = getdoublefrombson(metadata, "x_unit", 0);
	float y_unit = getdoublefrombson(metadata, "y_unit", 0);
	float x_bias = getdoublefrombson(metadata, "x_bias", 0);  // x_real = x_bias + x_unit * x
	float y_bias = getdoublefrombson(metadata, "y_bias", 0);
	if (title == "" || x_label == "" || y_label == "" || type == "" || dimension == -1 || x_unit == 0 || y_unit == 0) luaL_error(L, "record not valid, loss title or x_label or y_label or type");
	char dimension_str[32]; sprintf(dimension_str, "%d", dimension);
	char x_unit_str[32]; sprintf(x_unit_str, "%f", x_unit);
	char y_unit_str[32]; sprintf(y_unit_str, "%f", y_unit);
	string draw_name = getstringfrombson(metadata, "plot", "draw");
	vector<char> buf;
	if (lua_gettop(L) == 3) {
		int idx_start = luaL_checkinteger(L, 2);
		int idx_length = luaL_checkinteger(L, 3);
		x_bias += idx_start * x_unit;
		int type_size = 0;
		if (type == "int8_t") type_size = sizeof(int8_t);
		else if (type == "uint8_t") type_size = sizeof(uint8_t);
		else if (type == "int16_t") type_size = sizeof(int16_t);
		else if (type == "uint16_t") type_size = sizeof(uint16_t);
		else if (type == "int32_t") type_size = sizeof(int32_t);
		else if (type == "uint32_t") type_size = sizeof(uint32_t);
		else if (type == "float") type_size = sizeof(float);
		else if (type == "double") type_size = sizeof(double);
		else luaL_error(L, "data type not recognized");
		int idx_multi = type_size * dimension;
		buf = mongodat.get_binary_file_bias_length(id, idx_start * idx_multi, idx_length * idx_multi);
	} else {
		buf = mongodat.get_binary_file(id);
	}
	char x_bias_str[32]; sprintf(x_bias_str, "%f", x_bias);
	char y_bias_str[32]; sprintf(y_bias_str, "%f", y_bias);
	string topic = string("retroturbo/") + TurboID + "/binary/" + draw_name + "/" + luacid + "/" + title + "/" + type  + "/" + dimension_str + "/"
		+ x_unit_str + "/" + x_label + "/" + y_unit_str + "/" + y_label + "/" + x_bias_str + "/" + y_bias_str;
	hostmqtt.publish(topic.c_str(), buf.data(), buf.size());  // transfer binary data
	return 0;
}

static int luafunc_save_to_file(lua_State* L) {
	string filename = my_luaL_getstring(L, 1);
	string content = my_luaL_getstring(L, 2);
	if (filename[0] == '/' || filename.find(":\\") != string::npos) luaL_error(L, "absolute path not allowed for safety reason");
	if (filename.find("..") != string::npos) luaL_error(L, ".. not allowed for safety reason");
	ofstream file;
	file.open(filename);
	if (!file) luaL_error(L, "cannot open file %s to write", filename.c_str());
	file << content;
	file.close();
	return 0;
}

static int luafunc_get_from_file(lua_State* L) {
	string filename = my_luaL_getstring(L, 1);
	if (filename[0] == '/' || filename.find(":\\") != string::npos) luaL_error(L, "absolute path not allowed for safety reason");
	if (filename.find("..") != string::npos) luaL_error(L, ".. not allowed for safety reason");
	ifstream file;
	file.open(filename);
	if (!file) luaL_error(L, "cannot open file %s to read", filename.c_str());
	string content = string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	file.close();
	lua_pushlstring(L, content.c_str(), content.size());
	return 1;
}

static int luafunc_run(lua_State* L) {
	string program = my_luaL_getstring(L, 1);
	if (program.size() == 0) luaL_error(L, "program name needed");
	if (program[0] == '/' || program.find(":\\") != string::npos) luaL_error(L, "absolute path not allowed for safety reason");
	if (program.find("..") != string::npos) luaL_error(L, ".. not allowed for safety reason");
	program = string("./") + program;  // to execute the program
	// for windows users, replace all '/' to '\\'
#if defined(_WIN32)
	size_t pos = 0;
	while ((pos = program.find('/')) != string::npos) program.replace(pos, 1, "\\\\");
	program += ".exe";
#endif
	ifstream test;
	test.open(program, ios::in);
	if (!test) {  // file not existed
		luaL_error(L, "program \"%s\" not existed", program.c_str());
	}
	test.close();
	int n = lua_gettop(L);
	string parameters;
	for (int i=2; i<=n; ++i) {
		parameters += " " + my_luaL_getstring(L, i);
	}
	string invalids = "?:;\"'><,$^#@!*()+=~`";
	for (size_t i=0; i<invalids.size(); ++i) {
		if (parameters.find(invalids[i]) != string::npos) luaL_error(L, "invalid character found ('%c'), do not use them for safety reason", invalids[i]);
	}
	string arguments;
	arguments += string(" ~H") + MQTT_HOST;
	char numbuf[16]; sprintf(numbuf, "%d", MQTT_PORT);
	arguments += string(" ~P") + numbuf;
	arguments += string(" ~U") + MONGO_URL;
	arguments += string(" ~D") + MONGO_DATABASE;
	arguments += string(" ~S") + TurboID;
	arguments += string(" ~C") + luacid;
	// arguments += " ~M" + <give this program a mqtt id?>;
	printf("%s\n", (program + arguments + parameters).c_str());
	int ret = system((program + arguments + parameters).c_str());
	lua_pushinteger(L, ret);
	return 1;
}

int luafunc_generate_random_data(lua_State* L) {
	int length = luaL_checkinteger(L, 1);
	if (length < 0 || length > 65536) luaL_error(L, "data too large");
	string ret;
	struct timeval time;
	gettimeofday(&time, NULL);
	mt19937 gen(1000000 * time.tv_sec + time.tv_usec);
	uniform_int_distribution<unsigned> dis(0, 255);
	char strbuf[16];
	for (int i=0; i<length; ++i) {
		uint8_t c = dis(gen);
		sprintf(strbuf, "%02X", c);
		ret += strbuf;
	}
	lua_pushlstring(L, ret.c_str(), ret.size());
	return 1;
}
