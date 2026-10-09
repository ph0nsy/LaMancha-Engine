/*
 *  LaMancha Engine
 *  File: sort.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */
#pragma once

#include "introsort.h"
#include "quicksort.h"

namespace LaMancha {
  namespace Sort {
    enum SortType : u8 { QuickSort = 0, HeapSort, InsertionSort, IntroSort };

    /**
     * @brief Sort ascending using introsort.
     * @param _cmp  Comparator: _cmp(a, b) returns true if a < b
     */
    template <typename T, typename CompareFn, usize N>
    void sort(DataStructures::ArrayLM<T, N>& _data, CompareFn _cmp, SortType _sortWith = SortType::IntroSort)
    {
      if (N < 2) { return; }
      switch (_sortWith) {
      case SortType::HeapSort:
        Sort::heapsort(_data, 0, N - 1, _cmp);
        break;
      case SortType::InsertionSort:
        Sort::insertionSort(_data, 0, N - 1, _cmp);
        break;
      case SortType::IntroSort:
        Sort::introsort(_data, 0, N - 1, _cmp, Math::uLog2(N) * 2);
        break;
      default:
        Sort::quicksort(_data, _cmp);
        break;
      }
    }

    /**
     * @brief Sort descending order using introsort.
     * @param _cmp  Comparator: _cmp(a, b) returns true if a < b
     */
    template <typename T, typename CompareFn, usize N>
    void reverseSort(CompareFn _cmp)
    {
      Sort::sort([&](const T& a, const T& b) { return _cmp(b, a); });
    }
  }
}