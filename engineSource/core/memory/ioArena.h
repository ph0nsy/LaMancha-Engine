/**
 * @file ioArena.h
 * @brief Individual-lifetime asset allocator for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * @a IOArena manages assets whose lifetimes do not align with level or frame
 * boundaries. Each asset has its own load time and its own eviction time.
 *
 * Unlike other arenas, @a IOArena does mot have a bulk reset. Assets are
 * allocated and freed individually via reference counting. Zero
 * fragmentation is possible because each asset type has its own
 * fixed-size typed pool.
 *
 * Three threads interact with IOArena:
 * - @a AssetThread: allocates slots, loads data, publishes handles
 * - @a LogicThread: requests assets, acquires/releases handles
 * - @a UtilityThread: evicts assets when refCount reaches zero
 *
 * They never touch the same slot simultaneously. The coordination is
 * sequenced through reference counts and the @a GCJobQueue.
 *
 * - Handles:
 * Assets are referenced through @c Handle<Tag> generational indices.
 * When an asset is evicted its generation increments. Any handle
 * from before the eviction fails the generation check and returns None.
 * This prevents use-after-free without any pointer invalidation.
 *
 * - Reference counting:
 * Logic Thread calls @c addRef(handle) to hold an asset alive.
 * Logic Thread calls @c release(handle) when done. Eviction only happens
 * when @c refCount equals 0.
 *
 * - Hash map:
 * Path hash (FNV-1a u64) maps to slot index per asset type.
 * `Protected by m_mapLock, acquired briefly on insert and remove.` I am uncertain about this
 * Hot read path (lookup by existing handle) does NOT lock.
 *
 * - Construction:
 * @a IOArena is created from the @a AssetThread's @a HeapAllocatorLM sub-region.
 * All internal pools are allocated from the same backing arena.
 */

#pragma once

#include "core/types/handle.h"  ///< Handle<Tag>, TextureHandle, etc.
#include "core/memory/sync.h"   ///< Sync::Atomic, Sync::Mutex, Sync::MutexGuard
#include "core/memory/arena.h"
#include "core/memory/pool.h"
#include "core/types/dataStructures/hashmap.h"

namespace LaMancha {

  enum class AssetRequestType : u8 {
    Texture = 0,
    Audio = 1,
    Font = 2,
  };

  enum class AssetState : u8 {
    Empty = 0,    // Slot is free, no asset here
    Loading = 1,  // Asset Thread is filling this slot
    Ready = 2,    // Asset is fully loaded and usable
  };

  /**
   * @brief Request from Logic Thread to Asset Thread to load an asset.
   *
   * Fixed-size at 128 bytes so the path fits without dynamic allocation.
   * Path is capped at 119 characters + null terminator.
   * 64 slots × 128 bytes = 8 KB total queue size.
   */
  struct AssetRequest {
    AssetRequestType type;
    u8 _pad[3];
    u64 pathHash;       ///< FNV-1a > TODO: must be pre-computed by requestTexture()
    char path[105];     ///< null-terminated asset path
  };

  static_assert(sizeof(AssetRequest) == 128,
    "AssetRequest must be 128 bytes, adjust path length if this fires");

  /**
   * @brief Header for type pool entry.
   * Every asset has this header, followed by the type-specific
   * data (TextureAsset, MeshAsset, etc.).
   *
   * @note
   * sizeof(AssetSlotHeader) = 24 bytes.
   * All pools align slots to at least alignof(AssetSlotHeader) = 8 bytes.
   */
  struct AssetSlotHeader {
    Sync::Atomic<u16> refCount; ///< reference count, 0 = evictable
    u16 generation;             ///< incremented on each eviction
    u64 pathHash;               ///< FNV-1a hash of the asset path
    AssetState state;           ///< current load state
  };

  // Each asset type has a slot structure: the common header followed by
  // the type-specific data. The pool allocates these as fixed-size units.
  //
  // Asset data structs are defined here as forward declarations.
  // Their full definitions live in (TODO) the relevant subsystem headers
  // (texture.h, audioClip.h, font.h).

  struct TextureAsset;   ///< TODO: Defined in render/texture.h
  struct AudioAsset;     ///< TODO: Defined in audio/audioClip.h
  struct FontAsset;      ///< TODO: Defined in ui/font.h

  struct TextureSlot { AssetSlotHeader header; TextureAsset* data; };
  struct AudioSlot { AssetSlotHeader header; AudioAsset* data; };
  struct FontSlot { AssetSlotHeader header; FontAsset* data; };

  // These constants define the maximum number of simultaneously loaded
  // assets of each type. Chosen to fit within the R36S memory budget.
  // Override in your project's config header if needed.
#ifndef LM_MAX_TEXTURES
  static constexpr usize LM_MAX_TEXTURES = 256;
#endif
#ifndef LM_MAX_AUDIO_CLIPS
  static constexpr usize LM_MAX_AUDIO_CLIPS = 128;
#endif
#ifndef LM_MAX_FONTS
  static constexpr usize LM_MAX_FONTS = 32;
#endif

  struct IOArenaLM {

    /**
     * @brief Create an IOArena backed by an ArenaLM.
     *
     * Allocates all internal pools from the provided arena. The arena
     * must outlive the IOArena. Returns None if the arena cannot satisfy
     * all pool allocations.
     *
     * Call from the Asset Thread after its HeapAllocatorLM is initialised.
     *
     * @param arena    Backing arena (Asset Thread's sub-region).
     */
    static Optional<IOArenaLM> fromArena(ArenaLM& arena) noexcept;

    IOArenaLM() noexcept = default;
    // Non-copyable, movable.
    IOArenaLM(const IOArenaLM&) = delete;
    IOArenaLM& operator=(const IOArenaLM&) = delete;
    IOArenaLM(IOArenaLM&&) noexcept = default;
    IOArenaLM& operator=(IOArenaLM&&) noexcept = default;

    /**
     * @brief Request a texture by path. Called from the Logic Thread.
     *
     * Three outcomes:
     *   - Already loaded and Ready > increments refCount, returns handle
     *   - Currently Loading > returns null handle (check next frame)
     *   - Not present > pushes load job, returns null handle
     *
     * The returned handle is valid as long as the caller holds a reference.
     * Call release(handle) when done to allow eviction.
     *
     * @param path      Asset path string (e.g. "textures/hero.png")
     * @param pathLen   Length of path string
     */
    TextureHandle requestTexture(const char* _path, usize _pathLen) noexcept;
    AudioHandle requestAudio(const char* _path, usize _pathLen) noexcept;
    FontHandle requestFont(const char* _path, usize _pathLen) noexcept;

    /**
     * @brief Increment the reference count for a texture handle.
     *
     * Call when storing a handle that must keep the asset alive.
     * Every addRef must be paired with exactly one release.
     *
     * Returns false if the handle is stale (asset was evicted).
     */
    bool addRef(TextureHandle _h) noexcept;
    bool addRef(AudioHandle _h) noexcept;
    bool addRef(FontHandle _h) noexcept;

    /**
     * @brief Decrement the reference count.
     *
     * When refCount reaches zero, pushes an EVICT_ASSET job to the
     * Utility Thread's GCJobQueue. The Utility Thread performs the
     * actual eviction asynchronously.
     *
     * On stale handle call does nothing.
     */
    void release(TextureHandle _h) noexcept;
    void release(AudioHandle _h) noexcept;
    void release(FontHandle _h) noexcept;

    /**
     * @brief Get a pointer to a TextureAsset by handle.
     *
     * Does NOT lock. Generation check is the only validation.
     * Returns None if the handle is stale or null.
     *
     * The returned pointer is valid as long as the caller holds a
     * reference (refCount > 0). Do not cache the pointer beyond the
     * current frame without holding a reference.
     */
    Optional<TextureAsset*> get(TextureHandle _h) noexcept;
    Optional<AudioAsset*> get(AudioHandle _h) noexcept;
    Optional<FontAsset*> get(FontHandle _h) noexcept;

    /**
     * @brief Allocate a slot for a texture being loaded.
     *
     * Called by the Asset Thread when it begins loading an asset.
     * Sets state = Loading. The slot is exclusively owned by the
     * Asset Thread until publish() is called.
     *
     * Returns None if the texture pool is exhausted.
     *
     * @param pathHash  FNV-1a hash of the asset path.
     */
    Optional<TextureHandle> beginTextureLoad(u64 _pathHash) noexcept;
    Optional<AudioHandle> beginAudioLoad(u64 _pathHash) noexcept;
    Optional<FontHandle> beginFontLoad(u64 _pathHash) noexcept;

    /**
     * @brief Publish a loaded texture slot.
     *
     * Called by the Asset Thread when loading is complete.
     * Sets state = Ready and inserts into the hash map.
     * After this call, the Logic Thread can acquire the handle.
     *
     * @param h         Handle returned by beginLoad.
     * @param pathHash  FNV-1a hash used to look up this asset later.
     */
    void publish(TextureHandle _h, u64 _pathHash) noexcept;
    void publish(AudioHandle _h, u64 _pathHash) noexcept;
    void publish(FontHandle _h, u64 _pathHash) noexcept;

    /**
     * @brief Attempt to evict a texture slot.
     *
     * Called by the Utility Thread when processing a EVICT_ASSET job.
     * Only evicts if refCount is still 0. If a new reference was acquired
     * between the release() call and this eviction, does nothing.
     *
     * Eviction sequence:
     *   1. loadAcquire refCount: if != 0, abort (new ref acquired)
     *   2. increment generation: stales all existing handles
     *   3. lock map > remove path hash entry > unlock
     *   4. pool.free(slot): slot available for reuse
     */
    void evict(TextureHandle _h) noexcept;
    void evict(AudioHandle _h) noexcept;
    void evict(FontHandle _h) noexcept;

    usize texturesLoaded() const noexcept { return m_textures.usedSlots(); }
    usize audioLoaded() const noexcept { return m_audio.usedSlots(); }
    usize fontsLoaded() const noexcept { return m_fonts.usedSlots(); }

    f32 textureUsageRatio() const noexcept { return m_textures.usageRatio(); }
    f32 audioUsageRatio() const noexcept { return m_audio.usageRatio(); }
    f32 fontUsageRatio() const noexcept { return m_fonts.usageRatio(); }

  private:

    // One pool per asset type.
    TypedPool<TextureSlot> m_textures;
    TypedPool<AudioSlot> m_audio;
    TypedPool<FontSlot> m_fonts;

    // Path hash to slot index, one per asset type.
    // Using u64 key (FNV-1a path hash) > u16 slot index.
    // Separate maps per type avoid hash collisions across types.
    // All protected by m_mapLock.
    DataStructures::HashMapLM<u64, u16, LM_MAX_TEXTURES>* m_textureMap;
    DataStructures::HashMapLM<u64, u16, LM_MAX_AUDIO_CLIPS>* m_audioMap;
    DataStructures::HashMapLM<u64, u16, LM_MAX_FONTS>* m_fontMap;

    // Single mutex protecting all hash map
    // Tiny lock window of hash map lookup or insert.
    // Asset data reads do NOT acquire this lock (hot read path is lock-free).
    // This is a pointer because Option gives an error due to Mutex being non-movable.
    Sync::Mutex* m_mapLock = nullptr;

    template <typename SlotT, typename HandleT>
    SlotT* slotFromHandle(TypedPool<SlotT>& _pool, HandleT _h) noexcept {
      if (_h.isNull()) { return nullptr; }
      SlotT* slot = _pool.getByIndex(_h.index());
      if (!slot || slot->header.generation != _h.generation()) { return nullptr; }
      return slot;
    }

    template <typename SlotT, typename HandleT, usize MapSize>
    Optional<HandleT> requestImpl(
      TypedPool<SlotT>& _pool,
      DataStructures::HashMapLM<u64, u16, MapSize>& _map,
      const char* _path, usize _pathLen,
      AssetRequestType reqType) noexcept
    {
      u64 hashpath = Utils::hash(_path, _pathLen);
      {
        Sync::MutexGuard g(*m_mapLock);

        Optional<u16*> found = _map.get(hashpath);
        if (found) {
          // If found in map, check state.
          SlotT* slot = _pool.getByIndex(*found.value());
          if (!slot) { return Optional<HandleT>::None(); }

          if (slot->header.state == AssetState::Ready) {
            // Increment refCount under the lock so eviction cannot race.
            slot->header.refCount.fetchAddAcquire(1u);
            return Optional<HandleT>::Some(HandleT::create(*found.value(), slot->header.generation));
          }
          // Still loading, return null, caller retries next frame.
          return Optional<HandleT>::None();
        }
      } // Release lock

      // Not found. Push a load request to the Asset Thread.
      AssetRequest req;
      req.type = reqType;
      req.pathHash = hashpath;
      usize copyLen = _pathLen < 119u ? _pathLen : 119u;
      for (usize i = 0; i < copyLen; ++i) { req.path[i] = _path[i]; }
      req.path[copyLen] = '\0';
      GC::gAssetRequestQueue.push(req);   // if full, request dropped (caller must retry next frame)
      return Optional<HandleT>::None();
    }

    template <typename SlotT, typename HandleT>
    Optional<HandleT> beginLoadImpl(TypedPool<SlotT>& _pool, u64 _pathHash) noexcept 
    {
      Optional<SlotT*> slot = _pool.alloc();
      if (!slot) { return Optional<HandleT>::None(); }

      SlotT* s = slot.value();
      s->header.refCount.storeRelaxed(0u);
      s->header.state = AssetState::Loading;
      s->header.pathHash = _pathHash;
      // Generation is carried from previous use of this slot (don't reset here).
      // The generation only increments on eviction, not on alloc.

      // Compute index from pointer arithmetic over the pool's block.
      // This is safe because TypedPool allocates a contiguous block.
      // index = (ptr - blockStart) / slotSize
      u16 idx = _pool.indexOfSlot(s);
      return Optional<HandleT>::Some(HandleT::create(idx, s->header.generation));
    }
  };
} // namespace LaMancha