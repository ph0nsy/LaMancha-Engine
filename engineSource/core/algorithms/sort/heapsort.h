/*
 *  LaMancha Engine
 *  File: heapsort.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */
#pragma once

#include "sortUtilities.h"

namespace LaMancha {
  namespace Sort {
    template <typename T, typename CompareFn, usize N>
    void heapsort(DataStructures::ArrayLM<T, N>& _data, usize _lo, usize _hi, CompareFn _cmp)
    {
      usize n = _hi - _lo + 1;
      // Build max-heap
      for (isize i = static_cast<isize>(n / 2) - 1; i >= 0; i--)
      {
        Utils::siftDown(_data, _lo, _lo + static_cast<usize>(i), _hi, _cmp);
      }
      // Extract
      for (usize i = _hi; i > _lo; i--)
      {
        T t = _data[_lo];
        _data[_lo] = _data[i];
        _data[i] = t;
        Utils::siftDown(_data, _lo, _lo, i - 1, _cmp);
      }
    }
  }
};
