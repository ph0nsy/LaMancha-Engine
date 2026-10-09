/*
 *  LaMancha Engine
 *  File: arrayList.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */
#pragma once

#include "core/pch.h"

namespace LaMancha {
  namespace DataStructures {
    /**
     * @brief Dynamic array with SIMD-accelerated bulk and search operations.
     *
     * Backed by a heap-allocated buffer that grows by BlockSize elements
     * when capacity is exceeded. Allocator hook is stubbed, replace
     * new/delete with arena allocation when allocator lands.
     *
     * SIMD operations:
     *   - @c find() > scan for value, 16 bytes at a time
     *   - @c _bulkCopy() > copy elements, 16 bytes at a time
     *   - @c _bulkFill() > fill elements, 16 bytes at a time
     *
     * @tparam T Element type
     * @tparam BlockSize Growth increment in elements (default 32)
     * @tparam InitialCapacity Starting capacity in elements (default BlockSize)
     */
    template <typename T, usize BlockSize = 32, usize InitialCapacity = BlockSize>
    struct ArrayListLM {
      static_assert(BlockSize > 0, "BlockSize must be > 0.");
      static_assert(InitialCapacity > 0, "InitialCapacity must be > 0.");

      T* data;
      usize count;
      usize cap;

      ArrayListLM()
      {
        cap = InitialCapacity;
        data = TODO_allocate(cap);
      }

      ~ArrayListLM()
      {
        TODO_deallocate(data);
        data = nullptr;
        count = 0;
        cap = 0;
      }

      /** @brief Deep copy: allocates new buffer, copies all elements */
      ArrayListLM(const ArrayListLM& _other)
      {
        cap = _other.cap;
        data = TODO_allocate(cap);
        count = _other.count;
        _bulkCopy(data, _other.data, count);
      }

      ArrayListLM& operator=(const ArrayListLM& _other)
      {
        if (this == &_other) { return *this; }
        TODO_deallocate(data);
        cap = _other.cap;
        data = TODO_allocate(cap);
        count = _other.count;
        _bulkCopy(data, _other.data, count);
        return *this;
      }

      /** @brief Move: steals buffer, leaves source empty */
      ArrayListLM(ArrayListLM&& _other) noexcept
      {
        data = _other.data;
        count = _other.count;
        cap = _other.cap;
        _other.data = nullptr;
        _other.count = 0;
        _other.cap = 0;
      }

      ArrayListLM& operator=(ArrayListLM&& _other) noexcept
      {
        if (this == &_other) { return *this; }
        TODO_deallocate(data);
        data = _other.data;
        count = _other.count;
        cap = _other.cap;
        _other.data = nullptr;
        _other.count = 0;
        _other.cap = 0;
        return *this;
      }

      T& operator[](usize _idx)
      {
        LAMANCHA_ASSERT(_idx < count, "ArrayListLM::operator[] > Index out of bounds.");
        return data[_idx];
      }

      const T& operator[](usize _idx) const
      {
        LAMANCHA_ASSERT(_idx < count, "ArrayListLM::operator[] > Index out of bounds.");
        return data[_idx];
      }

      /** @brief Get element at index. */
      Optional<T*> get(usize _idx)
      {
        if (_idx >= count) return Optional<T*>::None();
        return Optional<T*>::Some(&data[_idx]);
      }

      /** @brief Get element at index. */
      Optional<const T*> get(usize _idx) const
      {
        if (_idx >= count) return Optional<const T*>::None();
        return Optional<const T*>::Some(&data[_idx]);
      }

      /** @brief Insert at end position. */
      ResultVoid push(const T& _val)
      {
        if (count >= cap)
        {
          ResultVoid r = _grow();
          if (!r) { return r; }
        }
        data[count++] = _val;
        return Ok();
      }

      /** @brief Remove at end position. */
      ResultVoid pop()
      {
        if (count == 0)
        {
          return ErrorVoid(ErrorCode::OutOfBounds, "Pop on empty list.");
        }
        count--;
        return Ok();
      }

      /**
       * @brief Insert at index, shifting elements right.
       * @nore Preserves order. Slower
       */
      ResultVoid insert(usize _idx, const T& _val) {
        if (_idx > count) { return ErrorVoid(ErrorCode::OutOfBounds, "Insert index out of bounds."); }
        if (count >= cap)
        {
          ResultVoid r = _grow();
          if (!r) { return r; }
        }
        // Shift right from end to _idx
        for (usize i = count; i > _idx; i--)
        {
          data[i] = data[i - 1];
        }
        data[_idx] = _val;
        count++;
        return Ok();
      }

      /**
       * @brief Remove at index.
       * @param _keepOrder swap-with-last (false) | shift-down (true)
       * @note Swap with last is O(1) and shift down is O(n)
       */
      ResultVoid remove(usize _idx, bool _keepOrder = false) {
        if (_idx >= count)
        {
          return ErrorVoid(ErrorCode::OutOfBounds, "Remove index out of bounds.");
        }
        if (_keepOrder) { return removeShift(_idx); }
        return removeSwap(_idx);
      }

      /**
       * @brief Find first index of _val, returns N (count) if not found.
       *
       * For sizeof(T) == 1: scans 16 bytes per iteration via SimdBlock16u8.
       * For other sizes:    scalar fallback.
       *
       * @note SIMD path requires T to be byte-comparable (trivially copyable,
       *       no padding variance). Safe for u8, Handle types, enum class u8.
       */
      usize find(const T& _val) const
      {
        using namespace Internal;

        // SIMD path (byte-sized T only)
        if constexpr (sizeof(T) == 1)
        {
          const u8 target = *reinterpret_cast<const u8*>(&_val);
          const u8* bytes = reinterpret_cast<const u8*>(data);

          usize i = 0;
          for (; i + 16 <= count; i += 16)
          {
            SimdBlock16u8 vec = SimdBlock16u8::load(bytes + i);
            SimdBlock16u8 tvec = broadcast(target);
            SimdBlock16u8 cmp = cmpeq(vec, tvec);
            u16 mask = extractMask(cmp);
            if (mask)
            {
              usize lane = static_cast<usize>(__builtin_ctz(mask));
              return i + lane;
            }
          }
          // Tail, remaining elements not covered by full SIMD group
          for (; i < count; i++)
          {
            if (data[i] == _val) { return i; }
          }
          return count;  // not found
        }

        // Scalar fallback for larger T
        for (usize i = 0; i < count; i++)
        {
          if (data[i] == _val) return i;
        }
        return count;
      }

      void clear() { count = 0; }

      usize size() const { return count; }
      usize capacity() const { return cap; }

      T* begin() { return data; }
      T* end() { return data + count; }
      const T* begin() const { return data; }
      const T* end() const { return data + count; }

    private:

      LAMANCHA_INLINE T* TODO_allocate(usize _n)
      {
        return new T[_n];
      }

      LAMANCHA_INLINE void TODO_deallocate(T* _ptr)
      {
        delete[] _ptr;
      }

      LAMANCHA_INLINE ResultVoid removeSwap(usize _idx) {
        data[_idx] = data[count - 1];
        count--;
        return Ok();
      }

      LAMANCHA_INLINE ResultVoid removeShift(usize _idx) {
        for (usize i = _idx; i < count - 1; i++)
          data[i] = data[i + 1];
        count--;
        return Ok();
      }

      ResultVoid grow()
      {
        usize newCap = cap + BlockSize;
        T* newData = TODO_allocate(newCap);
        if (!newData)
        {
          return ErrorVoid(ErrorCode::OutOfMemory, "ArrayList growth failed.");
        }
        bulkCopy(newData, data, count);
        TODO_deallocate(data);
        data = newData;
        cap = newCap;
        return Ok();
      }

      /**
       * @brief Copy _count elements from _src to _dst, 16 bytes at a time.
       * Tail elements copied scalar.
       */
      LAMANCHA_INLINE void bulkCopy(T* _dst, const T* _src, usize _count) {
        using namespace Internal;
        const u8* src = reinterpret_cast<const u8*>(_src);
        u8* dst = reinterpret_cast<u8*>(_dst);
        usize      bytes = _count * sizeof(T);
        usize      i = 0;

        for (; i + 16 <= bytes; i += 16) {
          SimdBlock16u8 block = SimdBlock16u8::load(src + i);
          block.store(dst + i);
        }
        for (; i < bytes; i++) dst[i] = src[i];
      }

      /**
       * @brief Fill _count elements with _val, 16 bytes at a time.
       * Only meaningful for sizeof(T) == 1. Larger T filled scalar.
       */
      LAMANCHA_INLINE void bulkFill(T* _dst, const T& _val, usize _count) {
        using namespace Internal;

        if constexpr (sizeof(T) == 1) {
          u8  byte = *reinterpret_cast<const u8*>(&_val);
          u8* dst = reinterpret_cast<u8*>(_dst);
          usize i = 0;
          SimdBlock16u8 filled = broadcast16u8(byte);
          for (; i + 16 <= _count; i += 16) filled.store(dst + i);
          for (; i < _count; i++) dst[i] = byte;
        }
        else {
          for (usize i = 0; i < _count; i++) _dst[i] = _val;
        }
      }

      //void sort(CompareFn) {}
      //void reverseSort(CompareFn) {}
    };
  }
}