/**
 * @file LaManchaObject.h
 * @brief Base class template for all user-facing LaMancha Engine objects
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * @a LaManchaObject<Lifetime> overrides operator @c new and operator @c delete
 * to route allocations through the engine's allocator system rather
 * than the OS heap. It is the only mechanism that makes natural C++
 * new/delete syntax work with the arena and pool allocators.
 *
 * Users do not inherit from LaManchaObject directly. They inherit from
 * the named semantic bases in @a Actor.h, @a UIObject.h, and @a Manager.h.
 *
 * operator new:
 * Allocates from Lifetime::arena. 
 * In debug, asserts that the calling thread matches @c Lifetime::ownerThread. 
 * A missing @c bind() or wrong-thread allocation fires immediately at the call site.
 *
 * operator delete:
 * No-op on the memory. The destructor has already run before this is
 * called (C++ guarantees this). Arena memory is reclaimed in bulk when
 * the arena resets, individual slots are not reusable between new/delete
 * pairs within a level. For slot reuse, use @a TypedPool<T> directly.
 *
 * Virtual destructor:
 * Required so deleting through a base pointer runs the full destructor
 * chain. Without it, delete actor would run @c ~Actor() only, not @c ~Enemy().
 *
 * @note 
 * Do not inherit from LaManchaObject directly. Use Actor, Component,
 * UIWidget, Manager, etc. from their respective headers.
 */

#pragma once

#include "core/memory/sync.h"    ///< Sync::gTelemetry

namespace LaMancha {
  /**
   * @brief Base class for all user-created engine objects.
   *
   * @tparam Lifetime One of: LevelLifetime, SessionLifetime,
   *                   PermanentLifetime, AssetLifetime.
   */
  template <typename Lifetime>
  struct LaManchaObject
  {

    /**
     * @brief Allocate from the Lifetime's arena.
     *
     * Asserts in debug that:
     *   - The arena has been bound (Lifetime::bind() was called)
     *   - The calling thread is the correct owner for this lifetime
     *
     * On failure: increments oom_ecs_pool telemetry and returns nullptr.
     * The caller's constructor will crash on the first member access,
     * making the failure immediately visible in debug.
     */
    static void* operator new(usize size) noexcept
    {
      LAMANCHA_ASSERT(Lifetime::arena != nullptr,
        "LaManchaObject::operator new: lifetime arena not bound. Call Lifetime::bind() before creating objects of this type.");

#if LAMANCHA_DEBUG
      LAMANCHA_ASSERT(Thread::currentRole() == Lifetime::ownerThread,
        "LaManchaObject::operator new: wrong thread. See Lifetimes.h for thread ownership assignments.");
#endif

      Optional<void*> mem = Lifetime::arena->alloc(size, alignof(uptr));
      if (!mem)
      {
        Sync::gTelemetry.oom_ecs_pool.fetchAddRelaxed(1u);
        return nullptr;
      }
      return mem.value();
    }

    /**
     * @brief No-op. Arena memory is reclaimed at arena reset, not per-object.
     *
     * The destructor has already run before this is called.
     * If per-object slot reuse is needed, use TypedPool<T> instead.
     */
    static void operator delete(void* /*ptr*/) noexcept
    {
      // Intentional no-op. See file header.
    }

    static void* operator new[](usize size) noexcept
    {
      return LaManchaObject::operator new(size);
    }

    static void operator delete[](void* /*ptr*/) noexcept
    {
      // Intentional no-op.
    }

    virtual ~LaManchaObject() noexcept = default;

  protected:
    LaManchaObject() noexcept = default;
  };
}