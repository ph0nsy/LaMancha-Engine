/**
 * @file system.h
 * @brief System definition for LaMancha Engine's ECS
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#pragma once
#include "core/pch.h"

namespace LaMancha {
  namespace ECS {
    /**
     * @brief Type-erased component array with grow/shrink/swap operations
     *
     * Each archetype has one ComponentArray per component type.
     * Supports fast swap-remove (O(1) deletion by moving last element to hole).
     */
    struct ComponentArray {
      void* data;          ///< Raw component storage (malloc'd)
      usize elementSize;   ///< sizeof(Component)
      usize capacity;      ///< Allocated slots
      usize count;         ///< Active elements

      /**
       * @brief Allocate initial capacity
       * @param _elementSize sizeof(ComponentType)
       * @param initialCap Starting capacity (default 64)
       */
      LAMANCHA_INLINE void init(usize _elementSize, usize initialCap = 64) {
        elementSize = _elementSize;
        capacity = initialCap;
        count = 0;
        data = malloc(elementSize * capacity);
        LAMANCHA_ASSERT(data != nullptr, "");
      }

      /** @brief Free memory */
      LAMANCHA_INLINE void destroy() {
        if (data) { free(data); data = nullptr; }
        count = capacity = 0;
      }

      /** @brief Grow capacity (2x expansion) */
      LAMANCHA_INLINE void grow() {
        usize newCap = capacity * 2;
        void* newData = malloc(elementSize * newCap);
        LAMANCHA_ASSERT(newData != nullptr, "");
        memcpy(newData, data, elementSize * count);
        free(data);
        data = newData;
        capacity = newCap;
      }

      /**
       * @brief Get typed pointer to array
       * @tparam T Component type
       * @return Pointer to first element
       */
      template <typename T>
      LAMANCHA_INLINE T* as() {
        LAMANCHA_ASSERT(elementSize == sizeof(T), "");
        return reinterpret_cast<T*>(data, "");
      }

      template <typename T>
      LAMANCHA_INLINE const T* as() const {
        LAMANCHA_ASSERT(elementSize == sizeof(T), "");
        return reinterpret_cast<const T*>(data);
      }

      /**
       * @brief Add element (copy)
       * @param elem Pointer to source data
       * @return Index of new element
       */
      LAMANCHA_INLINE usize push(const void* elem) {
        if (count == capacity) { grow(); }
        memcpy(static_cast<char*>(data) + count * elementSize, elem, elementSize);
        return count++;
      }

      /**
       * @brief Remove element at index (swap with last)
       * @param index Slot to remove
       *
       * Copies last element to `index`, then decrements count.
       * Invalidates iteration order but is O(1).
       */
      LAMANCHA_INLINE void swapRemove(usize index) {
        LAMANCHA_ASSERT(index < count, "");
        if (index != count - 1) {
          void* dest = static_cast<char*>(data) + index * elementSize;
          void* src = static_cast<char*>(data) + (count - 1) * elementSize;
          memcpy(dest, src, elementSize);
        }
        --count;
      }

      /**
       * @brief Get pointer to element at index
       * @param index Slot number
       * @return void* to element
       */
      LAMANCHA_INLINE void* at(usize index) {
        LAMANCHA_ASSERT(index < count, "");
        return static_cast<char*>(data) + index * elementSize;
      }

      LAMANCHA_INLINE const void* at(usize index) const {
        LAMANCHA_ASSERT(index < count, "");
        return static_cast<const char*>(data) + index * elementSize;
      }
    };
  }
}