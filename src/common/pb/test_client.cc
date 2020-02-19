/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "op.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using gvds::OpRequest;
using gvds::OpReply;
using gvds::Operator;
using gvds::OpType;

class OperatorClient {
 public:
  OperatorClient(std::shared_ptr<Channel> channel)
      : stub_(Operator::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  uint64_t Submit(const std::string& user) {
    // Data we are sending to the server.
    OpRequest request;
    request.set_id(2);
    request.set_type(OpType::write);
    request.set_filepath("8491a8df-0cea-41e4-94fe-f59de8a71152/c");
    request.mutable_io_param()->set_offset(10);
    request.mutable_io_param()->set_size(10);
    request.set_data("1234567890");
    // request.set_mode(0777);
    // request.set_newpath("8491a8df-0cea-41e4-94fe-f59de8a71152/d");

    // Container for the data we expect from the server.
    OpReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->Submit(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      // for (auto i : reply.entry_names())
      //   std::cout << "error: " << i << std::endl;
      // for (auto i : reply.entries())
      //   std::cout << "fs: " << i.size() << std::endl;
      return reply.id();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return 0;
    }
  }

 private:
  std::unique_ptr<Operator::Stub> stub_;
};

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint specified by
  // the argument "--target=" which is the only expected argument.
  // We indicate that the channel isn't authenticated (use of
  // InsecureChannelCredentials()).
  std::string target_str;
  std::string arg_str("--target");
  if (argc > 1) {
    std::string arg_val = argv[1];
    size_t start_pos = arg_val.find(arg_str);
    if (start_pos != std::string::npos) {
      start_pos += arg_str.size();
      if (arg_val[start_pos] == '=') {
        target_str = arg_val.substr(start_pos + 1);
      } else {
        std::cout << "The only correct argument syntax is --target=" << std::endl;
        return 0;
      }
    } else {
      std::cout << "The only acceptable argument is --target=" << std::endl;
      return 0;
    }
  } else {
    target_str = "localhost:7092";
  }
  OperatorClient greeter(grpc::CreateChannel(
      target_str, grpc::InsecureChannelCredentials()));
  std::string user("world");
  uint64_t reply = greeter.Submit(user);
  std::cout << "Greeter received: " << reply << std::endl;

  return 0;
}
