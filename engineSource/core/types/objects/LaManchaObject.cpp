#include "core/pch.h"
#include "LaManchaObject.h"
#include "core/memory/lifetimes.h"

namespace LaMancha {

  // All lifetime tag static members start as nullptr / unknown value.
  // They are assigned during engine initialisation via `Lifetime::bind()`.
  //
  // Expected startup sequence:
  //   1. Allocators constructed(ArenaLM instances, HeapAllocatorLM)
  //   2. Logging initialized > (`Log_Init()`)
  //   3. Lifetime tags bound > (`Lifetime::bind()` calls below)
  //   4. Threads started
  //   5. First LaManchaObject may now be created
  // 
  // Missing a `bind()` call produces an immediate assert:
  // "LaManchaObject::operator new: lifetime arena not bound"

  LevelArenaLM* LevelLifetime::arena = nullptr;
  CoreAffinity LevelLifetime::ownerThread = CoreAffinity::Unknown;

  ArenaLM* SessionLifetime::arena = nullptr;
  CoreAffinity SessionLifetime::ownerThread = CoreAffinity::Unknown;

  PermanentArenaLM* PermanentLifetime::arena = nullptr;
  CoreAffinity    PermanentLifetime::ownerThread = CoreAffinity::Unknown;

  ArenaLM* AssetLifetime::arena = nullptr;
  CoreAffinity AssetLifetime::ownerThread = CoreAffinity::Unknown;
}