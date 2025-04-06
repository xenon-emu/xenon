// Copyright 2025 Xenon Emulator Project

#pragma once

namespace Base {

// Check if type satisfies the ContiguousContainer named requirement.
template <typename T>
concept IsContiguousContainer = std::contiguous_iterator<typename T::iterator>;

} // namespace Base
