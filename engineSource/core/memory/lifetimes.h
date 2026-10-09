/**
 * @file lifetimes.h
 * @brief Lifetime tags for LaMancha Engine object allocation
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * Lifetime tags are plain structs that carry three pieces of information:
 * - @c arena > static pointer to the backing allocator, set by bind()
 * - @c ownerThread > which @a CoreAffinity may allocate from this lifetime
 * - @c bind() > called once during engine init to wire up the arena
 *
 * These tags are used as template parameters for LaManchaObject<Lifetime>.
 * They have no data members and no virtual functions. Their only purpose
 * is to carry type-level information that @c LaManchaObject::operator @c new
 * uses at runtime.
 *
 * All lifetime tags must be bound before any LaManchaObject of that
 * lifetime is constructed. Binding happens in main() after allocators
 * are created but before threads are started:
 *
 * @code
 *   PermanentLifetime::bind(mainThread.permanentArena);
 *   LevelLifetime::bind(logicThread.levelArena);
 *   SessionLifetime::bind(logicThread.sessionArena);
 *   AssetLifetime::bind(assetThread.ioArena);
 * @endcode
 *
 * @note
 * Thread ownership:
 * - LevelLifetime > Logic Thread (game objects, UI, VFX, scripts)
 * - SessionLifetime > Logic Thread (UI persisting across levels)
 * - PermanentLifetime > Main Thread (engine singletons, shaders)
 * - AssetLifetime > Asset Thread (simulation, loaded resources)
 */

#pragma once

#include "arena.h"
#include "levelArena.h"
#include "permanent.h"
#include "thread.h"   ///< CoreAffinity

namespace LaMancha {
  /**
   * @brief Level-scoped lifetime. Reset on level unload.
   *
   * @details
   * Owned by the @a LogicThread. Holds everything that lives exactly
   * as long as the current level: actors, components, UI widgets,
   * VFX emitters, audio sources, scripts.
   */
  struct LevelLifetime
  {
    static LevelArenaLM* arena;
    static CoreAffinity  ownerThread;

    /**
     * @brief Bind this lifetime to a @c LevelArena.
     * Call once during engine init, before any @a LogicThread object is created.
     */
    static void bind(LevelArenaLM& a) noexcept
    {
      arena = &a;
      ownerThread = CoreAffinity::Logic;
    }
  };

  /**
   * @brief Session-scoped lifetime. Persists across level transitions.
   *
   * @details
   * Owned by the @a LogicThread. Holds UI canvases and persistent game state
   * that survives level unloads. Backed by a dedicated session arena.
   */
  struct SessionLifetime
  {
    static ArenaLM* arena;
    static CoreAffinity ownerThread;

    /**
     * @brief Bind this lifetime to an ArenaLM (typically a session arena).
     * Call once during engine init.
     */
    static void bind(ArenaLM& a) noexcept {
      arena = &a;
      ownerThread = CoreAffinity::Logic;
    }
  };

  /**
   * @brief Engine lifetime. Never reset during normal operation.
   *
   * @details
   * Owned by the @a MainThread. Holds anything that lives for the entire session: 
   * engine subsystem singletons, shader programs, manager objects.
   */
  struct PermanentLifetime
  {
    static PermanentArenaLM* arena;
    static CoreAffinity    ownerThread;

    /**
     * @brief Bind this lifetime to the @c PermanentArena.
     * Call once during engine init, before any Manager is created.
     */
    static void bind(PermanentArenaLM& a) noexcept
    {
      arena = &a;
      ownerThread = CoreAffinity::Main;
    }
  };

  /**
   * @brief Asset-side lifetime. Owned by the @a AssetThread.
   *
   * @details
   * Holds simulation objects and loaded resource descriptors whose
   * lifecycle is driven by the @a AssetThread : VFX simulation state,
   * audio stream state, pathfinding data.
   */
  struct AssetLifetime
  {
    static ArenaLM* arena;
    static CoreAffinity ownerThread;

    /**
     * @brief Bind this lifetime to an @c ArenaLM (typically the @c OArena 's
     * internal arena). Call once during engine init.
     */
    static void bind(ArenaLM& a) noexcept
    {
      arena = &a;
      ownerThread = CoreAffinity::Asset;
    }
  };
}