/*
 *  LaMancha Engine
 *  File: array.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */
#pragma once

#include "core/pch.h"

namespace LaMancha {
  namespace DataStructures {
    template <typename T, usize N>
    struct ArrayLM {
      T data[N];

      LAMANCHA_INLINE usize size() const { return N; }

      T& operator[](usize _idx) {
        LAMANCHA_ASSERT(_idx < N, "ArrayLM > Index out of bounds.");
        return data[_idx];
      }
      const T& operator[](usize _idx) const {
        LAMANCHA_ASSERT(_idx < N, "ArrayLM > Index out of bounds.");
        return data[_idx];
      }

      T* begin() { return &data[0]; }
      T* end() { return &data[N]; }

    };
  }
}
