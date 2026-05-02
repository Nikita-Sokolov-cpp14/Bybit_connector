#pragma once

#include <atomic>

static const size_t hashLineSize = 64;

template<typename Data>
struct BaseLockfree {
    Data data[2];

    BaseLockfree();
};

template<typename Data>
BaseLockfree<Data>::BaseLockfree() {
    static_assert(alignof(Data) == hashLineSize);
}
