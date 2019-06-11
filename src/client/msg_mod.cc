#include "client/msg_mod.h"

using namespace std;
using namespace hvs;
using namespace Pistache;

void ClientRpc::start() {}

void ClientRpc::stop() {
  for (auto rpcc : rpc_clients) {
    rpcc.second->shutdown();
  }
}

std::shared_ptr<RpcClient> ClientRpc::rpc_channel(
    std::shared_ptr<IOProxyNode> node) {
  // Found, already established connection, maybe out-of-date.
  // Currently we not mantain the exists connection.
  rpc_mutex.lock_shared();
  auto rpcc = rpc_clients.find(node->uuid);
  rpc_mutex.unlock_shared();

  if (rpcc != rpc_clients.end()) {
    return rpcc->second;
  }
  // Just try create it
  auto rpcp = make_shared<RpcClient>(node->ip, node->rpc_port);
  rpc_mutex.lock();
  rpc_clients.try_emplace(node->uuid, rpcp);
  rpc_mutex.unlock();
  return rpcp;
}

std::shared_ptr<ClientSession> ClientRpc::udt_channel(
    std::shared_ptr<IOProxyNode> node) {
  // Found, already established connection, maybe out-of-date.
  // Currently we not mantain the exists connection.
  rpc_mutex.lock_shared();
  auto udtc = udt_clients.find(node->uuid);
  rpc_mutex.unlock_shared();

  if (udtc != udt_clients.end()) {
    return udtc->second;
  }
  // Just try create it
  auto udtp = udt_client.create_session(node->ip, node->data_port);
  if (!udtp.get()) return nullptr;
  rpc_mutex.lock();
  udt_clients.try_emplace(node->uuid, udtp);
  rpc_mutex.unlock();
  return udtp;
}

int ClientRpc::write_data(std::shared_ptr<IOProxyNode> node,
                          ioproxy_rpc_buffer& buf) {
  // TODO: We assume RpcClient can concurently call
  auto udtc = udt_channel(node);
  if (!udtc.get()) return -ETIMEDOUT;
  int id = udtc->write(buf);
  auto res = udtc->wait_op(id);
  return res->error_code;
}

unique_ptr<ioproxy_rpc_buffer> ClientRpc::read_data(
    std::shared_ptr<IOProxyNode> node, ioproxy_rpc_buffer& buf) {
  // TODO: We assume RpcClient can concurently call
  auto udtc = udt_channel(node);
  if (!udtc.get()) return nullptr;
  int id = udtc->write(buf);
  auto res = udtc->wait_op(id);
  return res;
}

std::shared_ptr<Pistache::Http::Client> ClientRpc::rest_channel(
    std::string endpoint) {
  // Found, already established connection, maybe out-of-date.
  // Currently we not mantain the exists connection.
  rpc_mutex.lock_shared();
  auto restc = rest_clients.find(endpoint);
  rpc_mutex.unlock_shared();

  if (restc != rest_clients.end()) {
    return restc->second;
  }
  // Just try create it
  auto restcp = make_shared<Http::Client>();
  auto opts = Http::Client::options().threads(2).maxConnectionsPerHost(8);
  restcp->init(opts);
  rpc_mutex.lock();
  rest_clients.try_emplace(endpoint, restcp);
  rpc_mutex.unlock();
  return restcp;
}

std::string ClientRpc::post_request(const std::string& endpoint,
                                    const std::string& url,
                                    const std::string& data) {
  try {
    auto restc = rest_channel(endpoint);
    string real_url = endpoint + url;
    auto response = restc->post(real_url).body(data).send();
    dout(15) << "Info: Client post request to " << url << dendl;

    auto prom_p = make_shared<promise<std::string>>();
    auto fu = prom_p->get_future();
    response.then(
        [prom_p, endpoint](Http::Response res) {
          dout(15) << "Info: Client post get response from " << endpoint
                   << dendl;
          prom_p->set_value(std::move(res.body()));
        },
        [prom_p, endpoint](exception_ptr& exptr) {
          try {
            if (exptr) {
              rethrow_exception(exptr);
            }
          } catch (const std::exception& e) {
            dout(10) << "Warnning: Client post get response rejected from "
                     << endpoint << " reason: " << e.what() << dendl;
          }
          prom_p->set_value("");
        });
    auto status = fu.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      dout(5) << "ERROR: Client connot connect to " << endpoint << dendl;
      return {};
    } else if (status == std::future_status::ready) {
      auto res = fu.get();
      return res;
    }
  } catch (const exception& e) {
    dout(5) << "ERROR: connot connect to " << endpoint << url
            << " Reason: " << e.what() << dendl;
  }
}

string ClientRpc::get_request(const string& endpoint, const string& url) {
  try {
    auto restc = rest_channel(endpoint);
    string real_url = endpoint + url;
    auto response = restc->get(real_url).send();
    dout(15) << "Info: Client get request to " << url << dendl;

    promise<std::string> prom;
    auto fu = prom.get_future();
    response.then(
        [&prom, &endpoint](Http::Response res) {
          dout(15) << "Info: Client post get response from " << endpoint
                   << dendl;
          prom.set_value(std::move(res.body()));
        },
        [&prom, &endpoint](exception_ptr& exptr) {
          try {
            if (exptr) {
              rethrow_exception(exptr);
            }
          } catch (const std::exception& e) {
            dout(10) << "Warnning: Client post get response rejected from "
                     << endpoint << " reason: " << e.what() << dendl;
          }
          prom.set_value("");
        });
    auto status = fu.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      dout(5) << "ERROR: Client connot connect to " << endpoint << dendl;
      return {};
    } else if (status == std::future_status::ready) {
      auto res = fu.get();
      return res;
    }
  } catch (const exception& e) {
    dout(5) << "ERROR: connot connect to " << endpoint << url
            << " Reason: " << e.what() << dendl;
  }
}
