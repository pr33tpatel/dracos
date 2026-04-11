#include <memorymanagement.h>


using namespace os;
using namespace os::common;

MemoryManager* MemoryManager::activeMemoryManager = 0;  // initally, activeMemoryManager is 0


MemoryManager::MemoryManager(size_t start, size_t size) {
  activeMemoryManager = this;  // set the activeMemoryManager to "this" MemoryManager

  if (size < sizeof(MemoryChunk)) {
    first = 0;
  } else {
    first = (MemoryChunk*)start;

    first->allocated = false;
    first->next = 0;
    first->prev = 0;
    first->size = size - sizeof(MemoryChunk);  // NOTE: sizeof(MemoryChunk) := metadata + size_t size
  }
}


MemoryManager::~MemoryManager() {}


void* MemoryManager::malloc(size_t size) {
  MemoryChunk* result = 0;

  // iterate through the memory space and find unallocated space large enough for requested size
  // NOTE: pretty much iterating through a linked list
  for (MemoryChunk* chunk = first; chunk != 0 && result == 0;
       chunk = chunk->next) {  // FIXME: O(n) complexity to find unallocated space
    if (size <= chunk->size && !chunk->allocated) result = chunk;
  }

  if (result == 0) {  // at this point, there is no available space to be allocated for requested size
    return 0;
  }

  // NOTE: available space needs to be (requested size + metadata) since requested size does not account
  // for metadata of MemoryChunk NOTE: "+ 1" acts like a buffer between requested size and the next
  // MemoryChunk*; without the "+ 1", the requested size would be directly followed by the metadata for
  // the next MemoryChunk*

  // if the result chunk is more than what we need, then we can partition it into what we need
  // the result will be partitioned into [ (metadata + requested size) | remaining result chunk ]
  if (result->size >=
      size + sizeof(MemoryChunk) + 1) {  // the result MemoryChunk size is more than the (metadata +
                                         // requested size), so we can partition
    MemoryChunk* remaining = (MemoryChunk*)((size_t)result + sizeof(MemoryChunk) +
                                            size);  // turn the  remaining unallocated space into
                                                    // MemoryChunk (metadata + size)
    remaining->allocated = false;
    remaining->size =
        result->size -
        (size +
         sizeof(MemoryChunk));  // remaining size = result_chunk size - (metadata + requested size)
    remaining->prev = result;
    remaining->next = result->next;
    if (remaining->next != 0) remaining->next->prev = remaining;


    result->size = size;
    result->next = remaining;
    // clang-format off
    /*
      DIAGRAM: Partitioning of result_chunk if result_chunk size is more than the (metadata + requested
      size)

      (time=0) | MEMORYDIMENSION := {[chunk1], [chunk2], [ ... unallocated space (64 KB) ... ], [chunk3], [chunk4], [ ... unallocated space (16 KB) ...  ] } => entire memory space

      (time=1) | malloc(size_t: 256) => requested size := 8192 bytes (size_t == uint32_t, so 32 * 32 = 8192) 
      (time=1) | let metadata be 512 bytes => (metadata + requested size) = 512 + 8192 = 8704 (total size needed) 
      (time=1) | result_chunk := first unallocated sector in memory space greater than requested size (8192 bytes), see for loop above for more details 
      (time=1) | result_chunk->size = 65536 bytes ( the result_chunk->size == unallocated space)

      (time=2) | the result_chunk->size is greater than (metadata + requested size = 8702), so partition the result_chunk (unallocated space) , at this point, the unallocated has no metadata 
      (time=2) | turn the result_chunk (unallocated space) into a MemoryChunk (result_chunk->size + metadata)
      (time=2) | result_chunk := [ (metadata + requested size) | remaining_after_partitioning |] 
      (time=2) | initalize metadata values (next, prev, allocated, size) for remaining_chunk,(e.g. remaining_chunk->size = result_chunk->size - (metadata + requested size)) 
      (time=2) | result_chunk := [ (metadata + requested size)], unallocated_space := [ remaining_after_partitioning] 
      (time=2) | now, the result_chunk->size = (metadata + requested size), so we are good to return the result_chunk, the remaining_chunk just becomes unallocated space again

      (time=3) | MEMORYDIMENSION := {[chunk1], [chunk2], [chunk5], [ ... unallocated space (55.5 KB) ... ], [chunk3], [chunk4], [ ... unallocated space (16 KB) ... ] } => entire memory space


  */
    // clang-format on
  }
  result->allocated = true;

  return (void*)(sizeof(MemoryChunk) +
                 ((size_t)result));  // return a pointer to the chunk (MemoryChunk*) that is
                                     // available to be allocated towards the requested size
}


void MemoryManager::free(void* ptr) {
  if (ptr == 0) return;

  MemoryChunk* chunk = (MemoryChunk*)((size_t)ptr - sizeof(MemoryChunk));

  chunk->allocated = false;

  if (chunk->prev != 0 && !chunk->prev->allocated) {
    chunk->prev->next = chunk->next;
    chunk->prev->size += chunk->size + sizeof(MemoryChunk);

    if (chunk->next != 0) chunk->next->prev = chunk->prev;

    chunk = chunk->prev;
  }

  if (chunk->next != 0 && !chunk->next->allocated) {
    chunk->size += chunk->next->size + sizeof(MemoryChunk);
    chunk->next = chunk->next->next;
    if (chunk->next != 0)  // if the new next chunk exists, then set its previous pointer to ourselves
      chunk->next->prev = chunk;
  }
}


/**
 * [allocate memory to DracOS heap]
 *
 * e.g.:
 * driver = new amd_am79c973(&dev, interrupts);
 */
void* operator new(unsigned size) {
  if (os::MemoryManager::activeMemoryManager == 0) return 0;
  return os::MemoryManager::activeMemoryManager->malloc(size);
}
/**
 * [allocate memory for array of objects on DracOS heap]
 *
 * e.g.:
 * int* arr = new int[[5] ];
 * uint8_t* buffer = new uint8_t[[sizeof(InternetProtocolMessage) + size] ];
 */
void* operator new[](unsigned size) {
  if (os::MemoryManager::activeMemoryManager == 0) return 0;
  return os::MemoryManager::activeMemoryManager->malloc(size);
}

void* operator new(unsigned size, void* ptr) {
  return ptr;
}

void* operator new[](unsigned size, void* ptr) {
  return ptr;
}

/**
 * [deallocate an object's memory from DracOS heap]
 */
void operator delete(void* ptr) {
  if (os::MemoryManager::activeMemoryManager != 0) {
    os::MemoryManager::activeMemoryManager->free(ptr);
  }
}
/**
 * [deallocate memory from array of objects]
 */
void operator delete[](void* ptr) {
  if (os::MemoryManager::activeMemoryManager != 0) {
    os::MemoryManager::activeMemoryManager->free(ptr);
  }
}

void operator delete(void* ptr, unsigned size) {
  if (os::MemoryManager::activeMemoryManager != 0) {
    os::MemoryManager::activeMemoryManager->free(ptr);
  }
}
void operator delete[](void* ptr, unsigned size) {
  if (os::MemoryManager::activeMemoryManager != 0) {
    os::MemoryManager::activeMemoryManager->free(ptr);
  }
}
