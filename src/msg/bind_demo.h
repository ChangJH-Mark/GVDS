#pragma once
#include "msg/rpc.h"
#include "msg/stat_demo.h"

namespace gvds {
  inline void gvds_rpc_bind(RpcServer* rpc_server) {
    rpc_server->bind("1", [] { return 1; });
    rpc_server->bind("sleep_test", sleep);
    rpc_server->bind("stat_test", get_stat);
    HvsContext::get_context()->node->rpc_bind(rpc_server);
  }
}