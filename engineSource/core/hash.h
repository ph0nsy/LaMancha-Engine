/**
 * @file hash.h
 * @brief Hashing functions for hash tables and hash sets
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 */
#pragma once

#include "core/pch.h"
#include "core/math/LaManchaMath.h"

namespace LaMancha
{
  namespace Utils
  {

    /**
    * @brief Byte wise FNV-1a (Fowler-Noll-Vo) over sizeof(T) bytes
    * @tparam T Type of whatever to hash
    * @param _val The variable to hash itself
    * @note Structs with padding may hash inconsistently if padding bytes are uninitialized
    */
    template <typename T>
    LAMANCHA_INLINE usize hash(const T& _val) 
    {
      const u8* bytes = reinterpret_cast<const u8*>(&_val);
      usize hsh = 14695981039346656037ULL; // FNV offset basis
      for (usize i = 0; i < sizeof(T); i++) {
        hsh ^= bytes[i];
        hsh *= 1099511628211ULL; // FNV prime
      }
      return hsh;
    }

    template <typename T>
    LAMANCHA_INLINE usize hash(const T& _val, const usize _len)
    {
      const u8* bytes = reinterpret_cast<const u8*>(&_val);
      usize hsh = 14695981039346656037ULL; // FNV offset basis
      for (usize i = 0; i < _len; i++) {
        hsh ^= bytes[i];
        hsh *= 1099511628211ULL; // FNV prime
      }
      return hsh;
    }

    /**
    * @brief Fast specialization for 32 bits-sized types (handles, IDs)
    * @param _val The variable to hash itself
    * @note For Handle / ID types padding is never an issue since they are plain integers.
    * @see https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
    */
    template<>
    LAMANCHA_INLINE usize hash(const u32& _val) 
    {
      usize hsh = _val;
      hsh ^= hsh >> 16;
      hsh *= 0x45d9f3b;
      hsh ^= hsh >> 16;
      return hsh;
    }

    /**
    * @brief Fast specialization for 64 bits-sized types (handles, IDs)
    * @param _val The variable to hash itself
    * @note For Handle / ID types padding is never an issue since they are plain integers.
    * @see https://www.reddit.com/r/C_Programming/comments/1ntrhhh/making_fast_generic_hash_table/
    */
    template<>
    LAMANCHA_INLINE usize hash(const u64& _val) 
    {
      usize hsh = _val;
      hsh ^= hsh >> 30;
      hsh *= 0xbf58476d1ce4e5b9ULL;
      hsh ^= hsh >> 27;
      hsh *= 0x94d049bb133111ebULL;
      hsh ^= hsh >> 31;
      return hsh;
    }

    LAMANCHA_INLINE u8 getEncodedH2(usize hash)
    {
      u8 h2 = hash & 0x7F;
      return h2 == 0 ? 1 : h2; // 0x01 - 0x7F (never 0x00, remapped 0 to 1)
    }

    LAMANCHA_INLINE usize getH1(usize _hash) { return _hash >> 7; }

    static constexpr usize GROUP_SIZE = 16;

    static constexpr u8 META_EMPTY = 0xFF;
    static constexpr u8 META_TOMBSTONE = 0x80;
    // META_OCCUPIED: 0x01 - 0x7F (H2 = hash & 0x7F, remapped 0 to 1)

    struct SimdScanResult 
    {
      u16   hitMask{0};        ///< Bitmask of slots where meta == h2
      u16   emptyMask{0};      ///< Bitmask of slots where meta == META_EMPTY
      usize firstTombstone{0}; ///< First tombstone lane in group, GROUP_SIZE if none
      usize firstEmpty{0};     ///< First empty lane in group, GROUP_SIZE if none
    };

    /** @brief Extract index of lowest set bit in mask */
    LAMANCHA_INLINE usize nextBit(u16 _mask) 
    {
#if defined(__GNUC__) || defined(__clang__)
      return static_cast<usize>(__builtin_ctz(_mask)); // Count trailing zeros
#elif defined(_MSC_VER)
      unsigned long idx;
      _BitScanForward(&idx, _mask);
      return static_cast<usize>(idx);
#else
      usize i = 0;
      while (!(_mask & (1u << i))) { i++; }
      return i;
#endif
    }

    /**
     * @brief Core SIMD probe step. Scans one 16-slot group.
     *
     * @note 
     * Tombstone detection is scalar, tombstones are rare enough that a SIMD third
     * compare would cost more than it saves.
     */
    LAMANCHA_INLINE SimdScanResult scanGroup(u8* _meta, usize _base, u8 _h2)
    {
      using namespace Math;

      SimdBlock16u8 metaVec = SimdBlock16u8::load(&_meta[_base]);

      // Two parallel comparisons over 16 meta[N] bytes
      SimdBlock16u8 h2Vec = broadcast(_h2);                 // H2 candidates for key confirm
      SimdBlock16u8 emptyVec = broadcast(META_EMPTY);       // Probe for termination

      SimdBlock16u8 hitCmp = cmpeq(metaVec, h2Vec);       // Hit Mask
      SimdBlock16u8 emptyCmp = cmpeq(metaVec, emptyVec);  // Empty Mask

      SimdScanResult result;
      result.hitMask = extractMask(hitCmp);
      result.emptyMask = extractMask(emptyCmp);

      // Scalar tombstone scan
      result.firstTombstone = GROUP_SIZE;
      result.firstEmpty = GROUP_SIZE;
      for (usize i = 0; i < GROUP_SIZE; i++)
      {
        // Find first tombstone lane for insert reuse
        if (result.firstEmpty == GROUP_SIZE && _meta[_base + i] == META_EMPTY) 
        { 
          result.firstEmpty = i; 
        }
        if (result.firstTombstone == GROUP_SIZE && _meta[_base + i] == META_TOMBSTONE) 
        { 
          result.firstTombstone = i; 
        }
      }
      return result;
    }
  }
};
