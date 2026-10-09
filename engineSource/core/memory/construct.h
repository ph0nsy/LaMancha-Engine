/**
 * @file Construct.h
 * @brief In-place construction and destruction utilities for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * constructAt and destroyAt are the ONLY place in the engine where
 * placement new and explicit destructor calls appear. All other code
 * that needs to construct or destruct objects in pre-allocated memory
 * goes through these two functions.
 *
 * This centralizes the unsafe operations in one clearly documented
 * location. grep for "new" or "->~" in the codebase and you should
 * find only this file.
 *
 * - Usage:
 * Memory comes from a pool or arena. These functions do not allocate.
 * @code
 *   Optional<Enemy*> slot = enemyPool.alloc();
 *   if (!slot) { return; }
 *
 *   Enemy* e = constructAt<Enemy>(slot.value(), startPos, health);
 *   // e is fully constructed at slot.value()
 *
 *   // ... later, when the enemy is destroyed ...
 *   destroyAt(e);
 *   enemyPool.free(e);
 * @endcode
 *
 * - Rules:
 *   - Use @c constructAt when T has a non-trivial constructor that must run.
 *   For plain data structs, prefer direct field assignment: t->position = Vec3{0,0,0};
 *   Direct assignment is clearer and has identical codegen for trivially constructible types.
 *   - Use @c destroyAt ONLY when T has a non-trivial destructor. Calling destroyAt
 *   on a trivially destructible type is a misuse. the debug assert will catch it. 
 *   For plain data structs, just call pool.free() directly.
 *
 * @note 
 * Why placement new is acceptable here. The memory passed to constructAt always comes
 * from a pool or arena. No OS call is made. No heap is involved.
 * - Regular new: allocates memory + constructs > violates no-heap rule.
 * - Placement new: no allocation + constructs > safe, memory is yours.
 */

#pragma once

#include "core/pch.h"   ///< LAMANCHA_ASSERT, LAMANCHA_INLINE

namespace LaMancha {
  /**
   * @brief Construct a T in pre-allocated memory using the given arguments.
   *
   * @details
   * Calls T's constructor at the address pointed to by ptr.
   * Does NOT allocate memory, ptr must already point to a valid block
   * of at least sizeof(T) bytes aligned to alignof(T).
   *
   * @tparam T Type to construct.
   * @tparam Args Constructor argument types (deduced).
   *
   * @param _ptr Destination memory. Must be non-null and correctly aligned.
   * Must point to at least sizeof(T) bytes. Typically the result of @c pool.alloc() 
   * or @c arena.alloc<T>().
   * @param _args Arguments forwarded to T's constructor.
   *
   * @returns ptr cast to @c T* . The same address, now pointing to a
   * fully constructed T.
   *
   * @code
   *   Optional<Enemy*> slot = enemyPool.alloc();
   *   LAMANCHA_ASSERT(slot.hasValue);
   *   Enemy* e = constructAt<Enemy>(slot.value(), startPos, health);
   * @endcode
   */
  template <typename T, typename... Args>
  LAMANCHA_INLINE T* constructAt(void* _ptr, Args&&... _args) noexcept
  {
    LAMANCHA_ASSERT(ptr != nullptr, "constructAt > _ptr is null. Did alloc() return None?");

    // This is the one placement new call in the engine.
    // It constructs T at ptr using the provided arguments.
    // No memory is allocated. The :: prefix ensures we call the global
    // placement new, not any class-specific override.
    
    #pragma push_macro("new")
    #undef new // Protect against redefinition

    T* allocatedObject = ::new (memoryLocation) T(forward<Args>(args)...);

    #pragma pop_macro("new")

    return allocatedObject;
  }

  /**
   * @brief Explicitly call T's destructor on an object in pool/arena memory.
   *
   * Does NOT free memory. Call pool.free() or let the arena reset handle
   * that separately, AFTER calling destroyAt.
   *
   * Call order is always:
   *   1. @c destroyAt(ptr) > destructor runs, resources released
   *   2. @c pool.free(ptr) > slot returned to pool
   *
   * @tparam T Type to destruct. Must be non-trivially destructible.
   * Use the debug build to catch misuse on trivial types.
   *
   * @param ptr Object to destroy. Must have been constructed by constructAt
   * or by the type's operator new routing through pool/arena memory. Must not 
   * be null.
   *
   * @code
   *   destroyAt(e);          // destructor runs, e releases its resources
   *   enemyPool.free(e);     // slot returned, memory reusable
   *   e = nullptr;           // prevent accidental use after destroy
   * @endcode
   */
  template <typename T>
  LAMANCHA_INLINE void destroyAt(T* ptr) noexcept
  {
    LAMANCHA_ASSERT(ptr != nullptr, "destroyAt > ptr is null");

#if LAMANCHA_DEBUG
    // Catch misuse: if T is trivially destructible, the caller should
    // not be calling destroyAt at all. Just call pool.free() directly.
    // __is_trivially_destructible is a compiler intrinsic available on
    // GCC, Clang, and MSVC, so no <type_traits> needed.
    static_assert(!__is_trivially_destructible(T),
      "destroyAt > called on a trivially destructible type. For plain data types, skip destroyAt and call pool.free() directly.");
#endif

    // Explicit destructor call. This is the only place in the engine
    // where ->~T() appears.
    ptr->~T();
  }
}