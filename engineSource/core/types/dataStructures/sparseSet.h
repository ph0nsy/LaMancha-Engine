/*
 *  LaMancha Engine
 *  File: sparseSet.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */

#include "core/pch.h"

namespace LaMancha {
  namespace DataStructure {
    // focus on components for ecs pool
    template <typename T, typename ID, usize N>
    struct SparseSetLM {
      static_assert(N <= U32_MAX, "SparseSet capacity exceeds max handle ID range.");

      ArrayLM<T, N> dense;       ///< contiguous values
      ArrayLM<ID, N> sparse;     ///< sparse[id] = index into dense[]. Expects ID to be Handle or EntityID
      ArrayLM<ID, N> denseToId;  ///< denseToId[denseIdx] = id (for removal). Expects ID to be Handle or EntityID
      usize count = 0;           ///< current number of elements

      static constexpr ID INVALID = ID{ 0 };

      // Initialize sparse to invalid on construction
      SparseSet() { for (usize i = 0; i < N; i++) { sparse[i] = INVALID; } }

      ResultVoid add(ID _id, const T& _val) {
        if (_id >= N || count >= N) { return ErrorVoid(ErrorCode::OutOfBounds, "Provided _id was Out of Bounds."); }
        if (sparse[_id] != INVALID) { return ErrorVoid(ErrorCode::Unknown, "Element _id already in use."); }
        sparse[_id] = count;
        dense[count] = _val;
        denseToId[count] = _id;
        count++;
        return Ok();
      }

      ResultVoid remove(ID _id) {
        if (_id >= N) { return ErrorVoid(ErrorCode::Unknown, "Provided _id was Out of Bounds."); }
        if (sparse[_id] == INVALID) { return ErrorVoid(ErrorCode::Unknown, "Element _id already in use."); }
        usize idx = sparse[_id];
        usize lastId = denseToId[count - 1];
        // Swap with last dense element to keep dense[] packed
        dense[idx] = dense[count - 1];
        denseToId[idx] = lastId;
        sparse[lastId] = idx;
        sparse[_id] = INVALID;
        count--;
        return Ok();
      }

      bool has(ID _id) const { return _id < N && sparse[_id] != INVALID; }

      Optional<T*> get(ID _id) 
      {
        if (!has(_id)) { return Optional<T*>::None(); }
        return Optional<T*>::Some(&dense[sparse[_id]]);
      }

      // Iterate dense[] directly - always contiguous, no gaps
      T* begin() { return &dense[0]; }
      T* end() { return &dense[count]; }
      usize size() const { return count; }
    };
  }
}