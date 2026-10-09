/*
 *  LaMancha Engine
 *  File: utilities.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */
#pragma once

#include "core/pch.h"
#include "core/types/dataStructures/array.h"

namespace LaMancha {
  namespace Sort {
    namespace Utils {
      /** @brief Hoare's partitioning scheme combined with a Median-of-Three pivot selection */
      template <typename T, typename CompareFn, usize N>
      usize partition(DataStructures::ArrayLM<T, N>& _data, usize _lo, usize _hi, CompareFn _cmp)
      {
        // Median-of-three pivot selection
        usize mid = _lo + (_hi - _lo) / 2;
        if (_cmp(_data[_hi], _data[_lo])) { T t = _data[_lo];  _data[_lo] = _data[_hi];  _data[_hi] = t; }
        if (_cmp(_data[mid], _data[_lo])) { T t = _data[_lo];  _data[_lo] = _data[mid];  _data[mid] = t; }
        if (_cmp(_data[_hi], _data[mid])) { T t = _data[_hi];  _data[_hi] = _data[mid];  _data[mid] = t; }

        T pivot = _data[mid];

        // Safe Unsigned Hoare Partitioning Indices
        usize i = _lo, j = _hi;

        while (true)
        {
          while (_cmp(_data[i], pivot)) { i++; }
          while (_cmp(pivot, _data[j])) { j--; }

          if (i >= j) { return j; }

          T t = _data[i];
          _data[i] = _data[j];
          _data[j] = t;

          i++;
          j--;
        }
      }

      template <typename T, typename CompareFn, usize N>
      void siftDown(DataStructures::ArrayLM<T, N>& _data, usize _base, usize _root, usize _end, CompareFn _cmp) {
        while (true)
        {
          usize child = _base + 2 * (_root - _base) + 1;
          if (child > _end) { break; }
          if (child + 1 <= _end && _cmp(_data[child], _data[child + 1])) { child++; }
          if (!_cmp(_data[_root], _data[child])) { break; }
          T t = _data[_root];
          _data[_root] = _data[child];
          _data[child] = t;
          _root = child;
        }
      }
    }
  }
};
