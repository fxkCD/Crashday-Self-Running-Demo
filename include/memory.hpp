#pragma once
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <limits>

namespace flydemo::mem {

template<class T = void>
inline T* pointer32(const void* base, std::size_t offset) {
    std::uint32_t address;
    std::memcpy(&address, static_cast<const std::uint8_t*>(base) + offset, 4);
    return reinterpret_cast<T*>(static_cast<std::uintptr_t>(address));
}

inline void store_pointer32(void* base, std::size_t offset, const void* pointer) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    assert(address <= std::numeric_limits<std::uint32_t>::max());
    const auto raw = static_cast<std::uint32_t>(address);
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &raw, 4);
}

template<class T>
inline T& field(void* base, std::size_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(base) + offset);
}

template<class T>
inline const T& field(const void* base, std::size_t offset) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const std::uint8_t*>(base) + offset);
}

}
