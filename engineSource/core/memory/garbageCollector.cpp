#include "core/pch.h"
#include "garbageCollector.h"

using namespace LaMancha;

DataStructures::AtomicRingBufferLM<GC::GCJob, 256> GC::gGCJobQueue;
DataStructures::AtomicRingBufferLM<AssetRequest, 64> GC::gAssetRequestQueue;


void GC::ProcessJobsGC(IOArenaLM& _io, LevelArenaLM* levelArena/*, lua_State* luaState*/)
{
  GCJob job;
  while (gGCJobQueue.pop(job)) {
    switch (job.type) {

    case GC::JobType::EvictTexture:
      _io.evict(job.textureHandle);
      Sync::gTelemetry.assets_evicted.fetchAddRelaxed(1u);
      break;

    case GC::JobType::EvictAudio:
      _io.evict(job.audioHandle);
      Sync::gTelemetry.assets_evicted.fetchAddRelaxed(1u);
      break;

    case GC::JobType::EvictFont:
      _io.evict(job.fontHandle);
      Sync::gTelemetry.assets_evicted.fetchAddRelaxed(1u);
      break;

    case GC::JobType::ResetLevel:
      if (levelArena) { levelArena->unload(); }
      break;

    case GC::JobType::DeleteGLObj:
      // GL calls must happen on the Main Thread.
      // Re-queue to the renderer's GL delete queue.
      // TODO: push job.glHandle to gGLDeleteQueue once renderer exists.
      // The GLuint remains valid until the Main Thread processes the delete.
      break;

    case GC::JobType::LuaGCStep:
      // Incremental Lua GC, runs a fixed number of steps per job
      // so the Utility Thread does not stall on a large collection.
      // TODO: wire luaState once Lua integration is complete.

      /*if (luaState) { lua_gc(luaState, LUA_GCSTEP, static_cast<int>(job.luaSteps)); }*/
      break;
    }

    Sync::gTelemetry.gc_jobs_processed.fetchAddRelaxed(1u);
  }
}

void LaMancha::GC::ProcessAssetRequests(IOArenaLM& _io)
{
  AssetRequest req;
  while (gAssetRequestQueue.pop(req)) {
    switch (req.type) {
    case AssetRequestType::Texture:
      // TODO: call loadTexture(io, req.path, req.pathHash) once
      // the asset loading subsystem exists.
      break;
    case AssetRequestType::Audio:
      // TODO: call loadAudio(io, req.path, req.pathHash)
      break;
    case AssetRequestType::Font:
      // TODO: call loadFont(io, req.path, req.pathHash)
      break;
    }
  }
}
