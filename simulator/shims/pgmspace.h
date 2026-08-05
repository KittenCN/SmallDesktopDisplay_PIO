#ifndef SDD_SIM_PGMSPACE_SHIM_H
#define SDD_SIM_PGMSPACE_SHIM_H

#include <cstdint>
#include <cstring>

#define PROGMEM

template <typename T>
inline T pgm_read_ptr(const T* address) noexcept {
  return *address;
}

template <typename T>
inline std::uint8_t pgm_read_byte(const T* address) noexcept {
  return static_cast<std::uint8_t>(*address);
}

template <typename T>
inline std::uint32_t pgm_read_dword(const T* address) noexcept {
  return static_cast<std::uint32_t>(*address);
}

inline void* memcpy_P(void* destination, const void* source,
                      std::size_t count) noexcept {
  return std::memcpy(destination, source, count);
}

#endif
