#include <eez/core/alloc.h>
#include <cstdlib>
#include <cassert>

static bool failAllocation;
static int constructed;
namespace eez { struct Object { Object() { constructed++; } }; }
using Object = eez::Object;
namespace eez {
void *alloc(size_t bytes, uint32_t) { return failAllocation ? nullptr : std::malloc(bytes); }
void free(void *ptr) { std::free(ptr); }
}
int main() {
    failAllocation = true;
    assert(eez::ObjectAllocator<Object>::allocate(0) == nullptr);
    assert(constructed == 0);
    failAllocation = false;
    auto object = eez::ObjectAllocator<Object>::allocate(0);
    assert(object && constructed == 1);
    eez::ObjectAllocator<Object>::deallocate(object);
    eez::ObjectAllocator<Object>::deallocate(nullptr);
}
