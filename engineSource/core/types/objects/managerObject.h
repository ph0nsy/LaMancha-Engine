/**
 * @file Manager.h
 * @brief Engine singleton base classes for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * All types in this file are @c PermanentLifetime, created once at engine
 * startup on the @a MainThread and never destroyed until process exit. Each 
 * engine subsystem exposes a Manager interface. User code creates a concrete 
 * implementation that inherits from the appropriate base and registers it 
 * with the engine. The engine owns the pointer (in the @c PermanentArena ) 
 * for the session lifetime.
 *
 * - Ownership. 
 * Main Thread creates and owns all managers. Other threads receive non-owning
 * pointers for read access or for posting requests via message queues.
 *
 * @code
 *   class MyAudioManager : public AudioManager {
 *   public:
 *       MyAudioManager();
 *       void play(SoundHandle handle, Vec3 position) override;
 *       void stop(SoundHandle handle) override;
 *   };
 *
 *   // main() (after PermanentLifetime::bind())
 *   AudioManager* audio = new MyAudioManager();   // PermanentArena
 * @endcode
 */

#pragma once

#include "LaManchaObject.h"
#include "core/memory/lifetimes.h"

namespace LaMancha {

  /**
   * @brief Base for all engine singleton managers.
   *
   * @details
   * A Manager is a subsystem controller that lives for the entire engine
   * session. Managers are created once at startup, stored in the @c PermanentArena ,
   * and accessed through non-owning pointers by other systems.
   *
   * Managers must not hold level-lifetime data, anything that needs to
   * reset between levels belongs in the level's ECS or LevelArena.
   *
   * @note Use for: any engine subsystem that needs a persistent controller.
   */
  struct Manager : public LaManchaObject<PermanentLifetime> {
    virtual ~Manager() noexcept = default;
  protected:
    Manager() noexcept = default;
  };

  /**
   * @brief Base for the audio subsystem manager.
   *
   * @details
   * Manages audio device initialization, sound bank loading, bus routing,
   * and global audio state. Audio source playback is triggered by the
   * @a LogicThread via the @c AudioManager interface. Actual mixing runs on
   * the @a AssetThread.
   *
   * @note Use for: the engine's single concrete @c AudioManager implementation.
   */
  struct AudioManager : public Manager {
    virtual ~AudioManager() noexcept = default;
  protected:
    AudioManager() noexcept = default;
  };

  /**
   * @brief Base for the scene and level transition manager.
   *
   * Manages the lifecycle of levels: loading, unloading, streaming, and
   * transitions. Coordinates the multi-thread hand off during level changes:
   * @a LogicThread standbys, @a LevelArena resets, @a AssetThread loads new data,
   * @a LogicThread resumes with the new level.
   *
   * @note Use for: the engine's single concrete SceneManager implementation.
   */
  struct SceneManager : public Manager {
    virtual ~SceneManager() noexcept = default;
  protected:
    SceneManager() noexcept = default;
  };

  /**
   * @brief Base for input management.
   *
   * @details
   * Wraps OS input polling (SDL2 events, evdev) and exposes a clean,
   * frame coherent @c InputState to the @a LogicThread. The @a MainThread writes
   * @c InputState each frame; the Logic Thread reads it at the frame boundary.
   *
   * Use for: the engine's single concrete InputManager implementation.
   */
  struct InputManager : public Manager {
    virtual ~InputManager() noexcept = default;
  protected:
    InputManager() noexcept = default;
  };

  /**
   * @brief Base for render pipeline management.
   *
   * @details
   * Manages OpenGL state, shader programs, framebuffers, and the render
   * command queue. Lives on the @a MainThread which owns the OpenGL context.
   *
   * @note Use for: the engine's single concrete @c RenderManager implementation.
   */
  struct RenderManager : public Manager {
    virtual ~RenderManager() noexcept = default;
  protected:
    RenderManager() noexcept = default;
  };

}