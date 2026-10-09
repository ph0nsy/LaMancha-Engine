/*
 *  LaMancha Engine
 *  File: pair.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */

#include "core/pch.h"

namespace LaMancha {
  namespace DataStructures {
    template <typename T1, typename T2>
    struct PairLM {
      T1 first;
      T2 second;
    };

    // Deduction helper, allows makePair(a, b) without spelling out types
    template <typename T1, typename T2>
    LAMANCHA_INLINE PairLM<T1, T2> makePair(const T1& _a, const T2& _b) { return { _a, _b }; }
  }
}