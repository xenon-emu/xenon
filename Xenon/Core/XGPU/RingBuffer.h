// Copyright 2025 Xenon Emulator Project. All rights reserved.

#include "Base/Logging/Log.h"
#include <mutex>

namespace Xe::XGPU {

  // During execution applications may change the contents of the RingBuffer while the CP
  // is executing it. We create a small buffer and load the data at CP_RB_BASE with size 
  // equal to CB_RB_CNTL & 0x3F, wich tells our size (log2).

  class RingBuffer {
  public:
    RingBuffer(u8 *buffer, size_t capacity);

    u8* buffer() const { return _buffer; }
    size_t capacity() const { return _capacity; }
    bool empty() const { return _readOffset == _writeOffset; }

    size_t readOffset() const { return _readOffset; }
    uptr readPtr() const { return uptr(_buffer) + _readOffset; }
    void setReadOffset(size_t offset) { _readOffset = offset % _capacity; }
    
    // Returns remaining data in buffer that's availeable to read.
    size_t readCount() const {
      if (empty())
        return 0;
      return _readOffset < _writeOffset ?
        _writeOffset - _readOffset :
        (_capacity - _readOffset) + _writeOffset;
    }

    size_t writeOffset() const { return _writeOffset; }
    uptr writePointer() const { return uptr(_buffer) + _writeOffset; }
    void setWriteOffset(size_t offset) { _writeOffset = offset % _capacity; }
    size_t writeCount() const {
      if (empty())
        return _capacity;
      return _writeOffset < _readOffset ?
        _readOffset - _writeOffset :
        (_capacity - _writeOffset) + _readOffset;
    }

    void AdvanceRead(size_t count);
    void AdvanceWrite(size_t count);

    struct ReadRange {
      const u8 *first;
      size_t firstLength;
      const u8 *second;
      size_t secondLength;
    };

    // Reads a range of memory, useful for loading large chunks of data such as shader data, etc...
    ReadRange BeginRead(size_t count);
    void EndRead(ReadRange readRange);

    size_t Read(u8 *buffer, size_t count);
    template <typename T>
    size_t Read(T *buffer, size_t count) { return Read(reinterpret_cast<u8*>(buffer), count * sizeof(T)); }

    // Performs a read at current buffer position.
    template <typename T>
    T Read() {
      static_assert(std::is_fundamental<T>::value, "Immediate read only supports basic types!");
      std::lock_guard lock(mutex);
      T imm;
      size_t read = Read(reinterpret_cast<u8*>(&imm), sizeof(T));
      assert(read == sizeof(T));
      return imm;
    }

    // Performs a read at current buffer position and does a byteswap operation on the data.
    template <typename T>
    T ReadAndSwap() {
      static_assert(std::is_fundamental<T>::value, "Immediate read only supports basic types!");
      std::lock_guard lock(mutex);
      T imm;
      size_t read = Read(reinterpret_cast<u8*>(&imm), sizeof(T));
      assert(read == sizeof(T));
      imm = byteswap_be<T>(imm);
      return imm;
    }

    size_t Write(const u8 *buffer, size_t count);
    template <typename T>
    size_t Write(const T *buffer, size_t count) {
      std::lock_guard lock(mutex);
      return Write(reinterpret_cast<const u8*>(buffer), count);
    }

    template <typename T>
    size_t Write(T &data) {
      std::lock_guard lock(mutex);
      return Write(reinterpret_cast<const u8*>(&data), sizeof(T));
    }

  private:
    std::recursive_mutex mutex{};
    // Buffer to store our data.
    u8 *_buffer = nullptr;
    // Current buffer capacity
    size_t _capacity = 0;
    // Read and Write offsets.
    size_t _readOffset = 0;
    size_t _writeOffset = 0;
  };
}
