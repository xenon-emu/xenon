// Copyright 2025 Xenon Emulator Project

#include "Core/XGPU/RingBuffer.h"

namespace Xe::XGPU {

  RingBuffer::RingBuffer(u8* buffer, size_t capacity)
    : _buffer(buffer), _capacity(capacity) {
  }

  void RingBuffer::AdvanceRead(size_t count) {
    if (_readOffset + count < _capacity) {
      _readOffset += count;
    }
    else {
      size_t left_half = _capacity - _readOffset;
      size_t right_half = count - left_half;
      _readOffset = right_half;
    }
  }

  void RingBuffer::AdvanceWrite(size_t count) {
    if (_writeOffset + count < _capacity) {
      _writeOffset += count;
    }
    else {
      size_t left_half = _capacity - _writeOffset;
      size_t right_half = count - left_half;
      _writeOffset = right_half;
    }
  }

  RingBuffer::ReadRange RingBuffer::BeginRead(size_t count) {
    count = std::min(count, _capacity);
    if (!count) {
      return { 0 };
    }
    if (_readOffset + count < _capacity) {
      return { _buffer + _readOffset, count, nullptr, 0 };
    }
    else {
      size_t left_half = _capacity - _readOffset;
      size_t right_half = count - left_half;
      return { _buffer + _readOffset, left_half, _buffer, right_half };
    }
  }

  void RingBuffer::EndRead(ReadRange read_range) {
    if (read_range.second) {
      _readOffset = read_range.second_length;
    }
    else {
      _readOffset += read_range.first_length;
    }
  }

  size_t RingBuffer::Read(u8* buffer, size_t count) {
    count = std::min(count, _capacity);
    if (!count) {
      return 0;
    }

    // Sanity check: Make sure we don't read over the write offset.
    if (_readOffset < _writeOffset) {
      assert(_readOffset + count <= _writeOffset);
    }
    else if (_readOffset + count >= _capacity) {
      size_t left_half = _capacity - _readOffset;
      assert(count - left_half <= _writeOffset);
    }

    if (_readOffset + count < _capacity) {
      std::memcpy(buffer, _buffer + _readOffset, count);
      _readOffset += count;
    }
    else {
      size_t left_half = _capacity - _readOffset;
      size_t right_half = count - left_half;
      std::memcpy(buffer, _buffer + _readOffset, left_half);
      std::memcpy(buffer + left_half, _buffer, right_half);
      _readOffset = right_half;
    }

    return count;
  }

  size_t RingBuffer::Write(const u8* buffer, size_t count) {
    count = std::min(count, _capacity);
    if (!count) {
      return 0;
    }

    // Sanity check: Make sure we don't write over the read offset.
    if (_writeOffset < _readOffset) {
      assert(_writeOffset + count <= _readOffset);
    }
    else if (_writeOffset + count >= _capacity) {
      size_t left_half = _capacity - _writeOffset;
      assert(count - left_half <= _readOffset);
    }

    if (_writeOffset + count < _capacity) {
      std::memcpy(_buffer + _writeOffset, buffer, count);
      _writeOffset += count;
    }
    else {
      size_t left_half = _capacity - _writeOffset;
      size_t right_half = count - left_half;
      std::memcpy(_buffer + _writeOffset, buffer, left_half);
      std::memcpy(_buffer, buffer + left_half, right_half);
      _writeOffset = right_half;
    }

    return count;
  }

}