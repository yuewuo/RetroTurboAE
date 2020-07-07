#include "stdio.h"
#include <algorithm>
#include "assert.h"
#include "stdlib.h"
#include <vector>
#include <fstream>
namespace Mongo {
	#include "mongoc.h"
}
using Mongo::mongoc_database_t;
using Mongo::mongoc_client_t;
using Mongo::mongoc_uri_t;
using Mongo::bson_error_t;
using Mongo::mongoc_stream_t;
using Mongo::bson_value_t;
using Mongo::mongoc_gridfs_bucket_t;
using Mongo::bson_t;
using Mongo::mongoc_client_new_from_uri;
using Mongo::mongoc_uri_new_with_error;
using Mongo::mongoc_client_set_error_api;
using Mongo::mongoc_client_get_database;
using Mongo::mongoc_stream_file_new_for_path;
using Mongo::mongoc_gridfs_bucket_new;
using Mongo::bson_new;

int main() {
	mongoc_database_t *database = NULL;
	mongoc_client_t *client = NULL;
	// mongoc_collection_t *collection = NULL;
	mongoc_uri_t *uri = NULL;
	bson_error_t error;
	// const char *host_and_port = "mongodb://user:pass@localhost:27017";
	const char *host_and_port = "mongodb://localhost:27017";
	Mongo::mongoc_init();
	uri = mongoc_uri_new_with_error(host_and_port, &error);
	if (!uri) {
		printf("error message: %s\n", error.message);
		assert(0 && "uri failed");
	}
	client = mongoc_client_new_from_uri(uri);
	assert(client && "client failed");
	mongoc_client_set_error_api(client, 2);
	database = mongoc_client_get_database(client, "test");
	// collection = mongoc_database_get_collection(database, "test");
	// init OK, then test insert single document
	// mongoc_bulk_operation_t *bulk;
	// bulk = mongoc_collection_create_bulk_operation_with_opts(collection, NULL);
	// bson_t *doc;
	// doc = BCON_NEW("x", BCON_DOUBLE(1.0), "tags", "[", "dog", "cat", "]");
	// mongoc_bulk_operation_insert(bulk, doc);
	// bson_destroy(doc);
	// bool ret = mongoc_bulk_operation_execute(bulk, NULL, &error);
	// if (!ret) {
	// 	printf("error message: %s\n", error.message);
	// 	assert(0 && "insert failed");
	// }
	// mongoc_bulk_operation_destroy(bulk);
	// then test write gridfs
	mongoc_stream_t *upload_stream;
	mongoc_gridfs_bucket_t *bucket;
	bson_t *opts;
	opts = bson_new();
	// BSON_APPEND_INT32(opts, "chunkSizeBytes", 10);
	bucket = mongoc_gridfs_bucket_new(database, opts, NULL, &error);
	if (!bucket) {
		printf("error message: %s\n", error.message);
		assert(0 && "bucket failed");
	}
	bson_value_t file_id;
	upload_stream = mongoc_gridfs_bucket_open_upload_stream(bucket, "test2.txt", NULL, &file_id, &error);
	assert(upload_stream);
	mongoc_stream_write(upload_stream, (void*)"hello world", 11, 0);
	mongoc_stream_destroy(upload_stream);
	mongoc_stream_t *download_stream;
	download_stream = mongoc_gridfs_bucket_open_download_stream(bucket, &file_id, NULL);
	assert(download_stream);
	char buf[100];
	mongoc_stream_read(download_stream, buf, 11, 1, 0);
	buf[11] = '\0';
	printf("buf[0~10]: %s\n", buf);
	// close
	// if (collection) mongoc_collection_destroy(collection);
	if (database) mongoc_database_destroy(database);
	if (client) mongoc_client_destroy(client);
	if (uri) mongoc_uri_destroy(uri);
	Mongo::mongoc_cleanup ();
	printf("program done\n");
}
