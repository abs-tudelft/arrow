#pragma once

#include "plasma/common.h"
#include "plasma/plasma.h"


namespace plasma {

class RemoteObjectHandler {
 public:
  RemoteObjectHandler(void* local_ptr, void* remote_ptr, int reserved_volume);

  ~RemoteObjectHandler();

  const PlasmaObject* GetRemoteObject(ObjectID object_id);

  void RegisterLocalObject(ObjectID object_id, PlasmaObject plasma_object);

  void RemoveLocalObject(ObjectID object_id);

 private:
  void RegisterNewRemoteObjects();

  struct RemoteObjectDetails {
    ObjectID object_id;
    PlasmaObject plasma_object;
  };

  int reserved_volume_;
  uint8_t* local_flags_ptr_;
  uint8_t* remote_flags_ptr_;
  uint16_t* local_size_ptr_;
  uint16_t* remote_size_ptr_;
  RemoteObjectDetails* local_objects_ptr_;
  RemoteObjectDetails* remote_objects_ptr_;
  std::unordered_map<ObjectID, PlasmaObject> remote_objects_;
}

}  // namespace plasma