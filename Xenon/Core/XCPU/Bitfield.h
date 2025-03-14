#pragma once
/*
* Copyright 2025 Xenon Emulator Project

* All original authors of the rpcs3 PPU_Decoder and PPU_Opcodes maintain their original copyright.
* Modifed for usage in the Xenon Emulator
* All rights reserved
* License: GPL2
*/

#include "Base/Types.h"

template <typename T, u64 N>
class _bitfield {
public:
  // Types
	using bT = T;
  using vT = std::common_type_t<T>;
  using uT = std::make_unsigned_t<vT>;
  static constexpr bool canBePacked = N < (sizeof(s64) * 8 + (std::is_unsigned_v<vT> ? 1 : 0)) && sizeof(vT) > sizeof(s64);
  using cT = std::conditional_t<canBePacked, std::conditional_t<std::is_unsigned_v<vT>, u64, s64>, vT>;
  // Bitsize
  static constexpr u64 bitmax = sizeof(T) * 8;
	static_assert(N - 1 < bitmax, "_bitfield<> error: N out of bounds");
  // Bitfield size
  static constexpr u64 bitsize = N;
  // All ones mask
  static constexpr uT mask1 = static_cast<uT>(~static_cast<uT>(0));
  // Value mask
  static constexpr uT vmask = mask1 >> (bitmax - bitsize);
protected:
  T data;
};
// Bitfield accessor (N bits from I position, 0 is LSB)
template <typename T, u64 I, u64 N>
class bitfield : public _bitfield<T, N> {
public:
	// Types
	using bT = typename bitfield::bT;
	using vT = typename bitfield::vT;
	using uT = typename bitfield::uT;
	using cT = typename bitfield::cT;
	// Field offset
	static constexpr u64 bitpos = I;
	static_assert((bitpos + N) <= bitfield::bitmax, "bitfield<> error: I out of bounds");
	// Get bitmask of size N, at I pos
	static constexpr uT data_mask() {
		return static_cast<uT>(static_cast<uT>(bitfield::mask1 >> (bitfield::bitmax - bitfield::bitsize)) << bitpos);
	}
	// Bitfield extraction
	static constexpr cT extract(const T& data) noexcept {
		if constexpr (std::is_signed_v<T>) {
			// Load signed value (sign-extended)
			return static_cast<cT>(static_cast<vT>(static_cast<uT>(data) << (bitfield::bitmax - bitpos - N)) >> (bitfield::bitmax - N));
		}
		else {
			// Load unsigned value
			return static_cast<cT>((static_cast<uT>(data) >> bitpos) & bitfield::vmask);
		}
	}
	// Bitfield insertion
	static constexpr vT insert(cT value) {
		return static_cast<vT>((value & bitfield::vmask) << bitpos);
	}
	// Load bitfield value
	constexpr cT value() const noexcept {
		return extract(this->data);
	}
	// Load bitfield value
	constexpr operator cT() const noexcept {
		return value();
	}
	// Load raw data with mask applied
	constexpr T unshifted() const {
		return static_cast<T>(this->data & data_mask());
	}
	// Optimized bool conversion (must be removed if inappropriate)
	explicit constexpr operator bool() const noexcept {
		return unshifted() != 0u;
	}
	// Store bitfield value
	bitfield& operator =(cT value) noexcept {
		this->data = static_cast<vT>((this->data & ~data_mask()) | insert(value));
		return *this;
	}
	cT operator ++(s32) {
		cT result = *this;
		*this = static_cast<cT>(result + 1u);
		return result;
	}
	bitfield& operator ++() {
		return *this = static_cast<cT>(*this + 1u);
	}
	cT operator --(s32) {
		cT result = *this;
		*this = static_cast<cT>(result - 1u);
		return result;
	}
	bitfield& operator --() {
		return *this = static_cast<cT>(*this - 1u);
	}
	bitfield& operator +=(cT right) {
		return *this = static_cast<cT>(*this + right);
	}
	bitfield& operator -=(cT right) {
		return *this = static_cast<cT>(*this - right);
	}
	bitfield& operator *=(cT right)	{
		return *this = static_cast<cT>(*this * right);
	}
	bitfield& operator &=(cT right)	{
		this->data &= static_cast<vT>(((static_cast<uT>(right + 0u) & bitfield::vmask) << bitpos) | ~(bitfield::vmask << bitpos));
		return *this;
	}
	bitfield& operator |=(cT right) {
		this->data |= static_cast<vT>((static_cast<uT>(right + 0u) & bitfield::vmask) << bitpos);
		return *this;
	}
	bitfield& operator ^=(cT right) 	{
		this->data ^= static_cast<vT>((static_cast<uT>(right + 0u) & bitfield::vmask) << bitpos);
		return *this;
	}
};
template <typename T, u64 I, u64 N>
class std::common_type<bitfield<T, I, N>, bitfield<T, I, N>> : public std::common_type<T>
{};
template <typename T, u64 I, u64 N, typename T2>
class std::common_type<bitfield<T, I, N>, T2> : public std::common_type<T2, std::common_type_t<T>>
{};
template <typename T, u64 I, u64 N, typename T2>
class std::common_type<T2, bitfield<T, I, N>> : public std::common_type<std::common_type_t<T>, T2>
{};

// Field pack (concatenated from left to right)
template <typename F = void, typename... Fields>
class ControlField : public _bitfield<typename F::bT, F::bitsize + ControlField<Fields...>::bitsize> {
public:
	// Types
	using bT = typename ControlField::bT;
	using vT = typename ControlField::vT;
	using uT = typename ControlField::uT;
	using cT = typename ControlField::cT;
	// Get disjunction of all "data" masks of concatenated values
	static constexpr vT data_mask() {
		return static_cast<vT>(F::data_mask() | ControlField<Fields...>::data_mask());
	}
	// Extract all bitfields and concatenate
	static constexpr cT extract(const bT& data) {
		return static_cast<cT>(static_cast<uT>(F::extract(data)) << ControlField<Fields...>::bitsize | ControlField<Fields...>::extract(data));
	}
	// Split bitfields and insert them
	static constexpr vT insert(cT value) 	{
		return static_cast<vT>(F::insert(value >> ControlField<Fields...>::bitsize) | ControlField<Fields...>::insert(value));
	}
	constexpr operator cT() const noexcept 	{
		return extract(this->data);
	}
	ControlField& operator =(cT value) noexcept 	{
		this->data = (this->data & ~data_mask()) | insert(value);
		return *this;
	}
};

// Empty field pack (recursion terminator)
template <>
class ControlField<void> {
public:
	static constexpr u64 bitsize = 0;
	static constexpr u64 data_mask() {
		return 0;
	}
	template <typename T>
	static constexpr auto extract(const T&) -> decltype(+T()) {
		return 0;
	}
	template <typename T>
	static constexpr T insert(T /*value*/) {
		return 0;
	}
};

// Fixed field (provides constant values in field pack)
template <typename T, T V, u64 N>
class FixedField : public _bitfield<T, N> {
public:
	using bT = typename FixedField::bT;
	using vT = typename FixedField::vT;
	// Return constant value
	static constexpr vT extract(const T&) {
		static_assert((V & FixedField::vmask) == V, "FixedField<> error: V out of bounds");
		return V;
	}
	// Get value
	constexpr operator vT() const noexcept {
		return V;
	}
};
