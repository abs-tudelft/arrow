#include <plasma/rpc/rpc.h>

#include "arrow/util/logging.h"

namespace plasma {

RpcServiceImpl::RpcServiceImpl(PlasmaStoreInfo* plasma_store_info, std::mutex* mutex)
    : plasma_store_info_(plasma_store_info),
      mutex_(mutex) {}

grpc::Status RpcServiceImpl::GetObjects(grpc::ServerContext* context, const plasmaRPC::ObjectIDs* request,
                plasmaRPC::ObjectDetailsList* reply) {
  ARROW_LOG(DEBUG) << "RPC: servicing request for " << request->ids_size() << " remote objects";
  for (auto id : request->ids()) {
    std::lock_guard<std::mutex> lock(*mutex_);
    const ObjectTableEntry* entry = GetObjectTableEntry(plasma_store_info_, 
                                  ObjectID::from_binary(id));

    auto object_details = reply->add_objects_details();
    if (!entry) {
      object_details->set_status(plasmaRPC::ObjectDetails::MISSING);
      continue;
    }

    switch (entry->state) {
      case(ObjectState::PLASMA_EVICTED):
        object_details->set_status(plasmaRPC::ObjectDetails::EVICTED);
        break;
      case(ObjectState::PLASMA_CREATED):
        object_details->set_status(plasmaRPC::ObjectDetails::UNSEALED);
        break;
      case(ObjectState::PLASMA_SEALED): {
        object_details->set_status(plasmaRPC::ObjectDetails::OK);
        auto object = object_details->mutable_object();
        object->set_data_offset(entry->offset);
        object->set_metadata_offset(entry->offset + entry->data_size);
        object->set_data_size(entry->data_size);
        object->set_metadata_size(entry->metadata_size);
        object->set_device_num(entry->device_num);
        break;
      }
      default:
        ARROW_LOG(ERROR) << "RPC: Invalid object state";
        break;
    }
  }
  return grpc::Status::OK;
}

RpcClient::RpcClient() {}

RpcClient::RpcClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(plasmaRPC::RemoteObjectShare::NewStub(channel)) {}

// Assembles the client's payload, sends it and presents the response back
// from the server.
plasmaRPC::ObjectDetailsList RpcClient::GetObjects(std::vector<ObjectID> object_ids) {
  // Data we are sending to the server.
  plasmaRPC::ObjectIDs request;
  for (auto id : object_ids) {
   request.add_ids(id.binary());
  }

  // Container for the data we expect from the server.
  plasmaRPC::ObjectDetailsList reply;

  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  grpc::ClientContext context;

  // The actual RPC.
  grpc::Status status = stub_->GetObjects(&context, request, &reply);

  // Act upon its status.
  if (!status.ok()) {
    ARROW_LOG(ERROR) << "RPC error: " << status.error_code() << " - " << status.error_message();
  }
  return reply;
}

void RunRpcServer(RpcServiceImpl& service, const std::string& local_address) {
  grpc::EnableDefaultHealthCheckService(true);
  // grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(local_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  auto grpc_server = builder.BuildAndStart();
  ARROW_LOG(INFO) << "RPC: server listening on " << local_address;

  grpc_server->Wait();
}

} // namespace plasma