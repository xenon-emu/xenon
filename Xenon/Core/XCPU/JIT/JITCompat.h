#pragma once

#include <asmjit/x86.h>
#include <type_traits>
#include <utility>

namespace Xe::JITCompat {

template <typename CC>
inline asmjit::Label NewLabel(CC *cc) {
  if constexpr (requires { cc->new_label(); }) {
    return cc->new_label();
  } else if constexpr (requires { cc->newLabel(); }) {
    return cc->newLabel();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no label creation API found");
  }
}

template <typename CC>
inline asmjit::x86::Mem NewStack(CC *cc, u64 size, u64 alignment) {
  if constexpr (requires { cc->new_stack(size, alignment); }) {
    return cc->new_stack(size, alignment);
  } else if constexpr (requires { cc->newStack(size, alignment); }) {
    return cc->newStack(size, alignment);
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no label creation API found");
  }
}

template <typename CC>
inline asmjit::x86::Vec NewXmm(CC *cc) {
  if constexpr (requires { cc->new_xmm(); }) {
    return cc->new_xmm();
  } else if constexpr (requires { cc->newXmm(); }) {
    return cc->newXmm();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no XMM allocator");
  }
}

template <typename CC>
inline asmjit::x86::Vec NewYmm(CC *cc) {
  if constexpr (requires { cc->new_ymm(); }) {
    return cc->new_ymm();
  } else if constexpr (requires { cc->newYmm(); }) {
    return cc->newYmm();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no YMM allocator");
  }
}

template <typename CC>
inline asmjit::x86::Vec NewZmm(CC *cc) {
  if constexpr (requires { cc->new_zmm(); }) {
    return cc->new_zmm();
  } else if constexpr (requires { cc->newZmm(); }) {
    return cc->newZmm();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no ZMM allocator");
  }
}

template <typename CC>
inline asmjit::x86::Gp NewGP64(CC *cc) {
  if constexpr (requires { cc->new_gp64(); }) {
    return cc->new_gp64();
  } else if constexpr (requires { cc->new_gpq(); }) {
    return cc->new_gpq();
  } else if constexpr (requires { cc->newGpq(); }) {
    return cc->newGpq();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no 64-bit GP creation API found");
  }
}

template <typename CC>
inline asmjit::x86::Gp NewGP32(CC *cc) {
  if constexpr (requires { cc->new_gp32(); }) {
    return cc->new_gp32();
  } else if constexpr (requires { cc->new_gpd(); }) {
    return cc->new_gpd();
  } else if constexpr (requires { cc->newGpd(); }) {
    return cc->newGpd();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no 32-bit GP creation API found");
  }
}

template <typename CC>
inline asmjit::x86::Gp NewGP16(CC *cc) {
  if constexpr (requires { cc->new_gp16(); }) {
    return cc->new_gp16();
  } else if constexpr (requires { cc->new_gpw(); }) {
    return cc->new_gpw();
  } else if constexpr (requires { cc->newGpw(); }) {
    return cc->newGpw();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no 16-bit GP creation API found");
  }
}

template <typename CC>
inline asmjit::x86::Gp NewGP8(CC *cc) {
  if constexpr (requires { cc->new_gp8(); }) {
    return cc->new_gp8();
  } else if constexpr (requires { cc->new_gpb(); }) {
    return cc->new_gpb();
  } else if constexpr (requires { cc->newGpb(); }) {
    return cc->newGpb();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no 8-bit GP creation API found");
  }
}

template <typename CC>
inline asmjit::x86::Gp NewGP8(CC *cc, const char *label) {
  if constexpr (requires { cc->new_gp8(label); }) {
    return cc->new_gp8(label);
  } else if constexpr (requires { cc->new_gpb(label); }) {
    return cc->new_gpb(label);
  } else if constexpr (requires { cc->newGpb(label); }) {
    return cc->newGpb(label);
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no 8-bit GP creation API found");
  }
}

template <typename CC>
inline asmjit::x86::Gp NewGPZ(CC *cc) {
  if constexpr (requires { cc->new_gpz(); }) {
    return cc->new_gpz();
  } else if constexpr (requires { cc->newGpz(); }) {
    return cc->newGpz();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no GPZ API found");
  }
}

template <typename CC>
inline asmjit::x86::Gp NewGPZ(CC *cc, const char *label) {
  if constexpr (requires { cc->new_gpz(label); }) {
    return cc->new_gpz(label);
  } else if constexpr (requires { cc->newGpz(label); }) {
    return cc->newGpz(label);
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no GPZ API found");
  }
}

#if defined(XE_ASMJIT_NEW_OUT_API)
template <typename CC, typename Target, typename Signature>
inline void Invoke(CC *cc, asmjit::InvokeNode *&out, Target &&target, const Signature &signature) {
  cc->invoke(asmjit::Out(out), std::forward<Target>(target), signature);
}
#else
template <typename CC, typename Target, typename Signature>
inline void Invoke(CC *cc, asmjit::InvokeNode *&out, Target &&target, const Signature &signature) {
  cc->invoke(&out, std::forward<Target>(target), signature);
}
#endif

template <typename CC>
inline void EndFunc(CC *cc) {
  if constexpr (requires { cc->end_func(); }) {
    cc->end_func();
  } else if constexpr (requires { cc->endFunc(); }) {
    cc->endFunc();
  } else {
    static_assert(sizeof(CC) == 0, "Unsupported AsmJit: no EndFunc API found");
  }
}

template <typename Node, typename Arg>
inline void SetArg(Node *node, u32 index, Arg &&arg) {
  if constexpr (requires { node->set_arg(index, std::forward<Arg>(arg)); }) {
    node->set_arg(index, std::forward<Arg>(arg));
  } else if constexpr (requires { node->setArg(index, std::forward<Arg>(arg)); }) {
    node->setArg(index, std::forward<Arg>(arg));
  } else {
    static_assert(sizeof(Node) == 0, "Unsupported AsmJit: no InvokeNode arg setter found");
  }
}

template <typename Node, typename Ret>
inline void SetRet(Node *node, u32 index, Ret &&ret) {
  if constexpr (requires { node->set_ret(index, std::forward<Ret>(ret)); }) {
    node->set_ret(index, std::forward<Ret>(ret));
  } else if constexpr (requires { node->setRet(index, std::forward<Ret>(ret)); }) {
    node->setRet(index, std::forward<Ret>(ret));
  } else {
    static_assert(sizeof(Node) == 0, "Unsupported AsmJit: no InvokeNode return setter found");
  }
}

} // namespace Xe::JITCompat