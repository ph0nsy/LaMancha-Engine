/*
 *  LaMancha Engine
 *  File: introsort.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */
#pragma once

#include "heapsort.h"
#include "insertionsort.h"

namespace LaMancha {
  namespace Sort {
    template <typename T, typename CompareFn, usize N>
    void introsort(DataStructures::ArrayLM<T, N> _data, usize _lo, usize _hi, CompareFn _cmp, usize _depthLimit)
    {
      while (_hi - _lo > 16)
      {
        if (_depthLimit == 0)
        {
          Sort::heapsort(_data, _lo, _hi, _cmp);
          return;
        }
        _depthLimit--;
        usize pivot = Utils::partition(_data, _lo, _hi, _cmp);
        Sort::introsort(pivot + 1, _hi, _cmp, _depthLimit);
        _hi = pivot;
      }
      Sort::insertionSort(_data, _lo, _hi, _cmp);
    };
  }
}
