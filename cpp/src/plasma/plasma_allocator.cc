// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <arrow/util/logging.h>

#include "plasma/malloc.h"
#include "plasma/plasma_allocator.h"
#include <sys/mman.h>
#include <fcntl.h>


namespace plasma {

extern "C" {
void* dlmemalign(size_t alignment, size_t bytes);
void dlfree(void* mem);
}

int64_t PlasmaAllocator::footprint_limit_ = 0;
int64_t PlasmaAllocator::allocated_ = 0;
void* PlasmaAllocator::base_pointer_ = nullptr;
std::multimap<size_t, uint64_t> PlasmaAllocator::available_regions_;
int64_t PlasmaAllocator::fd_ = 0;

void PlasmaAllocator::Init(int64_t fd, void* base_pointer) {
  fd_ = fd;
  base_pointer_ = base_pointer;
  ARROW_CHECK(base_pointer_);
  MmapRecord& record = mmap_records[base_pointer_];
  record.fd = fd_;
  record.size = footprint_limit_;
  available_regions_.emplace(footprint_limit_, 0);
  ARROW_LOG(INFO) << "Base ptr at address: " << base_pointer_;
  ARROW_LOG(INFO) << "Available memory: " << footprint_limit_ << "bytes";
}

void* PlasmaAllocator::Memalign(size_t alignment, size_t bytes, int* fd, int64_t* map_size, ptrdiff_t* offset) {
  if (allocated_ + static_cast<int64_t>(bytes) > footprint_limit_) {
    return nullptr;
  }
  *offset = FindRegion(bytes);
  if (*offset == -1) {
    return nullptr;
  }
  ARROW_LOG(DEBUG) << "Allocating " << bytes << " bytes of memory at " << *offset;
  *map_size = bytes;
  *fd = fd_;
  allocated_ += bytes;
  return base_pointer_;
}

int64_t PlasmaAllocator::FindRegion(size_t bytes) {
  int64_t offset = -1;
  auto it = available_regions_.lower_bound(bytes);
  if (it == available_regions_.end()) {
    return -1;
  }
  offset = it->second;
  if (it->first > bytes) {
    available_regions_.emplace(it->first - bytes, offset + bytes);
  }
  available_regions_.erase(it);
  return offset;
}

void PlasmaAllocator::Free(void* mem, size_t bytes) {
  int64_t begin, end, offset, regOffset;
  size_t size, regSize;
  begin = static_cast<uint8_t*>(mem) - static_cast<uint8_t*>(base_pointer_);
  end = begin + bytes;
  offset = begin;
  size = bytes;
  ARROW_LOG(DEBUG) << "Freeing " << bytes << " bytes of memory at " << mem << ", offset:" << offset;
  for (auto it = available_regions_.begin(); it != available_regions_.end(); it++) {
    regSize = it->first;
    regOffset = it->second;
    if (regOffset == end) {
      size += regSize;
      available_regions_.erase(it);
    } else if (regOffset + static_cast<int>(regSize) == begin) {
      offset = regOffset;
      size += regSize;
      available_regions_.erase(it);
    }
  }
  available_regions_.emplace(size, offset);
  allocated_ -= bytes;
}

void PlasmaAllocator::SetFootprintLimit(size_t bytes) {
  footprint_limit_ = static_cast<int64_t>(bytes);
}

int64_t PlasmaAllocator::GetFootprintLimit() { return footprint_limit_; }

int64_t PlasmaAllocator::Allocated() { return allocated_; }

}  // namespace plasma
