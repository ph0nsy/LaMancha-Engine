/*
 *  LaMancha Engine
 *  File: hashmap.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */

#pragma once

#include "core/hash.h"
#include "list/arrayList.h"
#include "core/memory/sync.h"
#include "core/memory/arena.h"
#include "core/memory/scratchpad.h"

namespace LaMancha {
  namespace DataStructures {
    /**
     * @brief Swiss Table-inspired open-addressed hashmap.
     *
     * growthLeft set on init(), decrements on every insert.
     * When growthLeft reaches 0, if live/N < GROW_THRESHOLD, rehash in
     * place (clears tombstones); else, if live/N >= GROW_THRESHOLD, grow
     * by N/2 (TODO: requires allocator).
     *
     * Probe pipeline (as seen in get/insert/remove):
     *
     *  1. Hash split: H1 is starting group, H2 is meta fragment
     *
     *  2. Phase 1: SIMD load 16 meta bytes, compare vs H2 and META_EMPTY
     *
     *  3. Phase 2: scalar key confirm on each H2 hit
     *
     * @tparam K    Key type. Must be equality-comparable.
     * @tparam V    Value type.
     * @tparam N    Slot count. Must be power-of-two and >= 16.
     * @param meta[N]    1 byte/slot, SIMD-aligned. Encodes empty/tombstone/H2.
     * @param slots[N]   {K, V} pairs. Only touched on Phase 2 (H2 candidate confirm).
     */
    template <typename K, typename V, usize N = 32>
    struct HashMapLM {
      static_assert((N& (N - 1)) == 0, "N must be a power of two.");
      static_assert(N >= 16, "N must be >= GROUP_SIZE (16).");

      struct Slot {
        K key;
        V val;
      };

      LAMANCHA_SIMD_ALIGN u8 meta[N];     ///< Slot state + H2 fragment
      Slot slots[N];                      ///< Key-value pairs, no state field
      usize liveCount = 0;                ///< Amount of values on the map
      usize growthLeft = 0;               ///< Decrements on every insert; triggers rehash/grow at 0
      ArenaLM* m_scratchArena = nullptr;  ///< optional arena for rehash scratch

      HashMapLM() { init(); }

      /**
       * @brief Insert or update key-value pair.
       * @param _key
       * @param _val
       * @note If key exists, value is overwritten.
       * @note If growthLeft hits 0 before insert, triggers rehash or grow.
       */
      ResultVoid set(const K& _key, const V& _val)
      {
        if (LAMANCHA_UNLIKELY(growthLeft == 0))
        {
          ResultVoid r = maybeRehashOrGrow();
          if (!r) { return r; }
        }

        const usize h = Utils::hash(_key);
        const usize h1 = Utils::getH1(h);
        const u8 h2 = Utils::getEncodedH2(h);

        for (usize grp_idx = 0; grp_idx < GROUP_COUNT; grp_idx++)
        {
          const usize group = (h1 + grp_idx) & (GROUP_COUNT - 1);
          const usize base = group * Utils::GROUP_SIZE;

          // Scan meta for H2 hits, empty slots, tombstones
          Utils::SimdScanResult scan = Utils::scanGroup(meta, base, h2);

          // Check for existing key in H2 candidates
          u16 hits = scan.hitMask;
          while (hits)
          {
            usize lane = Utils::nextBit(hits);
            hits &= hits - 1;  // clear lowest set bit
            if (LAMANCHA_LIKELY(slots[base + lane].key == _key))
            {
              slots[base + lane].val = _val;
              return Ok();  // update without growthLeft change
            }
          }

          // Insert into first available slot (tombstone preferred over empty)
          if (scan.firstTombstone < Utils::GROUP_SIZE)
          {
            usize target = base + scan.firstTombstone;
            meta[target] = h2;
            slots[target] = { _key, _val };
            liveCount++;
            growthLeft--;
            return Ok();
          }

          if (scan.firstEmpty < Utils::GROUP_SIZE)
          {
            usize target = base + scan.firstEmpty;
            meta[target] = h2;
            slots[target] = { _key, _val };
            liveCount++;
            growthLeft--;
            return Ok();
          }
          // Group full (continue to next group)
        }
        return ErrorVoid(ErrorCode::OutOfBounds, "HashMap full after full probe.");
      }

      /**
       * @brief Look up a key.
       * @return Optional pointer to value, None if not found.
       */
      Optional<V*> get(const K& _key)
      {
        const usize h = Utils::hash(_key);
        const usize h1 = Utils::getH1(h);
        const u8 h2 = Utils::getEncodedH2(h);

        for (usize grp_idx = 0; grp_idx < GROUP_COUNT; grp_idx++)
        {
          const usize group = (h1 + grp_idx) & (GROUP_COUNT - 1);
          const usize base = group * Utils::GROUP_SIZE;

          // Scan meta for H2 hits, empty slots, tombstones
          Utils::SimdScanResult scan = Utils::scanGroup(meta, base, h2);

          // Confirm H2 candidates
          u16 hits = scan.hitMask;
          while (hits)
          {
            usize lane = Utils::nextBit(hits);
            hits &= hits - 1; // Clear lowest set bit
            // Found key
            if (LAMANCHA_LIKELY(slots[base + lane].key == _key))
            {
              return Optional<V*>::Some(&slots[base + lane].val);
            }
          }

          // Empty slot in this group (key cannot be further in chain)
          if (scan.emptyMask) { return Optional<V*>::None(); }
        }
        return Optional<V*>::None();
      }

      /** @brief Remove a key, leaving a tombstone. */
      ResultVoid remove(const K& _key)
      {
        const usize h = Utils::hash(_key);
        const usize h1 = Utils::getH1(h);
        const u8 h2 = Utils::getEncodedH2(h);

        for (usize grp_idx = 0; grp_idx < GROUP_COUNT; grp_idx++)
        {
          const usize group = (h1 + grp_idx) & (GROUP_COUNT - 1);
          const usize base = group * Utils::GROUP_SIZE;

          // Scan meta for H2 hits, empty slots, tombstones
          Utils::SimdScanResult scan = Utils::scanGroup(meta, base, h2);

          // Confirm H2 candidates
          u16 hits = scan.hitMask;
          while (hits)
          {
            usize lane = Utils::nextBit(hits);
            hits &= hits - 1;
            if (LAMANCHA_LIKELY(slots[base + lane].key == _key)) {
              meta[base + lane] = Utils::META_TOMBSTONE;
              liveCount--;
              // Not restoring growthLeft (tombstone still costs probe chain)
              return Ok();
            }
          }
          if (scan.emptyMask) { return ErrorVoid(ErrorCode::Unknown, "Key not found."); }
        }
        return ErrorVoid(ErrorCode::Unknown, "Key not found.");
      }

      /**
       * @return Array list with all of the keys in the map
       */
      template <usize BlockSize>
      ArrayListLM<K, BlockSize> getKeys() const
      {
        LAMANCHA_ASSERT(liveCount <= N, "ArrayListLM<K, BlockSize>::getKeys > Live count over current size.");
        ArrayListLM<K, BlockSize> out;
        for (usize i = 0; i < N; i++)
        {
          if (meta[i] != META_EMPTY && meta[i] != META_TOMBSTONE)
          {
            out.push(slots[i].key);
          }
        }
        return out;
      }

      /**
       * @return Array list with all of the values in the map
       */
      template <usize BlockSize>
      ArrayListLM<V, BlockSize> getValues() const
      {
        LAMANCHA_ASSERT(liveCount <= N, "ArrayListLM<K, BlockSize>::getValues > Live count over current size.");
        ArrayListLM<V, BlockSize> out;
        for (usize i = 0; i < N; i++)
        {
          if (meta[i] != META_EMPTY && meta[i] != META_TOMBSTONE)
          {
            out.push(slots[i].val);
          }
        }
        return out;
      }

      /**
       * @brief Transfers all elements from this map onto another one
       * @tparam OtherN Size of the new container (deduced)
       * @param _other The new container itself
       */
      template <usize OtherN>
      ResultVoid putAll(const HashMapLM<K, V, OtherN>& _other) {
        for (usize i = 0; i < OtherN; i++) {
          if (_other.meta[i] != META_EMPTY &&
            _other.meta[i] != META_TOMBSTONE)
          {
            ResultVoid r = set(_other.slots[i].key, _other.slots[i].val);
            if (!r) return r;
          }
        }
        return Ok();
      }

      usize size() const { return liveCount; }
      f32 loadFactor() const { return static_cast<f32>(liveCount) / N; }
      bool has(const K& _key) { return get(_key).hasValue; }
      void  clear() { init(); }

      /**
       * @brief Bind an arena for rehash scratch allocation.
       *
       * When bound, @c rehashInPlace() allocates its temporary copy buffer
       * from a ScratchpadArena instead of the stack. Required when N is
       * large enough that `sizeof(Slot) * N` exceeds safe stack budget (8 KB).
       *
       * The LevelArena or the arena that also backs this HashMap's owner
       * must outlive the HashMap.
       *
       * The stack fallback is used when nullptr.
       */
      void bindArena(ArenaLM& arena) noexcept { m_scratchArena = &arena; }

    private:

      HashMapLM(HashMapLM&&) = default;
      HashMapLM& operator=(HashMapLM&&) = default;

      static constexpr usize GROUP_COUNT = N / Utils::GROUP_SIZE;
      static constexpr f32 GROW_THRESHOLD = 0.6f;

      void init()
      {
        // 0xFF = META_EMPTY across all slots in one hardware call
        for (usize i = 0; i < N; i += 16)
        {
          // Loop unrolling of 16, which N is multiple of.
          meta[i] = Utils::META_EMPTY;
          meta[i + 1] = Utils::META_EMPTY;
          meta[i + 2] = Utils::META_EMPTY;
          meta[i + 3] = Utils::META_EMPTY;
          meta[i + 4] = Utils::META_EMPTY;
          meta[i + 5] = Utils::META_EMPTY;
          meta[i + 6] = Utils::META_EMPTY;
          meta[i + 7] = Utils::META_EMPTY;
          meta[i + 8] = Utils::META_EMPTY;
          meta[i + 9] = Utils::META_EMPTY;
          meta[i + 10] = Utils::META_EMPTY;
          meta[i + 11] = Utils::META_EMPTY;
          meta[i + 12] = Utils::META_EMPTY;
          meta[i + 13] = Utils::META_EMPTY;
          meta[i + 14] = Utils::META_EMPTY;
          meta[i + 15] = Utils::META_EMPTY;
        }
        liveCount = 0;
        growthLeft = static_cast<usize>(N * GROW_THRESHOLD);
      }

      /** @brief Check whether to rehash map or grow it based on liveCount. */
      ResultVoid maybeRehashOrGrow()
      {
        if (static_cast<f32>(liveCount) / N >= GROW_THRESHOLD)
        {
          // TODO: grow > allocate N + N/2 slots, reinsert all live keys.
          // If you are hitting this assert, you have two options right now:
          //  1. Increase N at the call site (e.g. HashMapLM<K,V,512>)
          //  2. Wait for the runtime-N redesign
          //
          // This should never fire in production if N is correctly sized at
          // construction. The IOArena maps use N = LM_MAX_TEXTURES etc.
          // (which should be sized to the realistic maximum asset count).
          LAMANCHA_ASSERT(false, "HashMap needs to grow. Increase N at construction.");
          return ErrorVoid(ErrorCode::OutOfMemory, "HashMap growth not yet implemented, increase N.");
        }
        // Live count below threshold (tombstones are the problem). Rehash in place.
        rehashInPlace();
        return Ok();
      }

      /**
       * @brief Rehash in place: clears tombstones without resizing.
       *
       * Copies all live slots onto the stack, wipes the table, reinserts every
       * live entry fresh.
       *
       * @note Stack-allocates N Slot objects. Safe for small N. Replace stack
       * copy with scratch arena allocation when allocator is available.
       */
      void rehashInPlace()
      {
        // Stack budget guard
        static constexpr usize STACK_BUDGET = 8_KB;
        static constexpr usize SLOT_BYTES = sizeof(Slot) * N + N; // slots + meta
        if (m_scratchArena)
        {
          // Arena path, safe for any N.
          ScratchpadArenaLM scratch = ScratchpadArenaLM(*m_scratchArena);

          Optional<Slot*> oldSlots = scratch.allocArray<Slot>(N);
          Optional<u8*>   oldMeta = scratch.allocArray<u8>(N);

          // If scratch allocation fails, fall through to stack path 
          // (this only happens if the arena is nearly full).
          if (!oldSlots || !oldMeta) {
            Logging::WarningLog("HashMapLM::rehashInPlace > scratch alloc failed, "
              "falling back to stack. Consider increasing arena size.");
            goto stack_path; // Jump forward past return to fallback.
          }

          for (usize i = 0; i < N; i++) 
          {
            oldSlots.value()[i] = slots[i];
            oldMeta.value()[i] = meta[i];
          }
          
          init();
          for (usize i = 0; i < N; i++) {
            if (oldMeta.value()[i] != Utils::META_EMPTY && oldMeta.value()[i] != Utils::META_TOMBSTONE) 
            {
              set(oldSlots.value()[i].key, oldSlots.value()[i].val);
            }
          }       
          // scratch destructor restores the arena automatically
          return;
        }

      // Stack path (Only safe for small N)
      stack_path:
        static_assert(SLOT_BYTES <= STACK_BUDGET, "HashMapLM: N is too large for stack rehash. "
          "Call bindArena() before use to enable arena-backed rehash.");
         
        Slot oldSlots[N];
        u8 oldMeta[N];
        for (usize i = 0; i < N; i++)
        {
          oldSlots[i] = slots[i];
          oldMeta[i] = meta[i];
        }
        init();
        for (usize i = 0; i < N; i++)
        {
          if (oldMeta[i] != Utils::META_EMPTY && oldMeta[i] != Utils::META_TOMBSTONE)
          {
            set(oldSlots[i].key, oldSlots[i].val);
          }
        }
      }
    };
  }
}
