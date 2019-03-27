#include "datastore/couchbase_helper.h"
#include <memory>
#include "common/debug.h"
#include "libcouchbase/couchbase++.h"
#include "libcouchbase/couchbase++/query.h"
#include <cerrno>
// #include "config.h"

void hvs::N1qlResponse::deserialize_impl() {
  std::string status_s;
  std::map<std::string, unsigned> metrics;
  get("requestID", id);
  get("errors", errors);
  get("status", status_s);
  get("metrics", metrics);
  resultCount = metrics["resultCount"];
  resultSize = metrics["resultSize"];
  errorCount = metrics["errorCount"];
  if(status_s == "fatal") {
    status = -EINVAL;
  } else if(status_s == "success") {
    status = 0;
  }
}

int hvs::CouchbaseDatastore::init() { return _connect(name); }

int hvs::CouchbaseDatastore::set(const DatastoreKey& key,
                                 DatastoreValue& value) {
  return _set(key, value);
}

hvs::DatastoreValue hvs::CouchbaseDatastore::get(const DatastoreKey& key) {
  return _get(key);
}

hvs::DatastoreValue hvs::CouchbaseDatastore::get(const DatastoreKey& key,
                                                 const std::string& subpath) {
  return _get(key, subpath);
}

int hvs::CouchbaseDatastore::remove(const DatastoreKey& key) {
  return _remove(key);
}

int hvs::CouchbaseDatastore::_connect(const std::string& bucket) {
  // format the connection string
  // TODO: options below should get from config module
  const std::string server_address = "192.168.10.235";
  const std::string username = "dev";
  const std::string passwd = "buaaica";

  std::string connstr_f = "couchbase://%s/%s";
  // calculate the size of connection string before concentrate
  size_t nsize = connstr_f.length() + server_address.length() + bucket.length();
  std::unique_ptr<char[]> connstr_p(new char[nsize]);
  snprintf(connstr_p.get(), nsize, connstr_f.c_str(), server_address.c_str(),
           bucket.c_str());
  dout(20) << "DEBUG: format couchbase connection string: " << connstr_p.get()
           << dendl;

  client = std::shared_ptr<Couchbase::Client>(
      new Couchbase::Client(connstr_p.get(), passwd, username));
  Couchbase::Status rc = client->connect();
  if (!rc.success()) {
    dout(-1) << "ERROR: couldn't connect to couchbase. " << rc.description()
             << dendl;
  }
  // assert(rc.success());
  return rc.errcode();
}

int hvs::CouchbaseDatastore::_set(const std::string& key,
                                  const std::string& doc) {
  Couchbase::StoreResponse rs = client->upsert(key, doc);
  if (!rs.status().success()) {
    dout(5) << "ERROR: Couchbase helper couldn't set kv pair " << key.c_str()
            << "-" << doc.c_str() << ", Reason: " << rs.status().description()
            << dendl;
  }
  return rs.status().errcode();
}

int hvs::CouchbaseDatastore::_set(const std::string& key,
                                  const std::string& path,
                                  const std::string& subdoc) {
  Couchbase::SubdocResponse rs = client->upsert_sub(key, path, subdoc);
  if (!rs.status().success()) {
    dout(5) << "ERROR: Couchbase helper couldn't get kv pair " << key.c_str()
            << ", Reason: " << rs.status().description() << dendl;
  }
  return rs.status().errcode();
}

std::string hvs::CouchbaseDatastore::_get(const std::string& key) {
  Couchbase::GetResponse rs = client->get(key);
  if (!rs.status().success()) {
    dout(5) << "ERROR: Couchbase helper couldn't set kv pair " << key.c_str()
            << ", Reason: " << rs.status().description() << dendl;
  }
  return rs.value().to_string();
}

std::string hvs::CouchbaseDatastore::_get(const std::string& key,
                                          const std::string& path) {
  Couchbase::SubdocResponse rs = client->get_sub(key, path);
  if (!rs.status().success()) {
    dout(5) << "ERROR: Couchbase helper couldn't get kv pair " << key.c_str()
            << ", Reason: " << rs.status().description() << dendl;
  }
  return rs.value().to_string();
}

int hvs::CouchbaseDatastore::_remove(const std::string& key) {
  Couchbase::RemoveResponse rs = client->remove(key);
  if (!rs.status().success()) {
    dout(5) << "ERROR: Couchbase helper couldn't remove kv pair " << key.c_str()
            << ", Reason: " << rs.status().description() << dendl;
  }
  return rs.status().errcode();
}

std::string hvs::CouchbaseDatastore::_n1ql(const std::string& query) {
  auto m = Couchbase::Query::execute(*client, query);
  N1qlResponse res;
  res.deserialize(m.body().to_string());
  return res.id;
}