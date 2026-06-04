#pragma once
#include <atomic>
#include <cstddef>

template <typename T, size_t Size = 2048>
class LockFreeQueue {
public:
    LockFreeQueue() : writePtr(0), readPtr(0) {}

    bool enqueue(const T& item) {
        size_t currWrite = writePtr.load(std::memory_order_relaxed);
        size_t nextWrite = (currWrite + 1) % Size;
        if (nextWrite == readPtr.load(std::memory_order_acquire)) return false;

        buffer[currWrite] = item;
        writePtr.store(nextWrite, std::memory_order_release);
        return true;
    }

    bool dequeue(T& item) {
        size_t currRead = readPtr.load(std::memory_order_relaxed);
        if (currRead == writePtr.load(std::memory_order_acquire)) return false;

        item = buffer[currRead];
        readPtr.store((currRead + 1) % Size, std::memory_order_release);
        return true;
    }

    void clear() {
        writePtr.store(0, std::memory_order_relaxed);
        readPtr.store(0, std::memory_order_relaxed);
    }

private:
    T buffer[Size];
    alignas(64) std::atomic<size_t> writePtr;
    alignas(64) std::atomic<size_t> readPtr;
};