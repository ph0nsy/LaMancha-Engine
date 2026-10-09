/*
 *  LaMancha Engine
 *  File: insertionsort.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */

#pragma once

#include "core/pch.h"
#include "core/types/dataStructures/array.h"

namespace LaMancha {
  namespace Sort {
    template <typename T, typename CompareFn, usize N>
    void insertionSort(DataStructures::ArrayLM<T, N>& _data, usize _lo, usize _hi, CompareFn _cmp)
    {
      for (usize i = _lo + 1; i <= _hi; i++)
      {
        T key = _data[i];
        usize j = i;
        while (j > _lo && _cmp(key, _data[j - 1]))
        {
          _data[j] = _data[j - 1];
          j--;
        }
        _data[j] = key;
      }
    }
  }
}