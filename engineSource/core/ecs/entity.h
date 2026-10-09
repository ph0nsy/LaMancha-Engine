/**
 * @file entity.h
 * @brief Basic entity-related definitions for LaMancha Engine's ECS
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#pragma once
#include "core/pch.h"

namespace LaMancha {
  namespace ECS {
    /**
     * @brief Type-safe entity identifier
     *
     * Combines index with generation counter for safety.
     * When an entity is destroyed, its generation increments - old references
     * become invalid automatically.
     *
     * Layout (32-bit):
     *   Bits [32 - 20] = Generation (12 bits) - increments on reuse
     *   Bits [19 - 0]  = Index (20 bits) - slot in entity metadata array
     */
    struct EntityID {
      u32 value;  ///< Packed (generation << 32) | index

      static constexpr u32 INDEX_MASK = (1u << 20) - 1;

      /** @brief Extract entity index (0 to 2^32-1) */
      LAMANCHA_INLINE u32 index() const { return value & INDEX_MASK; }

      /** @brief Extract generation counter */
      LAMANCHA_INLINE u16 generation() const { return value >> 20; }

      /** @brief Check if this is the null entity */
      LAMANCHA_INLINE bool isNull() const { return value == 0; }

      /** @brief Equality (both index and generation must match) */
      LAMANCHA_INLINE bool operator==(EntityID other) const { return value == other.value; }
      LAMANCHA_INLINE bool operator!=(EntityID other) const { return value != other.value; }

      /** @brief Create from index and generation */
      static LAMANCHA_INLINE EntityID create(u32 index, u16 generation) {
        return EntityID{ (generation << 20) | (index & INDEX_MASK) };
      }
    };
  }
}