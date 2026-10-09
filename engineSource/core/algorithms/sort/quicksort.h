/*
 *  LaMancha Engine
 *  File: quicksort.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */
#pragma once
#include "sortUtilities.h"

namespace LaMancha {
  namespace Sort {
    /**
     * @brief Quicksort Function for Hoare's Scheme.
     * For base case, Hoare's partition requires checking if indices cross or meet.
     *
     * Asumes assuming [0,N) range but can be modified.
     *
     * @note Careful with underflow if _lo were 0 and we did _lo < _hi - 1
     */
    template <typename T, typename CompareFn, usize N>
    void quicksort(DataStructures::ArrayLM<T, N>& _data, CompareFn _cmp, usize _lo = 0, usize _hi = N - 1)
    {
      if (_lo < _hi)
      {
        // p is the split point returned by your partition function
        usize p = Utils::partition(_data, _lo, _hi, _cmp);

        // Note the bounds! Unlike Lomuto, p is INCLUDED in the left partition.
        Sort::quicksort(_data, _lo, p, _cmp);
        Sort::quicksort(_data, p + 1, _hi, _cmp);
      }
    }
  }
};
