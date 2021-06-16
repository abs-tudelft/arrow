#pragma once

#include <string>
#include <mutex>

#include <plasma/plasma.h>

#include <grpcpp/grpcpp.h>

#include "rpc.grpc.pb.h"

namespace plasma {

class RpcServiceImpl : public plasmaRPC::RemoteObjectShare::Service {
 public:
  RpcServiceImpl(PlasmaStoreInfo* plasma_store_info, std::mutex* mutex);

 private:
  grpc::Status GetObject(grpc::ServerContext* context, const plasmaRPC::ObjectID* request,
                  plasmaRPC::ObjectDetails* response) override;

  // std::unique_ptr<PlasmaStoreInfo> plasma_store_info_;
  PlasmaStoreInfo* plasma_store_info_;
  std::mutex* mutex_;
};

class RpcClient {
 public:
  RpcClient();
  RpcClient(std::shared_ptr<grpc::Channel> channel);

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  plasmaRPC::ObjectDetails GetObject(const ObjectID& object_id);

 private:
  std::unique_ptr<plasmaRPC::RemoteObjectShare::Stub> stub_;
};

void RunRpcServer(RpcServiceImpl& service, const std::string& local_address);

} // namespace plasma