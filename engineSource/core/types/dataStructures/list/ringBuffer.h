/*
 *  LaMancha Engine
 *  File: ringBuffer.h
 *
 *  Copyright (C) 2026 Alonso Moreno
 *
 */

#include "core/pch.h"
#include "core/types/dataStructures/array.h"
#include "core/memory/sync.h"

namespace LaMancha {
  namespace DataStructures {
    template <typename T, usize N>
    struct RingBufferLM { // Queue implementation
      static_assert(N > 0, "RingBuffer size must be > 0.");
      static_assert((N& (N - 1)) == 0, "RingBuffer size must be power of 2.");
      ArrayLM<T, N> data;
      usize head = 0, tail = 0;

      // Push a value. Returns false if full (value is dropped).
      ResultVoid push(const T& _val)
      {
        if (isFull()) { return ErrorVoid(ErrorCode::OutOfBounds, "Buffer full."); }
        data[head] = _val;
        head = Math::bwMod((head + 1), N);
        return Ok();
      }

      // Pop oldest value into out. Returns false if empty.
      ResultVoid pop(T& _out)
      {
        if (isEmpty()) { return ErrorVoid(ErrorCode::OutOfBounds, "Buffer empty."); }
        _out = data[tail];
        tail = Math::bwMod((tail + 1), N);
        return Ok();
      }

      // Peek at oldest value without removing it
      ResultVoid peek(T& _out) const
      {
        if (isEmpty()) { return ErrorVoid(ErrorCode::OutOfBounds, "Buffer empty."); }
        _out = data[tail];
        return Ok();
      }

      void  clear() { head = tail = 0; }
      usize size()    const { return (head - tail) & (N - 1); }
      bool  isEmpty() const { return size() == 0; }
      bool  isFull()  const { return size() == N; }
    };

    template <typename T, usize N>
    struct AtomicRingBufferLM {
      static_assert(N > 0, "RingBuffer size must be > 0.");
      static_assert((N& (N - 1)) == 0, "RingBuffer size must be power of 2.");

    private:
      struct Slot {
        T data;
        Sync::Atomic<u8> ready;  ///< 0 = empty, 1 = written and ready to read
      };

    public:
      ArrayLM<Slot, N> slots;
      Sync::Atomic<usize> head{ static_cast<usize>(0) };
      Sync::Atomic<usize> tail{ static_cast<usize>(0) }; ///< next slot producers will claim

      // Called by producer thread(s). Returns false if full.
      ResultVoid push(const T& _val) {
        usize slot = head.fetchAddRelaxed(1);
        slot &= (N - 1);

        // Ring is full: producer lapped consumer. Return failure.
        if (slots[slot].ready.loadAcquire() != 0) { return ErrorVoid(ErrorCode::OutOfBounds, "AtomicRingBuffer full."); }

        slots[slot].data = _val;
        slots[slot].ready.storeRelease(1);
        return Ok();
      }

      // Called by a single consumer thread. Returns false if empty.
      ResultVoid pop(T& _out) {
        usize slot = tail.loadRelaxed() & (N - 1);
        if (slots[slot].ready.loadAcquire() == 0) { return ErrorVoid(ErrorCode::Unknown, "Nothing ready yet on the queue."); }
        _out = slots[slot].data;
        slots[slot].ready.storeRelease(0);
        tail.fetchAddRelaxed(1);
        return Ok();
      }
    };
  }
}
