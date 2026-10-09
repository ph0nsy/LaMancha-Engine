/**
 * @file garbageCollector.h
 * @brief Job types for the Utility Thread GC queue and Asset Thread request queue
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * Two ring buffers connect the engine's threads for deferred work:
 *
 *  GCJobQueue: Logic Thread pushes, Utility Thread pops and executes.
 *  Lives on the Utility Thread (its inbox).
 *  Capacity: 256 jobs × 8 bytes = 2 KB.
 *
 *  AssetRequestQueue: Logic Thread pushes, Asset Thread pops and executes.
 *  Lives on the Asset Thread (its inbox).
 *  Capacity: 64 requests × 128 bytes = 8 KB.
 *
 * Neither queue owns its storage, both are backed by their owning
 * thread's HeapAllocatorLM sub-region, initialised at thread startup.
 *
 * DeleteGLObj two-hop:
 *   GL calls must happen on the Main Thread (OpenGL context owner).
 *   When the GC needs to delete a GPU object, it pushes a DeleteGLObj
 *   job onto the Main Thread's GL command queue (not this queue).
 *   The GCJob carries the GLuint handle across the hop.
 */

#pragma once

#include "core/pch.h"
#include "core/types/handle.h"                      
#include "core/types/dataStructures/list/ringBuffer.h"
#include "core/memory/ioArena.h"
#include "core/memory/levelArena.h"
#include "core/memory/sync.h"

// Lua forward declaration (included fully once Lua is integrated)
/*
struct lua_State;
extern "C" int lua_gc(lua_State* L, int what, int data);
static constexpr int LUA_GCSTEP = 5;   // as seen in lua.h
*/

namespace LaMancha {
  namespace GC {
  enum class JobType : u8 {
    EvictTexture = 0,     ///< IOArena: evict a texture slot
    EvictAudio = 1,       ///< IOArena: evict an audio slot
    EvictFont = 2,        ///< IOArena: evict a font slot
    ResetLevel = 3,       ///< LevelArena: unload() after level transition
    DeleteGLObj = 4,      ///< forward to Main Thread GL command queue
    LuaGCStep = 5,        ///< lua_gc(L, LUA_GCSTEP, steps)
  };

  /**
   * @brief Tagged union carrying one unit of deferred GC work.
   *
   * Sized to 8 bytes so 256 slots fit in 2 KB.
   * The type tag is in the first byte; the payload follows.
   */
  struct GCJob {
    JobType type;
    u8 _pad[3];
    union {
      TextureHandle textureHandle;  ///< EvictTexture
      AudioHandle audioHandle;      ///< EvictAudio
      FontHandle fontHandle;        ///< EvictFont
      u32 glHandle;                 ///< DeleteGLObj (GLuint)
      u32 luaSteps;                 ///< LuaGCStep
      // ResetLevel carries no payload, the LevelArena is a singleton
    };
  };

  static_assert(sizeof(GCJob) == 8, "GCJob must be 8 bytes, adjust padding or handle size if this fires");

  // Declared extern here, defined in .cpp.
  // Initialised at engine startup before threads are started.
  //
  // GCJobQueue: Utility Thread's inbox. Logic Thread writes to it.
  // AssetRequestQueue: Asset Thread's inbox.  Logic Thread writes to it.
  //
  // Both use AtomicRingBufferLM which is safe for single-producer
  // single-consumer use without a mutex.

  extern DataStructures::AtomicRingBufferLM<GCJob, 256> gGCJobQueue;
  extern DataStructures::AtomicRingBufferLM<AssetRequest, 64> gAssetRequestQueue;

  /**
   * @brief DeleteGLObj jobs cannot be executed here
   * 
   * @details
   * Called by the Utility Thread each iteration of its loop.
   * Drains up to the entire GCJobQueue in one call.
   *
   * The LevelArena and IOArena pointers are non-owning, the GC does not
   * own these arenas. It holds references set at engine startup.
   *
   * GL calls must happen on the Main Thread. These jobs are re-queued onto
   * the Main Thread's GL command queue (gGLDeleteQueue, defined in the renderer).
   *
   * LuaGCStep jobs are stubbed until Lua integration is complete.
   *
   * @param io The engine's IOArena.
   * @param levelArena The current level's LevelArena. May be nullptr
   * if no level is loaded (startup, shutdown).
   * @param luaState The Lua VM state. May be nullptr before Lua init.
   */
  void ProcessJobsGC(IOArenaLM& io, LevelArenaLM* levelArena/*, lua_State* luaState*/);
  

  /**
   * @brief Pops asset load requests pushed by the Logic Thread via requestTexture()
   * and friends, and initiates loading.
   *
   * @details
   * The actual loading (file I/O, decompression) is done by the asset
   * subsystem functions (loadTexture, loadMesh, etc.) which will be
   * implemented in the asset loading subsystem.
   *
   * @param io The engine's IOArena. The Asset Thread allocates slots here.
   */
  void ProcessAssetRequests(IOArenaLM& io);


  } // namespace GC
} // namespace LaMancha