/*
 *  LaMancha Engine
 *  File: hashset.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */
#pragma once

#include "core/hash.h"

namespace LaMancha {
  namespace DataStructures {
    template <typename K, usize N = 32>
    struct HashSetLM
    {
      static_assert((N& (N - 1)) == 0, "N must be a power of two.");
      static_assert(N >= 16, "N must be >= GROUP_SIZE (16).");

      LAMANCHA_SIMD_ALIGN u8 meta[N];
      K keys[N];
      usize liveCount = 0;
      usize growthLeft = 0;

      HashSetLM() { init(); }

      ResultVoid insert(const K& _key)
      {
        if (LAMANCHA_UNLIKELY(growthLeft == 0))
        {
          ResultVoid r = maybeRehashOrGrow();
          if (!r) { return r; }
        }

        const usize h = Utils::hash(_key);
        const usize h1 = Utils::getH1(h);
        const u8 h2 = Utils::getEncodedH2(h);

        for (usize g = 0; g < GROUP_COUNT; g++)
        {
          const usize group = (h1 + g) & (GROUP_COUNT - 1);
          const usize base = group * Utils::GROUP_SIZE;

          Utils::SimdScanResult scan = Utils::scanGroup(meta, base, h2);

          u16 hits = scan.hitMask;
          while (hits)
          {
            usize lane = Utils::nextBit(hits);
            hits &= hits - 1;
            if (LAMANCHA_LIKELY(keys[base + lane] == _key)) { return Ok(); }
          }

          if (scan.firstTombstone < Utils::GROUP_SIZE)
          {
            usize target = base + scan.firstTombstone;
            meta[target] = h2;
            keys[target] = _key;
            liveCount++;
            growthLeft--;
            return Ok();
          }

          if (scan.firstEmpty < Utils::GROUP_SIZE)
          {
            usize target = base + scan.firstEmpty;
            meta[target] = h2;
            keys[target] = _key;
            liveCount++;
            growthLeft--;
            return Ok();
          }
        }
        return ErrorVoid(ErrorCode::OutOfBounds, "HashSet full after full probe.");
      }

      ResultVoid remove(const K& _key)
      {
        const usize h = Utils::hash(_key);
        const usize h1 = Utils::getH1(h);
        const u8 h2 = Utils::getEncodedH2(h);

        for (usize g = 0; g < GROUP_COUNT; g++)
        {
          const usize group = (h1 + g) & (GROUP_COUNT - 1);
          const usize base = group * Utils::GROUP_SIZE;

          Utils::SimdScanResult scan = Utils::scanGroup(meta, base, h2);

          u16 hits = scan.hitMask;
          while (hits)
          {
            usize lane = Utils::nextBit(hits);
            hits &= hits - 1;
            if (LAMANCHA_LIKELY(keys[base + lane] == _key))
            {
              meta[base + lane] = Utils::META_TOMBSTONE;
              liveCount--;
              return Ok();
            }
          }
          if (scan.emptyMask) { return ErrorVoid(ErrorCode::Unknown, "Key not found."); }
        }
        return ErrorVoid(ErrorCode::Unknown, "Key not found.");
      }

      bool has(const K& _key)
      {
        const usize h = Utils::hash(_key);
        const usize h1 = Utils::getH1(h);
        const u8 h2 = Utils::getEncodedH2(h);

        for (usize g = 0; g < GROUP_COUNT; g++)
        {
          const usize group = (h1 + g) & (GROUP_COUNT - 1);
          const usize base = group * Utils::GROUP_SIZE;

          Utils::SimdScanResult scan = Utils::scanGroup(meta, base, h2);

          u16 hits = scan.hitMask;
          while (hits) {
            usize lane = Utils::nextBit(hits);
            hits &= hits - 1;
            if (LAMANCHA_LIKELY(keys[base + lane] == _key)) { return true; }
          }
          if (scan.emptyMask) { return false; }
        }
        return false;
      }

      template <usize BlockSize>
      ArrayListLM<K, BlockSize> getAll() const
      {
        LAMANCHA_ASSERT(liveCount <= N, "ArrayListLM<K, BlockSize>::getAll > Live count over current size.");
        ArrayListLM<K, BlockSize> out;
        for (usize i = 0; i < N; i++)
        {
          if (meta[i] != META_EMPTY && meta[i] != META_TOMBSTONE)
          {
            out.push(keys[i]);
          }
        }
        return out;
      }

      void clear() { init(); }
      usize size() const { return liveCount; }
      f32 loadFactor() const { return static_cast<f32>(liveCount) / N; }

    private:
      static constexpr f32 GROW_THRESHOLD = 0.60f;
      static constexpr usize GROUP_COUNT = N / Utils::GROUP_SIZE;

      void init() {
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

      ResultVoid maybeRehashOrGrow() {
        if (static_cast<f32>(liveCount) / N >= GROW_THRESHOLD)
        {
          LAMANCHA_ASSERT(false, "HashSet needs to grow. Increase N at construction.");
          return ErrorVoid(ErrorCode::OutOfMemory, "HashSet growth not yet implemented.");
        }
        rehashInPlace();
        return Ok();
      }

      void rehashInPlace() {
        K  oldKeys[N];
        u8 oldMeta[N];
        for (usize i = 0; i < N; i++)
        {
          oldKeys[i] = keys[i];
          oldMeta[i] = meta[i];
        }
        init();
        for (usize i = 0; i < N; i++)
        {
          if (oldMeta[i] != META_EMPTY && oldMeta[i] != META_TOMBSTONE)
          {
            insert(oldKeys[i]);
          }
        }
      }
    };
  }
}