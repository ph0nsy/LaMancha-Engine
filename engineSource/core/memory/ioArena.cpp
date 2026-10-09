#include "core/pch.h"
#include "ioArena.h"
#include "core/hash.h"
#include "core/memory/garbageCollector.h"

using namespace LaMancha;

Optional<IOArenaLM> IOArenaLM::fromArena(ArenaLM& arena) noexcept
{
  IOArenaLM io;
  Optional<Sync::Mutex*> mtxSlot = arena.alloc<Sync::Mutex>();
  LAMANCHA_ASSERT(mtxSlot, "IOArenaLM::fromArena() > Empty mutex pointer.");
  Sync::Mutex* mtx = new (mtxSlot.value()) Sync::Mutex();
  ResultVoid r = mtx->init();
  // Initialise mutex.
  if (!r) { return Optional<IOArenaLM>::None(); }
  io.m_mapLock = mtx;

  // Allocate all pools from the backing arena.
  // If any allocation fails, the whole IOArena fails.
  
  // TEXTURE
  {
    using TextureMap = DataStructures::HashMapLM<u64, u16, LM_MAX_TEXTURES>;
    Optional<TextureMap*> slot = arena.alloc<TextureMap>();
    LAMANCHA_ASSERT(slot, "IOArenaLM: failed to allocate texture map");
    io.m_textureMap = new (slot.value()) TextureMap();
    io.m_textureMap->bindArena(arena);  // bind for rehash scratch
  }

  // AUDIO 
  {
    using AudioMap = DataStructures::HashMapLM<u64, u16, LM_MAX_AUDIO_CLIPS>;
    Optional<AudioMap*> slot = arena.alloc<AudioMap>();
    LAMANCHA_ASSERT(slot, "IOArenaLM: failed to allocate texture map");
    io.m_audioMap = new (slot.value()) AudioMap();
    io.m_audioMap->bindArena(arena);
  }

  // FONT
  {
    using FontMap = DataStructures::HashMapLM<u64, u16, LM_MAX_FONTS>;
    Optional<FontMap*> slot = arena.alloc<FontMap>();
    LAMANCHA_ASSERT(slot, "IOArenaLM: failed to allocate texture map");
    io.m_fontMap = new (slot.value()) FontMap();
    io.m_fontMap->bindArena(arena);
  }

  return Optional<IOArenaLM>::Some(static_cast<IOArenaLM&&>(io));
}

// The handle's index field gives us the pool slot index directly.
// The generation check is what makes this safe after eviction.

TextureHandle IOArenaLM::requestTexture(const char* _path, usize _pathLen) noexcept
{
  return IOArenaLM::requestImpl<TextureSlot, TextureHandle, LM_MAX_TEXTURES> (
    m_textures, *m_textureMap,
    _path, _pathLen,
    AssetRequestType::Texture)
    .valueOr(TextureHandle{});
}

AudioHandle IOArenaLM::requestAudio(const char* _path, usize _pathLen) noexcept
{
  return IOArenaLM::requestImpl<AudioSlot, AudioHandle, LM_MAX_AUDIO_CLIPS>(
    m_audio, *m_audioMap,
    _path, _pathLen,
    AssetRequestType::Audio)
    .valueOr(AudioHandle{});
}

FontHandle IOArenaLM::requestFont(const char* _path, usize _pathLen) noexcept
{
  return IOArenaLM::requestImpl<FontSlot, FontHandle, LM_MAX_FONTS>(
    m_fonts, *m_fontMap,
    _path, _pathLen,
    AssetRequestType::Font)
    .valueOr(FontHandle{});
}

bool IOArenaLM::addRef(TextureHandle _h) noexcept
{
  TextureSlot* slot = slotFromHandle(m_textures, _h);
  if (!slot) { return false; }
  slot->header.refCount.fetchAddAcquire(1u);
  return true;
}

bool IOArenaLM::addRef(AudioHandle _h) noexcept
{
  AudioSlot* slot = slotFromHandle(m_audio, _h);
  if (!slot) { return false; }
  slot->header.refCount.fetchAddAcquire(1u);
  return true;
}

bool IOArenaLM::addRef(FontHandle _h) noexcept
{
  FontSlot* slot = slotFromHandle(m_fonts, _h);
  if (!slot) { return false; }
  slot->header.refCount.fetchAddAcquire(1u);
  return true;
}

void IOArenaLM::release(TextureHandle _h) noexcept
{
  TextureSlot* slot = slotFromHandle(m_textures, _h);
  if (!slot) { return; }
  u16 prev = slot->header.refCount.fetchSubRelease(1u);
  if (prev == 1u) {
    GC::GCJob job;
    job.type = GC::JobType::EvictTexture;
    job.textureHandle = _h;
    GC::gGCJobQueue.push(job);
  }
}

void IOArenaLM::release(AudioHandle _h) noexcept
{
  AudioSlot* slot = slotFromHandle(m_audio, _h);
  if (!slot) { return; }
  u16 prev = slot->header.refCount.fetchSubRelease(1u);
  if (prev == 1u) {
    GC::GCJob job;
    job.type = GC::JobType::EvictAudio;
    job.audioHandle = _h;
    GC::gGCJobQueue.push(job);
  }
}

void IOArenaLM::release(FontHandle _h) noexcept
{
  FontSlot* slot = slotFromHandle(m_fonts, _h);
  if (!slot) { return; }
  u16 prev = slot->header.refCount.fetchSubRelease(1u);
  if (prev == 1u) {
    GC::GCJob job;
    job.type = GC::JobType::EvictFont;
    job.fontHandle = _h;
    GC::gGCJobQueue.push(job);
  }
}

Optional<TextureAsset*> IOArenaLM::get(TextureHandle _h) noexcept
{
  TextureSlot* slot = slotFromHandle(m_textures, _h);
  if (!slot) { return Optional<TextureAsset*>::None(); }
  if (slot->header.state != AssetState::Ready) {
    return Optional<TextureAsset*>::None();
  }
  return Optional<TextureAsset*>::Some(slot->data);
}

Optional<AudioAsset*> IOArenaLM::get(AudioHandle _h) noexcept
{
  AudioSlot* slot = slotFromHandle(m_audio, _h);
  if (!slot) { return Optional<AudioAsset*>::None(); }
  if (slot->header.state != AssetState::Ready) {
    return Optional<AudioAsset*>::None();
  }
  return Optional<AudioAsset*>::Some(slot->data);
}

Optional<FontAsset*> IOArenaLM::get(FontHandle _h) noexcept
{
  FontSlot* slot = slotFromHandle(m_fonts, _h);
  if (!slot) { return Optional<FontAsset*>::None(); }
  if (slot->header.state != AssetState::Ready) {
    return Optional<FontAsset*>::None();
  }
  return Optional<FontAsset*>::Some(slot->data);
}

Optional<TextureHandle> IOArenaLM::beginTextureLoad(u64 _pathHash) noexcept
{
  return beginLoadImpl<TextureSlot, TextureHandle>(m_textures, _pathHash);
}

Optional<AudioHandle> IOArenaLM::beginAudioLoad(u64 _pathHash) noexcept
{
  return beginLoadImpl<AudioSlot, AudioHandle>(m_audio, _pathHash);
}

Optional<FontHandle> IOArenaLM::beginFontLoad(u64 _pathHash) noexcept
{
  return beginLoadImpl<FontSlot, FontHandle>(m_fonts, _pathHash);
}

void IOArenaLM::publish(TextureHandle _h, u64 _pathHash) noexcept
{
  TextureSlot* slot = slotFromHandle(m_textures, _h);
  if (!slot) { return; }

  // State = Ready must be visible before the map insertion is visible.
  // The map lock provides release/acquire ordering between the Asset
  // Thread's publish and the Logic Thread's requestTexture lookup.
  slot->header.state = AssetState::Ready;

  Sync::MutexGuard g(*m_mapLock);
  m_textureMap->set(_pathHash, _h.index());
}

void IOArenaLM::publish(AudioHandle _h, u64 _pathHash) noexcept
{
  AudioSlot* slot = slotFromHandle(m_audio, _h);
  if (!slot) { return; }
  slot->header.state = AssetState::Ready;

  Sync::MutexGuard g(*m_mapLock);
  m_audioMap->set(_pathHash, _h.index());
}

void IOArenaLM::publish(FontHandle _h, u64 _pathHash) noexcept
{
  FontSlot* slot = slotFromHandle(m_fonts, _h);
  if (!slot) { return; }
  slot->header.state = AssetState::Ready;

  Sync::MutexGuard g(*m_mapLock);
  m_fontMap->set(_pathHash, _h.index());
}

void IOArenaLM::evict(TextureHandle _h) noexcept
{
  TextureSlot* slot = slotFromHandle(m_textures, _h);
  if (!slot) { return; }

  // Abort if someone acquired a new reference since the eviction was triggered.
  // That way the asset stays aliveTextureSlot* slot
  u16 count = slot->header.refCount.loadAcquire();
  if (count != 0u) { return; }

  // Increment generation (stales all handles pointing to this slot).
  // This must happen before the map removal so any concurrent lookup
  // that finds the map entry sees an already-stale generation.
  slot->header.generation++;
  slot->header.state = AssetState::Empty;

  {
    Sync::MutexGuard g(*m_mapLock);
    m_textureMap->remove(slot->header.pathHash);
  }
  m_textures.free(slot);
}

void IOArenaLM::evict(AudioHandle _h) noexcept
{
  AudioSlot* slot = slotFromHandle(m_audio, _h);
  if (!slot) { return; }

  u16 count = slot->header.refCount.loadAcquire();
  if (count != 0u) { return; }
  slot->header.generation++;
  slot->header.state = AssetState::Empty;

  {
    Sync::MutexGuard g(*m_mapLock);
    m_audioMap->remove(slot->header.pathHash);
  }
  m_audio.free(slot);
}

void IOArenaLM::evict(FontHandle _h) noexcept
{
  FontSlot* slot = slotFromHandle(m_fonts, _h);
  if (!slot) { return; }

  u16 count = slot->header.refCount.loadAcquire();
  if (count != 0u) { return; }
  slot->header.generation++;
  slot->header.state = AssetState::Empty;

  {
    Sync::MutexGuard g(*m_mapLock);
    m_fontMap->remove(slot->header.pathHash);
  }
  m_fonts.free(slot);
}