#include <plasma/remote_object_handler.h>

#include <arrow/util/logging.h>


#define FLAG_RESET		0b000
#define FLAG_ADDED		0b001
#define FLAG_REMOVED	0b010
#define FLAG_READING	0b100

#define COMPILER_REORDER_BARRIER() asm volatile("" ::: "memory")

namespace plasma {

RemoteObjectHandler::RemoteObjectHandler(void* local_ptr, void* remote_ptr, int reserved_volume)
	: reserved_volume_(reserved_volume) {
  local_flags_ptr_ = static_cast<uint8_t*>(local_ptr);
  local_size_ptr_ = static_cast<uint16_t*>(local_flags_ptr_ + 1);
  local_objects_ptr_ = static_cast<RemoteObjectDetails*>(local_size_ptr_ + 1);

  remote_flags_ptr_ = static_cast<uint8_t*>(remote_ptr);
  remote_size_ptr_ = static_cast<uint16_t*>(remote_flags_ptr_ + 1);
  remote_objects_ptr_ = static_cast<RemoteObjectDetails*>(remote_size_ptr_ + 1);
  
  remote_objects_ = std::unordered_map<ObjectID, PlasmaObject>();
}

const PlasmaObject* RemoteObjectHandler::GetRemoteObject(ObjectID object_id) {
  uint8_t flags = *remote_flags_ptr_;
  if (flags) {
    *remote_flags_ptr_ = FLAG_READING;
    COMPILER_REORDER_BARRIER();
    if (flags & FLAG_REMOVED) {
      remote_objects_.clear();
    }
    RegisterNewRemoteObjects();
    COMPILER_REORDER_BARRIER();
    *remote_flags_ptr_ = FLAG_RESET;
  }
  auto it = remote_objects_.find(object_id);
  if (!it) {
    return nullptr;
  }
  return *it->second;
}

void RemoteObjectHandler::RegisterNewRemoteObjects() {
  uint16_t n = *remote_size_ptr_;
  ARROW_CHECK(n > remote_objects_.size());
  for (int i = remote_objects_.size(); i < n; i++) {
    RemoteObjectDetails* object_details = remote_objects_ptr_ + i * sizeof(RemoteObjectDetails);
    remote_objects_.emplace(object_details->object_id, object_details->plasma_object);
  }
}

void RemoteObjectHandler::RegisterLocalObject(ObjectID object_id, PlasmaObject plasma_object) {
  RemoteObjectDetails object_details(object_id, plasma_object);

}

void RemoteObjectHandler::RemoveLocalObject(ObjectID object_id) {

}


}  // namespace plasma