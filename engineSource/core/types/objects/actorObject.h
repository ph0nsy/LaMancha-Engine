/**
 * @file actor.h
 * @brief Game object base classes for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * All types in this file are LevelLifetime. They live exactly as long
 * as the current level and are reclaimed when @c LevelArena resets on unload.
 *
 * - Ownership. 
 * @a LogicThread creates and owns all types in this file.
 * 
 * - Rendering. 
 * @a MainThread reads their state via the frame snapshot @c SyncPoint.
 * 
 * - Simulation. 
 * @a AssetThread handles VFX and audio mixing.
 *
 * @code
 *   class Enemy : public Actor {
 *   public:
 *       Enemy(Vec3 startPos, f32 health);
 *       void update(f32 dt);
 *   private:
 *       Vec3 m_position;
 *       f32  m_health;
 *   };
 *
 *   // Logic Thread:
 *   Enemy* e = new Enemy(spawnPos, 100.0f);   // LevelArena destructor runs,
 *   delete e;                                 // slot held until level unloads
 * @endcode
 */

#pragma once

#include "LaManchaObject.h"
#include "core/memory/lifetimes.h"

namespace LaMancha {

  /**
   * @brief Base for all game entities.
   *
   * @details
   * An Actor represents a game object with presence in the world: an enemy,
   * a player, an NPC, an interactive prop, a trigger volume. Actors typically
   * hold an EntityHandle to access their ECS components.
   *
   * @note Use for: enemies, players, NPCs, props, triggers, pickups.
   */
  struct Actor : public LaManchaObject<LevelLifetime> {
    virtual ~Actor() noexcept = default;
  protected:
    Actor() noexcept = default;
  };

  /**
   * @brief Base for user-defined ECS components with non-trivial constructors.
   *
   * @details
   * Engine-internal component types (TransformComponent, VelocityComponent)
   * are plain data structs managed by TypedPool directly. This base is for user 
   * components that require a constructor or destructor (for example, a component 
   * that registers with an external system).
   *
   * @note For plain data components, define a plain struct and use TypedPool.
   * @note Use for: user-defined components that own resources or need construction.
   */
  struct Component : public LaManchaObject<LevelLifetime> {
    virtual ~Component() noexcept = default;
  protected:
    Component() noexcept = default;
  };

  /**
   * @brief Base for behavior objects attached to components or actors.
   *
   * @details
   * ComponentLogic encapsulates rules and decisions that operate on component
   * data without being component data themselves. Controllers, state machines,
   * decision trees.
   *
   * @note Use for: AIController, PlayerController, AnimationController,
   * StateMachine, CombatController.
   */
  struct ComponentLogic : public LaManchaObject<LevelLifetime> {
    virtual ~ComponentLogic() noexcept = default;
  protected:
    ComponentLogic() noexcept = default;
  };

  /**
   * @brief Base for particle and visual effect emitters.
   *
   * @details
   * VFXEmitters are created and configured by the Logic Thread. Their particle
   * simulation runs on the Asset Thread during its idle time. The Main Thread
   * reads the resulting particle state for rendering.
   *
   * @note Use for: explosions, ambient particles, trail effects, impact sparks,
   * screen-space VFX.
   */
  struct VFXEmitter : public LaManchaObject<LevelLifetime> {
    virtual ~VFXEmitter() noexcept = default;
  protected:
    VFXEmitter() noexcept = default;
  };

  /**
   * @brief Base for world-space audio emitters.
   *
   * @details
   * AudioSources are positioned in the world and triggered by game events
   * on the Logic Thread. Audio decoding and mixing runs on the Asset Thread.
   *
   * @note Use for: footsteps, environmental sounds, character voices,
   * weapon sounds, ambient emitters.
   */
  struct AudioSource : public LaManchaObject<LevelLifetime> {
    virtual ~AudioSource() noexcept = default;
  protected:
    AudioSource() noexcept = default;
  };

  /**
   * @brief Base for Lua-backed behaviour objects.
   *
   * @details
   * Scripts wrap Lua closures or Lua objects and provide a C++ interface
   * to script-driven behavior. The Lua VM runs on the Logic Thread.
   *
   * @note Use for: cutscene controllers, dialogue trees, puzzle logic,
   * scripted events, tutorial triggers.
   */
  struct Script : public LaManchaObject<LevelLifetime> {
    virtual ~Script() noexcept = default;
  protected:
    Script() noexcept = default;
  };

}