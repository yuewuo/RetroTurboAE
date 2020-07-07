#include "stdio.h"
#include <algorithm>
#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include <vector>
#include <fstream>
#include <thread>
#include <mutex>
#include <functional>
using namespace std;
namespace Mongo {
	#include "mongoc.h"
}  // to avoid namespace conflict
using namespace Mongo;

#ifndef __MONGODAT_H
#define __MONGODAT_H

#define MONGODAT_PUTGET_SIZE 65536

struct BsonOp {
	public:
	bson_t** bsptr;
	// string position;
	// BsonOp* father;
	vector<string> history;  // path, from top to dowm
	BsonOp(bson_t** bsptr);
	bson_iter_t	__iterate_this();
	bool has(const char* dotkey);  // support nested name
	BsonOp operator[] (const char* key);
	BsonOp operator[] (int index);  // this will convert index to string, equal to [iota(index)]
	BsonOp father();
	string dump();
	bson_type_t type();
	bool is(bson_type_t type);
	template<class Type>
	Type value();
	void remove();  // call this on root to destroy it
	void replace(const char* key, const bson_value_t* newval);
	void replace(const char* key, bson_t* newobj);  // newobj will be destroyed for convenience
	BsonOp& operator = (const bson_value_t* newval);
	BsonOp& operator = (bson_t* newobj);  // newobj will be destroyed for convenience
	template<class Type>
	BsonOp& operator = (Type newval);
	BsonOp& assign_document(bson_t* newobj);  // newobj will be destroyed for convenience
	BsonOp& assign_array(bson_t* newobj);  // newobj will be destroyed for convenience
	bool existed();
	void build_up();  // initial current object and all predecessers with empty document
	void build_array();
	bson_t* ptr() const;  // pointing to root document
	bson_t* new_obj();
	function<const bson_oid_t*()> save;
	string dotkey();
	void clear();  // remove all its children
	template<class Type>
	void append(const vector<Type>& arr);
	int count();  // return the amount of chilren
	vector<uint8_t> get_bytes_from_hex_string();
};

// this is NOT thread safe
struct MongoDat {
	static void LibInit();
	static void LibDeinit();
	mutex lock;
	mongoc_database_t *database;
	mongoc_client_t *client;
	mongoc_uri_t *uri;
	bson_error_t error;
	mongoc_gridfs_bucket_t *bucket;
	mongoc_gridfs_t *gridfs;
	bson_value_t file_id;
	MongoDat();
	int open(const char* url = "mongodb://localhost:27017", const char* dbname = "retroturbo");
	int close();
	int upload_file(const char* remote_filename, const char* local_filename);
	int upload_file(bson_oid_t remote_id, const char* remote_filename, const char* local_filename);
	int download_file(const char* local_filename, bson_oid_t remote_fileid);
	bson_oid_t get_fileID();
	static bool isPossibleOID(const char* str);
	static bson_oid_t parseOID(const char* str);
	static string OID2str(const bson_oid_t& id);
	static string dump(const bson_t& data);
	static string dump(const BsonOp& bsonop);
	static string dump(const vector<uint8_t>& data);
	bool is_gridfs_file_existed(const bson_oid_t& id);
	vector<char> get_binary_file(const bson_oid_t& id);
	vector<char> get_binary_file_bias_length(const bson_oid_t& remote_fileid, int bias, int max_length);
	int upload_binary(const char* remote_filename, void* ptr, size_t length, const bson_t *metadata = NULL);
	template<class Type>
	static size_t record_size(size_t Dimension, size_t Length);  // memory structure is something like Type x[Dimension][Length]
	template<class Type>
	int upload_record(const char* remote_filename, Type* ptr, size_t Dimension, size_t Length, bson_t *metadata = NULL, const char* title = NULL
		, float x_unit = 1, const char* x_label = NULL, float y_unit = 1, const char* y_label = NULL);
	template<class Type>
	int upload_scatter(const char* remote_filename, Type* ptr, size_t Length, bson_t *metadata = NULL, const char* title = NULL
		, float x_unit = 1, const char* x_label = NULL, float y_unit = 1, const char* y_label = NULL);
	int delete_gridfs_file(const bson_oid_t& id);
	const bson_t* metadata_record(const bson_oid_t& id);
	string typeof_record(const bson_oid_t& id);
	int get_dimension_length_record(const bson_oid_t& id, size_t *Dimension, size_t *Length);
	string get_x_label_record(const bson_oid_t& id);
	string get_y_label_record(const bson_oid_t& id);
	string get_title_record(const bson_oid_t& id);
	bson_t* get_metadata_record(const bson_oid_t& id);
	int save_metadata_record(const bson_oid_t& id, bson_t *metadata);
	template<class Type>
	int get_data_record(const bson_oid_t& id, Type* ptr, size_t size);
	vector<double> get_scaled_data_record(const bson_oid_t& id);
	bson_t* get_file(const char* collection_str, const bson_oid_t& id);
	BsonOp get_bsonop(const char* collection_str, const bson_oid_t& id);
	BsonOp get_bsonop(const char* collection_str, const char* id_str);
	vector<bson_oid_t> get_all_id(const char* collection_str, const bson_t* query = NULL);
};

#endif

#ifdef MONGODAT_IMPLEMENTATION
#undef MONGODAT_IMPLEMENTATION


template<> int32_t BsonOp::value() { bson_iter_t iter = __iterate_this(); assert(BSON_ITER_HOLDS_INT32(&iter)); return bson_iter_int32(&iter); }
// template<> int64_t BsonOp::value() { bson_iter_t iter = __iterate_this(); assert(BSON_ITER_HOLDS_INT64(&iter)); return bson_iter_int64(&iter); }
template<> double BsonOp::value() { bson_iter_t iter = __iterate_this(); assert(BSON_ITER_HOLDS_DOUBLE(&iter)); return bson_iter_double(&iter); }
template<> string BsonOp::value() { bson_iter_t iter = __iterate_this(); assert(BSON_ITER_HOLDS_UTF8(&iter)); 
	uint32_t length; const char* str = bson_iter_utf8(&iter, &length); string ret; ret.resize(length); memcpy((void*)ret.data(), str, length); return ret; }
template<> bson_oid_t BsonOp::value() { bson_iter_t iter = __iterate_this(); assert(BSON_ITER_HOLDS_OID(&iter)); return *bson_iter_oid(&iter); }

template<> void BsonOp::append(const vector<string>& arr) {
	bson_t *subarr = new bson_t; bson_init(subarr); bson_t *newobj = new_obj(); char buf[32]; int idx = count();
	for (auto it = arr.begin(); it != arr.end(); ++it) { sprintf(buf, "%d", idx++); BSON_APPEND_UTF8(subarr, buf, it->c_str()); }
	assert(bson_concat(newobj, subarr) && "concat failed"); assign_array(newobj); bson_destroy(subarr); delete subarr; delete newobj;
}
template<> void BsonOp::append(const vector<int32_t>& arr) {
	bson_t *subarr = new bson_t; bson_init(subarr); bson_t *newobj = new_obj(); char buf[32]; int idx = count(); 
	for (auto it = arr.begin(); it != arr.end(); ++it) { sprintf(buf, "%d", idx++); BSON_APPEND_INT32(subarr, buf, *it); }
	assert(bson_concat(newobj, subarr) && "concat failed"); assign_array(newobj); bson_destroy(subarr); delete subarr; delete newobj;
}
template<> void BsonOp::append(const vector<double>& arr) {
	bson_t *subarr = new bson_t; bson_init(subarr); bson_t *newobj = new_obj(); char buf[32]; int idx = count(); 
	for (auto it = arr.begin(); it != arr.end(); ++it) { sprintf(buf, "%d", idx++); BSON_APPEND_DOUBLE(subarr, buf, *it); }
	assert(bson_concat(newobj, subarr) && "concat failed"); assign_array(newobj); bson_destroy(subarr); delete subarr; delete newobj;
}

template<> BsonOp& BsonOp::operator = (int32_t newval) { bson_value_t val; val.value_type = BSON_TYPE_INT32; val.value.v_int32 = newval; 
	*this=(const bson_value_t*)(&val); return *this;
}
template<> BsonOp& BsonOp::operator = (double newval) { bson_value_t val; val.value_type = BSON_TYPE_DOUBLE; val.value.v_double = newval; 
	*this=(const bson_value_t*)(&val); return *this;
}
template<> BsonOp& BsonOp::operator = (string newval) { bson_value_t val; val.value_type = BSON_TYPE_UTF8; 
	val.value.v_utf8.str = (char*)newval.c_str(), val.value.v_utf8.len = newval.size(); *this=(const bson_value_t*)(&val); return *this;
}
template<> BsonOp& BsonOp::operator = (const char* newval) { bson_value_t val; val.value_type = BSON_TYPE_UTF8; 
	val.value.v_utf8.str = (char*)newval, val.value.v_utf8.len = strlen(newval); *this=(const bson_value_t*)(&val); return *this;
}

bson_t* BsonOp::ptr() const {
	return *bsptr;
}

vector<uint8_t> BsonOp::get_bytes_from_hex_string() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	assert(existed() && "cannot convert non-existed one");
	vector<uint8_t> bs;
	assert(type() == BSON_TYPE_UTF8 && "raw bytes must convert from string");
	string hex = value<string>();
	assert(hex.size() % 2 == 0 && "invalid hex string");
	for (size_t i=0; i<hex.size(); i += 2) {
		char H = hex[i], L = hex[i+1];
#define ASSERT_HEX_VALID_CHAR_TMP(x) assert(((x>='0'&&x<='9')||(x>='a'&&x<='z')||(x>='A'&&x<='Z')) && "invalid char");
		ASSERT_HEX_VALID_CHAR_TMP(H) ASSERT_HEX_VALID_CHAR_TMP(L)
#undef ASSERT_HEX_VALID_CHAR_TMP
#define c2bs(x) (x>='0'&&x<='9'?(x-'0'):((x>='a'&&x<='f')?(x-'a'+10):((x>='A'&&x<='F')?(x-'A'+10):(0))))
		char b = (c2bs(H) << 4) | c2bs(L);
#undef c2bs
		bs.push_back(b);
	}
	return bs;
}

string MongoDat::dump(const BsonOp& bsonop) {
	return dump(*bsonop.ptr());
}

string MongoDat::dump(const vector<uint8_t>& data) {
	string ret;
	char strbuf[16];
	for (size_t i=0; i<data.size(); ++i) {
		uint8_t c = data[i];
		sprintf(strbuf, "%02X", c);
		ret += strbuf;
	}
	return ret;
}

BsonOp BsonOp::father() {
	assert(!history.empty() && "root object does not have father");
	BsonOp f = *this;
	f.history.erase(f.history.begin());
	return f;
}

string BsonOp::dotkey() {
	string abspath = "";
	for (size_t i=0; i<history.size(); ++i) {
		if (abspath.empty()) abspath = history[i];
		else abspath = history[i] + "." + abspath;
	}
	return abspath;
}

int BsonOp::count() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	assert(existed() && "cannot count non-existed one");
	int cnt = 0;
	if (history.empty()) {  // this is root object
		bson_iter_t iter;
		assert(bson_iter_init(&iter, *bsptr));
		while (bson_iter_next(&iter)) ++cnt;
	} else {
		bson_iter_t root, found, iter;
		assert(bson_iter_init(&root, *bsptr) && bson_iter_find_descendant(&root, dotkey().c_str(), &found));
		bson_iter_recurse(&found, &iter);
		while (bson_iter_next(&iter)) ++cnt;
	}
	return cnt;
}

void BsonOp::clear() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	assert(existed() && "cannot clear non-existed one");
	if (history.empty()) {
		bson_t* tmp = new bson_t;
		bson_init(tmp);
		bson_destroy(*bsptr);
		delete *bsptr;
		*bsptr = tmp;
	} else {
		bson_t* tmp = new bson_t;
		bson_init(tmp);
		father().replace(history[0].c_str(), tmp);
		delete tmp;
	}
}

void BsonOp::replace(const char* key, const bson_value_t* newval) {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	if (history.empty()) {  // this is root object
		bson_t* tmp = new bson_t;
		bson_init(tmp);
		bson_copy_to_excluding_noinit(*bsptr, tmp, key, NULL);
		if (newval) BSON_APPEND_VALUE(tmp, key, newval);
		bson_destroy(*bsptr);
		delete *bsptr;
		*bsptr = tmp;
	} else {
		bson_t* newobj = new bson_t;
		bson_init(newobj);
		vector<string> path;
		path.push_back(key);
		path.insert(path.end(), history.begin(), history.end());
		string nowpath = "";
		bson_t *child = NULL;
		for (size_t i=0; i<path.size()-1; ++i) {
			bson_t *parent = new bson_t;
			bson_init(parent);
			string name = path[i];
			string abspath = "";
			for (size_t j=i+1; j<path.size(); ++j) {
				if (abspath.empty()) abspath = path[j];
				else abspath = path[j] + "." + abspath;
			}
			bson_iter_t root, found, iter;
			assert(bson_iter_init(&root, *bsptr) && bson_iter_find_descendant(&root, abspath.c_str(), &found));
			bson_iter_recurse(&found, &iter);
			while (bson_iter_next(&iter)) {
				if (name != bson_iter_key(&iter)) BSON_APPEND_VALUE(parent, bson_iter_key(&iter), bson_iter_value(&iter));
			}
			if (i == 0) {
				if (newval) BSON_APPEND_VALUE(parent, name.c_str(), newval);
				child = parent;
			} else {
				BSON_APPEND_DOCUMENT(parent, name.c_str(), child);
				bson_destroy(child);
				delete child;
				child = parent;
			}
		}
		bson_iter_t iter;
		bson_iter_init(&iter, *bsptr);
		string name = path.back();
		while (bson_iter_next(&iter)) {
			if (name != bson_iter_key(&iter)) BSON_APPEND_VALUE(newobj, bson_iter_key(&iter), bson_iter_value(&iter));
		}
		if (path.size() == 1) {
			if (newval) BSON_APPEND_VALUE(newobj, name.c_str(), newval);
		} else {
			BSON_APPEND_DOCUMENT(newobj, name.c_str(), child);
			bson_destroy(child);
			delete child;
		}
		bson_destroy(*bsptr);
		delete *bsptr;
		*bsptr = newobj;
	}
}

bson_t* BsonOp::new_obj() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	assert(existed() && "cannot new object on non-existed one");
	bson_t* newobj = new bson_t;
	bson_init(newobj);
	if (history.empty()) {
		bson_iter_t iter;
		assert(bson_iter_init(&iter, *bsptr));
		while (bson_iter_next(&iter)) {
			BSON_APPEND_VALUE(newobj, bson_iter_key(&iter), bson_iter_value(&iter));
		}

	} else {
		bson_iter_t root, found, iter;
		assert(bson_iter_init(&root, *bsptr) && bson_iter_find_descendant(&root, dotkey().c_str(), &found));
		bson_iter_recurse(&found, &iter);
		while (bson_iter_next(&iter)) {
			BSON_APPEND_VALUE(newobj, bson_iter_key(&iter), bson_iter_value(&iter));
		}
	}
	return newobj;
}

void BsonOp::replace(const char* key, bson_t* newobj) {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	bson_t tmp;
	bson_init(&tmp);
	BSON_APPEND_DOCUMENT(&tmp, "a", newobj);
	bson_iter_t iter;
	assert(bson_iter_init_find(&iter, &tmp, "a") && "strange, cannot found document just inserted");
	replace(key, bson_iter_value(&iter));
	bson_destroy(&tmp);
	bson_destroy(newobj);  // also destroy input
}

BsonOp& BsonOp::assign_array(bson_t* newobj) {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	assert(!history.empty() && "root document cannot be array");
	bson_t tmp;
	bson_init(&tmp);
	BSON_APPEND_ARRAY(&tmp, "a", newobj);
	bson_iter_t iter;
	assert(bson_iter_init_find(&iter, &tmp, "a") && "strange, cannot found array just inserted");
	father().replace(history[0].c_str(), bson_iter_value(&iter));
	bson_destroy(&tmp);
	bson_destroy(newobj);  // also destroy input
	return *this;
}

BsonOp& BsonOp::assign_document(bson_t* newobj) {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	return *this = newobj;
}

BsonOp& BsonOp::operator = (bson_t* newobj) {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	if (history.empty()) {  // this is root object
		bson_t* tmp = new bson_t;
		bson_copy_to(newobj, tmp);
		bson_destroy(*bsptr);
		delete *bsptr;
		*bsptr = tmp;
		bson_destroy(newobj);  // also destroy input
	} else {
		father().build_up();
		bson_t tmp;
		bson_init(&tmp);
		BSON_APPEND_DOCUMENT(&tmp, "a", newobj);
		bson_iter_t iter;
		assert(bson_iter_init_find(&iter, &tmp, "a") && "strange, cannot found document just inserted");
		father().replace(history[0].c_str(), bson_iter_value(&iter));
		bson_destroy(&tmp);
		bson_destroy(newobj);  // also destroy input
	}
	return *this;
}

BsonOp& BsonOp::operator= (const bson_value_t* newval) {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	assert(!history.empty() && "cannot assign bson_value_t on root document, use bson_t instead");
	assert(newval && "value cannot be NULL, if you want to remove this field, call remove instead");
	father().build_up();
	father().replace(history[0].c_str(), newval);
	return *this;
}

void BsonOp::remove() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	if (history.empty()) {  // destroy document
		// bson_destroy(*bsptr);  // often crash here, don't know why... this will cause memory leak, repair this later
		delete *bsptr;
		*bsptr = NULL;
		delete bsptr;
		bsptr = NULL;
	} else {
		if (!existed()) return;  // has been removed
		father().replace(history[0].c_str(), (const bson_value_t*)NULL);
	}
}

void BsonOp::build_up() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	if (history.empty()) return;
	if (existed()) {
		assert(type() == BSON_TYPE_DOCUMENT && "already has this field but is not object");
		return;
	}
	vector<string> path;
	BsonOp lastexisted = *this;
	while (!lastexisted.existed()) {
		path.push_back(lastexisted.history[0]);
		lastexisted.history.erase(lastexisted.history.begin());
	}
	assert((lastexisted.history.empty() || lastexisted.type() == BSON_TYPE_DOCUMENT) && "cannot add field to non-document");
	bson_t *child = new bson_t;
	bson_init(child);
	for (size_t i=0; i<path.size()-1; ++i) {
		bson_t *parent = new bson_t;
		bson_init(parent);
		string name = path[i];
		BSON_APPEND_DOCUMENT(parent, name.c_str(), child);
		bson_destroy(child);
		delete child;
		child = parent;
	}
	lastexisted.replace(path.back().c_str(), child);  // has destroyed child
	delete child;
}

void BsonOp::build_array() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	assert(!history.empty() && "cannot build array on root document");
	if (existed()) {
		assert(type() == BSON_TYPE_ARRAY && "already has this field but is not array");
		return;
	}
	BsonOp f = father();
	bson_t *child = f.new_obj();
	bson_t arr = BSON_INITIALIZER;
	BSON_APPEND_ARRAY(child, history[0].c_str(), &arr);
	if (f.history.empty()) {  // father is root
		f = child;
	} else {
		f.father().replace(f.history[0].c_str(), child);
	}
	bson_destroy(&arr);
	delete child;
}

bson_type_t BsonOp::type() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	bson_iter_t iter = __iterate_this();
	const bson_value_t* val = bson_iter_value(&iter);
	return val->value_type;
}

bool BsonOp::is(bson_type_t _type) {
	return type() == _type;
}

bson_iter_t	BsonOp::__iterate_this() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	bson_iter_t iter;
	if (history.empty()) {  // this is root object
		assert(bson_iter_init(&iter, *bsptr));
	} else {
		bson_iter_t root;
		assert(bson_iter_init(&root, *bsptr) && bson_iter_find_descendant(&root, dotkey().c_str(), &iter));
	}
	return iter;
}

BsonOp::BsonOp(bson_t** _bsptr) {
	bsptr = _bsptr;
	save = []() -> const bson_oid_t* { assert(0 && "save method is not set"); return NULL; };
}

bool BsonOp::has(const char* dotkey) {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	if (!existed()) return false;
	bson_iter_t iter = __iterate_this();
	bson_iter_t doc;
	if (bson_iter_find_descendant(&iter, dotkey, &doc)) return true;
	return false;
}

BsonOp BsonOp::operator[] (const char* key) {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	// assert(string(key).find('.') == string::npos && "field name cannot include . due to avoid strange behavior");
	BsonOp op = *this;
	string str = key;
	size_t pos1 = 0, pos2 = str.find('.');
	while (pos2 != string::npos) {
		if (pos2 != pos1) op.history.insert(op.history.begin(), str.substr(pos1, pos2-pos1));
		pos1 = pos2 + 1;
		pos2 = str.find('.', pos1);
	} if (pos1 < str.size()) op.history.insert(op.history.begin(), str.substr(pos1));
	return op;
}

BsonOp BsonOp::operator[] (int index) {
	char buf[32]; sprintf(buf, "%d", index);
	return operator[](buf);
}

bool BsonOp::existed() {
	if (!bsptr || !*bsptr) return false;
	if (!history.empty()) {
		bson_iter_t root, iter;
		assert(bson_iter_init(&root, *bsptr));
		return bson_iter_find_descendant(&root, dotkey().c_str(), &iter);
	} return true;  // root always exist
}

string BsonOp::dump() {
	assert(bsptr && *bsptr && "invalid BsonOp, object possibly has been removed");
	assert(existed() && "object not existed");
	if (history.empty()) {
		return MongoDat::dump(**bsptr);
	}
	bson_t b;
	bson_init(&b);
	bson_iter_t iter = __iterate_this();
	BSON_APPEND_VALUE(&b, "v", bson_iter_value(&iter));
	string ret = MongoDat::dump(b);
	ret = ret.substr(8, ret.size()-8-2);
	bson_destroy(&b);
	return ret;
}

vector<bson_oid_t> MongoDat::get_all_id(const char* collection_str, const bson_t* ext_query) {
	bson_t query; if (!ext_query) bson_init(&query);
	bson_t *opts = BCON_NEW("projection", "{", "_id", BCON_BOOL(true), "}");
	mongoc_collection_t *collection = mongoc_database_get_collection(database, collection_str);
	mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(collection, ext_query ? ext_query : &query, opts, NULL);
	vector<bson_oid_t> ret;
	const bson_t *doc;
	bson_iter_t iter;
	while (mongoc_cursor_next(cursor, &doc)) {
		assert(bson_iter_init(&iter, doc) && bson_iter_next(&iter) && BSON_ITER_HOLDS_OID(&iter));
		bson_oid_t id = *bson_iter_oid(&iter);
		ret.push_back(id);
	}
	mongoc_cursor_destroy(cursor);
	mongoc_collection_destroy(collection);
	if (!ext_query) bson_destroy(&query);
	bson_destroy(opts);
	return ret;
}

BsonOp MongoDat::get_bsonop(const char* collection_str, const char* id_str) {
	bson_oid_t id = MongoDat::parseOID(id_str);
	return get_bsonop(collection_str, id);
}

BsonOp MongoDat::get_bsonop(const char* collection_str, const bson_oid_t& id) {
	bson_t** mem = new bson_t*;
	*mem = get_file(collection_str, id);
	BsonOp op = BsonOp(mem);
	MongoDat* mongodat = this;
	string str_collection = collection_str;
	op.save = [mongodat, str_collection, mem]() -> const bson_oid_t* {
		BsonOp tmp(mem);
		assert(tmp["_id"].existed() && tmp["_id"].type() == BSON_TYPE_OID && "file does not have id, cannot save");
		string query_str = string("{\"_id\":{\"$oid\":\"") + OID2str(tmp["_id"].value<bson_oid_t>()) + "\"}}";
		bson_t *query = bson_new_from_json((const uint8_t*)query_str.c_str(), -1, &mongodat->error);
		assert(query);
		bson_t *update = *mem;
		mongoc_collection_t *collection = mongoc_database_get_collection(mongodat->database, str_collection.c_str());
		mongoc_find_and_modify_opts_t *opts = mongoc_find_and_modify_opts_new();
		mongoc_find_and_modify_opts_set_update(opts, update);
		mongoc_find_and_modify_opts_set_flags(opts, MONGOC_FIND_AND_MODIFY_UPSERT);  // create if not existed
		bson_t reply;
		bson_t *fields = BCON_NEW("_id", BCON_INT32(1));  // only get result of _id, small overhead for reply
		mongoc_find_and_modify_opts_set_fields(opts, fields);
		const bson_oid_t * uid = NULL;
		bool success = mongoc_collection_find_and_modify_with_opts(collection, query, opts, &reply, &mongodat->error);
		if (success) {
			bson_iter_t iter;
			bson_iter_t baz;
			if (bson_iter_init(&iter, &reply) && bson_iter_find_descendant(&iter, "value._id", &baz)) {
				uid = bson_iter_oid(&baz);
			}
		}
		bson_destroy(&reply);
		bson_destroy(fields);
		mongoc_find_and_modify_opts_destroy(opts);
		mongoc_collection_destroy(collection);
		bson_destroy(update);
		bson_destroy(query);
		return uid;
	};
	return op;
}

bson_t* MongoDat::get_file(const char* collection_str, const bson_oid_t& id) {
	string query_str = string("{\"_id\":{\"$oid\":\"") + OID2str(id) + "\"}}";
	bson_t *query = bson_new_from_json((const uint8_t*)query_str.c_str(), -1, &error);
	bson_t *opts = BCON_NEW("limit", BCON_INT32(1));
	mongoc_collection_t *collection = mongoc_database_get_collection(database, collection_str);
	mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(collection, query, opts, NULL);
	const bson_t *doc;
	bson_t *reply = NULL;
	if (mongoc_cursor_next(cursor, &doc)) {
		reply = new bson_t;
		bson_copy_to(doc, reply);
	}
	mongoc_cursor_destroy(cursor);
	mongoc_collection_destroy(collection);
	bson_destroy(opts);
	bson_destroy(query);
	return reply;
}

template<class Type>
int MongoDat::get_data_record(const bson_oid_t& id, Type* ptr, size_t size) {
	size_t datsize = sizeof(Type) * size;
	vector<char> buf = get_binary_file(id);
	assert(buf.size() == datsize && "get_data_record buffer length error");
	return 0;
}

void bson_delete_field(bson_t* &obj, const char *fieldname) {
	bson_t* tmp = new bson_t;
	bson_init(tmp);
	bson_copy_to_excluding_noinit(obj, tmp, fieldname, NULL);
	bson_destroy(obj);
	delete obj;
	obj = tmp;
}

bson_t* MongoDat::get_metadata_record(const bson_oid_t& id) {
	bson_t* tmp = new bson_t;
	bson_copy_to(metadata_record(id), tmp);
	return tmp;
}

int MongoDat::save_metadata_record(const bson_oid_t& id, bson_t *metadata) {
	bson_t filter;
	bson_init(&filter);
	BSON_APPEND_OID(&filter, "_id", &id);  // query with ID
	mongoc_gridfs_file_list_t* list = mongoc_gridfs_find_with_opts(gridfs, &filter, NULL);
	mongoc_gridfs_file_t *file = mongoc_gridfs_file_list_next(list);
	assert(file && "record just inserted not existed, strange");
	mongoc_gridfs_file_set_metadata(file, metadata);
	mongoc_gridfs_file_save(file);
	mongoc_gridfs_file_destroy(file);
	mongoc_gridfs_file_list_destroy(list);
	bson_destroy(&filter);
	return 0;
}

string cptr2string(const char* buf, uint32_t length) {
	string str;
	str.resize(length);
	for (int i=0; i<(int)length; ++i) str[i] = buf[i];
	return str;
}

string getstringfrombson(const bson_t* bsondat, const char* key, const char* defaultstr) {
	bson_iter_t iter;
	if (bson_iter_init_find(&iter, bsondat, key) && BSON_ITER_HOLDS_UTF8(&iter)) {
		uint32_t length;
		const char* data = bson_iter_utf8(&iter, &length);
		return cptr2string(data, length);
	}
	return defaultstr;
}

int32_t getint32frombson(const bson_t* bsondat, const char* key, int32_t defaultval) {
	bson_iter_t iter;
	if (bson_iter_init_find(&iter, bsondat, key) && BSON_ITER_HOLDS_INT32(&iter)) {
		return bson_iter_int32(&iter);
	}
	return defaultval;
}

double getdoublefrombson(const bson_t* bsondat, const char* key, double defaultval) {
	bson_iter_t iter;
	if (bson_iter_init_find(&iter, bsondat, key) && BSON_ITER_HOLDS_DOUBLE(&iter)) {
		return bson_iter_double(&iter);
	}
	return defaultval;
}

vector<double> MongoDat::get_scaled_data_record(const bson_oid_t& id) {
	vector<double> buffer;
	const bson_t* metadata = metadata_record(id);
	int dimension = getint32frombson(metadata, "dimension", 0);
	int length = getint32frombson(metadata, "length", 0);
	int data_cnt = dimension * length;
	assert(dimension && length && "dimension or length error");
	double y_unit = getdoublefrombson(metadata, "y_unit", 1);
	string type = getstringfrombson(metadata, "type", "");
	if (type == "") { assert(0 && "type not provided"); }
	else if (type == "int16_t") {
		vector<char> ori = get_binary_file(id);
		assert(ori.size() == data_cnt * sizeof(int16_t) && "data size not matched, invalid record in database");
		const int16_t* buf = (const int16_t*)ori.data();
		buffer.resize(data_cnt);
		for (int i=0; i<data_cnt; ++i) buffer[i] = y_unit * buf[i];
	}
	else { assert(0 && "unknown type, cannot get scaled data"); }
	fflush(stdout);
	return buffer;
}

string MongoDat::get_title_record(const bson_oid_t& id) {
	const bson_t* metadata = metadata_record(id);
	return getstringfrombson(metadata, "title", "null");
}

string MongoDat::get_x_label_record(const bson_oid_t& id) {
	const bson_t* metadata = metadata_record(id);
	return getstringfrombson(metadata, "x_label", "null");
}

string MongoDat::get_y_label_record(const bson_oid_t& id) {
	const bson_t* metadata = metadata_record(id);
	return getstringfrombson(metadata, "y_label", "null");
}

int MongoDat::get_dimension_length_record(const bson_oid_t& id, size_t *Dimension, size_t *Length) {
	const bson_t* metadata = metadata_record(id);
	bson_iter_t iter;
	if (bson_iter_init_find(&iter, metadata, "dimension") && BSON_ITER_HOLDS_INT32(&iter)) {
		*Dimension = bson_iter_int32(&iter);
		if (bson_iter_init_find(&iter, metadata, "length") && BSON_ITER_HOLDS_INT32(&iter)) {
			*Length = bson_iter_int32(&iter);
			return 0;
		}
		return -2;
	}
	return -1;
}

string MongoDat::typeof_record(const bson_oid_t& id) {
	const bson_t* metadata = metadata_record(id);
	bson_iter_t iter;
	if (bson_iter_init_find(&iter, metadata, "type") && BSON_ITER_HOLDS_UTF8(&iter)) {
		uint32_t length;
		const char* data = bson_iter_utf8(&iter, &length);
		return cptr2string(data, length);
	}
	return "no-type";
}

const bson_t* MongoDat::metadata_record(const bson_oid_t& id) {
	bson_t filter;
	bson_init(&filter);
	BSON_APPEND_OID(&filter, "_id", &id);  // query with ID
	mongoc_gridfs_file_list_t* list = mongoc_gridfs_find_with_opts(gridfs, &filter, NULL);
	mongoc_gridfs_file_t *file = mongoc_gridfs_file_list_next(list);
	assert(file && "record not existed");
	const bson_t* metadata = mongoc_gridfs_file_get_metadata(file);
	mongoc_gridfs_file_destroy(file);
	mongoc_gridfs_file_list_destroy(list);
	bson_destroy(&filter);
	return metadata;
}

template<class Type>
size_t MongoDat::record_size(size_t Dimension, size_t Length) {
	return sizeof(Type) * Dimension * Length;
}

#define UPLOAD_RECORD_ADD_METADATA(type, typename) \
	time_t timep; time(&timep); \
	BSON_APPEND_TIMESTAMP(metadata, "timestamp", (uint32_t)timep, 0); \
	BSON_APPEND_INT32(metadata, "dimension", Dimension); \
	BSON_APPEND_INT32(metadata, "length", Length); \
	BSON_APPEND_INT32(metadata, "typesize", sizeof(type)); \
	BSON_APPEND_UTF8(metadata, "type", typename)

#define KNOWN_TYPED_UPLOAD_RECORD(x) \
template<> \
int MongoDat::upload_record(const char* remote_filename, x* ptr, size_t Dimension, size_t Length, bson_t *metadata, const char* title \
		, float x_unit, const char* x_label, float y_unit, const char* y_label) { \
	size_t datsize = record_size<x>(Dimension, Length); \
	bool new_metadata = (metadata == NULL); \
	if (new_metadata) { metadata = new bson_t; bson_init(metadata); } \
	UPLOAD_RECORD_ADD_METADATA(x, #x); \
	BSON_APPEND_UTF8(metadata, "title", title ? title : "no-title"); \
	BSON_APPEND_DOUBLE(metadata, "x_unit", x_unit); \
	BSON_APPEND_UTF8(metadata, "x_label", x_label ? x_label : "unknown"); \
	BSON_APPEND_DOUBLE(metadata, "y_unit", y_unit); \
	string str; \
	if (!y_label) { str = "unknown"; for (size_t i=1; i<Dimension; ++i) str += ",unknown"; } \
	BSON_APPEND_UTF8(metadata, "y_label", y_label ? y_label : str.c_str()); \
	upload_binary(remote_filename, (void*)ptr, datsize, metadata); \
	if (new_metadata) { bson_destroy(metadata); delete metadata; } \
	return 0; \
}

// these are all the type supported by JavaScript TypedArray
KNOWN_TYPED_UPLOAD_RECORD(int8_t);
KNOWN_TYPED_UPLOAD_RECORD(uint8_t);
KNOWN_TYPED_UPLOAD_RECORD(int16_t);
KNOWN_TYPED_UPLOAD_RECORD(uint16_t);
KNOWN_TYPED_UPLOAD_RECORD(int32_t);
KNOWN_TYPED_UPLOAD_RECORD(uint32_t);
KNOWN_TYPED_UPLOAD_RECORD(float);
KNOWN_TYPED_UPLOAD_RECORD(double);

template<class Type>
int MongoDat::upload_record(const char* remote_filename, Type* ptr, size_t Dimension, size_t Length, bson_t *metadata, const char* title \
		, float x_unit, const char* x_label, float y_unit, const char* y_label) {
	size_t datsize = record_size<Type>(Dimension, Length); \
	UPLOAD_RECORD_ADD_METADATA(Type, "unknown");
	upload_binary(remote_filename, (void*)ptr, datsize, metadata); \
	return 0;
}

#define KNOWN_TYPED_UPLOAD_SCATTER(x) \
template<> \
int MongoDat::upload_scatter(const char* remote_filename, x* ptr, size_t Length, bson_t *metadata, const char* title \
		, float x_unit, const char* x_label, float y_unit, const char* y_label) { \
	size_t Dimension = 2; \
	size_t datsize = record_size<x>(Dimension, Length); \
	bool new_metadata = (metadata == NULL); \
	if (new_metadata) { metadata = new bson_t; bson_init(metadata); } \
	UPLOAD_RECORD_ADD_METADATA(x, #x); \
	BSON_APPEND_UTF8(metadata, "plot", "scatter"); \
	BSON_APPEND_UTF8(metadata, "title", title ? title : "no-title"); \
	BSON_APPEND_DOUBLE(metadata, "x_unit", x_unit); \
	BSON_APPEND_UTF8(metadata, "x_label", x_label ? x_label : "unknown"); \
	BSON_APPEND_DOUBLE(metadata, "y_unit", y_unit); \
	string str; \
	if (!y_label) { str = "unknown"; for (size_t i=1; i<Dimension; ++i) str += ",unknown"; } \
	BSON_APPEND_UTF8(metadata, "y_label", y_label ? y_label : str.c_str()); \
	upload_binary(remote_filename, (void*)ptr, datsize, metadata); \
	if (new_metadata) { bson_destroy(metadata); delete metadata; } \
	return 0; \
}

// these are all the type supported by JavaScript TypedArray
KNOWN_TYPED_UPLOAD_SCATTER(int8_t);
KNOWN_TYPED_UPLOAD_SCATTER(uint8_t);
KNOWN_TYPED_UPLOAD_SCATTER(int16_t);
KNOWN_TYPED_UPLOAD_SCATTER(uint16_t);
KNOWN_TYPED_UPLOAD_SCATTER(int32_t);
KNOWN_TYPED_UPLOAD_SCATTER(uint32_t);
KNOWN_TYPED_UPLOAD_SCATTER(float);
KNOWN_TYPED_UPLOAD_SCATTER(double);

template<class Type>
int MongoDat::upload_scatter(const char* remote_filename, Type* ptr, size_t Length, bson_t *metadata, const char* title \
		, float x_unit, const char* x_label, float y_unit, const char* y_label) {
	size_t Dimension = 2;
	size_t datsize = record_size<Type>(Dimension, Length); \
	UPLOAD_RECORD_ADD_METADATA(Type, "unknown");
	upload_binary(remote_filename, (void*)ptr, datsize, metadata); \
	return 0;
}

int MongoDat::upload_binary(const char* remote_filename, void* buf, size_t length, const bson_t *metadata) {
	assert(database && "database not opened");
	assert(remote_filename && "NULL filename");
	assert(buf && "NULL ptr");
	mongoc_stream_t* upload_stream = mongoc_gridfs_bucket_open_upload_stream(bucket, remote_filename, NULL, &file_id, &error);
	if (!upload_stream) return 1;
	if (length != (size_t)mongoc_stream_write(upload_stream, buf, length, -1)) return 2;  // write failed
	// if (0 != mongoc_stream_flush(upload_stream)) return 3;  // flush failed
	if (0 != mongoc_stream_close(upload_stream)) return 4;  // close failed
	mongoc_stream_destroy(upload_stream);
	if (metadata) {
		bson_t filter;
		bson_init(&filter);
		bson_oid_t id = get_fileID();
		BSON_APPEND_OID(&filter, "_id", &id);  // query with ID
		mongoc_gridfs_file_list_t* list = mongoc_gridfs_find_with_opts(gridfs, &filter, NULL);
		mongoc_gridfs_file_t *file = mongoc_gridfs_file_list_next(list);
		assert(file && "record just inserted not existed, strange");
		mongoc_gridfs_file_set_metadata(file, metadata);
		mongoc_gridfs_file_save(file);
		mongoc_gridfs_file_destroy(file);
		mongoc_gridfs_file_list_destroy(list);
		bson_destroy(&filter);
	}
	return 0;
}

void MongoDat::LibInit() {
	Mongo::mongoc_init();
}

void MongoDat::LibDeinit() {
	Mongo::mongoc_cleanup();
}

MongoDat::MongoDat() {
	database = NULL;
	client = NULL;
	uri = NULL;
}

string MongoDat::dump(const bson_t& data) {
	char* str = bson_as_canonical_extended_json(&data, NULL);
	string ret(str);
	bson_free(str);
	return ret;
}

int MongoDat::open(const char* url, const char* dbname) {
	// printf("url: %s\n", url);
	assert(!database && "mongo has been opened");
	uri = mongoc_uri_new_with_error(url, &error);
	if (!uri) {
		printf("error message: %s\n", error.message);
		assert(0 && "uri failed");
	}
	client = mongoc_client_new_from_uri(uri);
	assert(client && "client failed");
	mongoc_client_set_error_api(client, 2);
	// ping the server
	bson_t *command = BCON_NEW("ping", BCON_INT32(1)), reply;
	if (!mongoc_client_command_simple (client, "admin", command, NULL, &reply, &error)) {
		printf("error message: %s\n", error.message);
		assert(0 && "database ping failed");
	}
	bson_destroy(&reply);
	bson_destroy(command);
	// continue initialization
	database = mongoc_client_get_database(client, dbname);
	assert(database && "database failed");
	bson_t *opts = bson_new();  // memory leak here, but OK
	bucket = mongoc_gridfs_bucket_new(database, opts, NULL, &error);
	if (!bucket) {
		printf("error message: %s\n", error.message);
		assert(0 && "bucket failed");
	}
	gridfs = mongoc_client_get_gridfs(client, dbname, "fs", &error);
	return 0;
}

int MongoDat::close() {
	// if (bucket) mongoc_gridfs_bucket_destroy(bucket);
	if (database) mongoc_database_destroy(database);
	if (client) mongoc_client_destroy(client);
	if (uri) mongoc_uri_destroy(uri);
	return 0;
}

bson_oid_t MongoDat::get_fileID() {
	if (file_id.value_type == Mongo::BSON_TYPE_OID) return file_id.value.v_oid;
	bson_oid_t tmp;
	memset(&tmp, 0, sizeof(tmp));
	return tmp;
}

int MongoDat::upload_file(const char* remote_filename, const char* local_filename) {
	assert(database && "database not opened");
	assert(remote_filename && local_filename && "NULL filename");
	ifstream input(local_filename, ios::binary);
	input.seekg (0, input.end);
	int filesize = input.tellg(), writtenlen = 0;
	input.seekg (0, input.beg);
	char buf[MONGODAT_PUTGET_SIZE];
	mongoc_stream_t* upload_stream = mongoc_gridfs_bucket_open_upload_stream(bucket, remote_filename, NULL, &file_id, &error);
	if (!upload_stream) return 1;
	while (writtenlen < filesize) {
		int writesize = MONGODAT_PUTGET_SIZE;
		if (writesize > filesize - writtenlen) writesize = filesize - writtenlen;  // the last small one
		// printf("read %d bytes\n", writesize);
		if (!input.read(buf, writesize)) return 2;  // read failed
		if (writesize != mongoc_stream_write(upload_stream, buf, writesize, -1)) return 3;  // write failed
		writtenlen += writesize;
	}
	input.close();
	mongoc_stream_close(upload_stream);
	mongoc_stream_destroy(upload_stream);
	return 0;
}

int MongoDat::download_file(const char* local_filename, bson_oid_t remote_fileid) {
	assert(database && "database not opened");
	assert(local_filename && "NULL filename");
	file_id.value_type = Mongo::BSON_TYPE_OID;
	file_id.value.v_oid = remote_fileid;
	mongoc_stream_t *download_stream = mongoc_gridfs_bucket_open_download_stream(bucket, &file_id, NULL);
	if (!download_stream) return 1;
	char buf[MONGODAT_PUTGET_SIZE];
	int ret, count = 0;
	ofstream output(local_filename, ios::binary);
	while ((ret = mongoc_stream_read(download_stream, buf, sizeof(buf), 0, -1)) > 0) {
		count += ret;
		// printf("got %d bytes\n", ret);
		output.write(buf, ret);
	}
	if (ret) return ret;  // error
	output.close();
	mongoc_stream_destroy(download_stream);
	return 0;
}

int MongoDat::delete_gridfs_file(const bson_oid_t& id) {
	assert(database && "database not opened");
	bson_t filter;
	bson_init(&filter);
	BSON_APPEND_OID(&filter, "_id", &id);  // query with ID
	mongoc_gridfs_file_list_t* list = mongoc_gridfs_find_with_opts(gridfs, &filter, NULL);
	mongoc_gridfs_file_t *file = mongoc_gridfs_file_list_next(list);
	bool found = !!file;
	if (found) {
		if ( ! mongoc_gridfs_file_remove(file, &error)) {
			return -1;  // failed to remove
		}
	}
	mongoc_gridfs_file_destroy(file);
	mongoc_gridfs_file_list_destroy(list);
	bson_destroy(&filter);
	return 0;
}

bool MongoDat::is_gridfs_file_existed(const bson_oid_t& id) {
	assert(database && "database not opened");
	bson_t filter;
	bson_init(&filter);
	BSON_APPEND_OID(&filter, "_id", &id);  // query with ID
	mongoc_gridfs_file_list_t* list = mongoc_gridfs_find_with_opts(gridfs, &filter, NULL);
	mongoc_gridfs_file_t *file = mongoc_gridfs_file_list_next(list);
	bool found = !!file;
	// if (found) {
	// 	const char *filename = mongoc_gridfs_file_get_filename(file);
	// 	printf("filename: %s\n", filename);
	// }
	mongoc_gridfs_file_destroy(file);
	mongoc_gridfs_file_list_destroy(list);
	bson_destroy(&filter);
	return found;
}

vector<char> MongoDat::get_binary_file(const bson_oid_t& remote_fileid) {
	assert(database && "database not opened");
	vector<char> retbuf;
	file_id.value_type = Mongo::BSON_TYPE_OID;
	file_id.value.v_oid = remote_fileid;
	mongoc_stream_t *download_stream = mongoc_gridfs_bucket_open_download_stream(bucket, &file_id, NULL);
	if (!download_stream) return retbuf;
	char buf[MONGODAT_PUTGET_SIZE];
	int ret, count = 0;
	while ((ret = mongoc_stream_read(download_stream, buf, sizeof(buf), 0, -1)) > 0) {
		count += ret;
		// printf("got %d bytes\n", ret);
		retbuf.insert(retbuf.end(), buf, buf + ret);
	}
	if (ret) return vector<char>{};  // error
	mongoc_stream_destroy(download_stream);
	return retbuf;
}

vector<char> MongoDat::get_binary_file_bias_length(const bson_oid_t& remote_fileid, int bias, int max_length) {
	assert(database && "database not opened");
	vector<char> retbuf;
	file_id.value_type = Mongo::BSON_TYPE_OID;
	file_id.value.v_oid = remote_fileid;
	mongoc_stream_t *download_stream = mongoc_gridfs_bucket_open_download_stream(bucket, &file_id, NULL);
	if (!download_stream) return retbuf;
	char buf[MONGODAT_PUTGET_SIZE];
	int ret = 0, count = 0;
	int count_bias = 0;
	while (count_bias < bias && (ret = mongoc_stream_read(download_stream, buf, min((int)sizeof(buf), bias - count_bias), 0, -1)) > 0) {
		count_bias += ret;  // first read those data out, this is not OPTIMIZED for large file stream but just for debugging
		// TODO: optimize for speed
	}
	while (count < max_length && (ret = mongoc_stream_read(download_stream, buf, min((int)sizeof(buf), max_length - count), 0, -1)) > 0) {
		count += ret;
		// printf("got %d bytes\n", ret);
		retbuf.insert(retbuf.end(), buf, buf + ret);
	}
	if (ret && count < max_length) return vector<char>{};  // error
	mongoc_stream_destroy(download_stream);
	return retbuf;
}

bool MongoDat::isPossibleOID(const char* str) {
	if (strlen(str) != 24) return false;
	for (int i=0; i<24; ++i) {
		char c = str[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
	} return true;
}

bson_oid_t MongoDat::parseOID(const char* str) {
	assert(MongoDat::isPossibleOID(str) && "invalid oid format");
	bson_oid_t id;
	for (int i=0; i<12; ++i) {
		char a = str[2*i];
		char b = str[2*i+1];
#define c2bs(x) (x>='0'&&x<='9'?(x-'0'):((x>='a'&&x<='f')?(x-'a'+10):((x>='A'&&x<='F')?(x-'A'+10):(0))))
		id.bytes[i] = (c2bs(a) << 4) | c2bs(b);
#undef c2bs
	}
	return id;
}

string MongoDat::OID2str(const bson_oid_t& id) {
	char buf[3];
	string str;
	for (size_t i=0; i<12; ++i) {
		sprintf(buf, "%02X", (unsigned char)id.bytes[i]);
		str += buf;
	}
	return str;
}

#endif
