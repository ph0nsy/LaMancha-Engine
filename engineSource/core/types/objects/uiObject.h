/**
 * @file UIObject.h
 * @brief UI base classes for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * @details
 * UI types use @c SessionLifetime, they persist across level transitions
 * and are reclaimed only at session end (game exit or full reset).
 *
 * - Ownership.
 *   - @a LogicThread: creates UI objects, drives UI logic (show/hide, value
 *     updates, input response). UI state is owned here.
 *   - @a MainThread: reads UI render state via the frame snapshot @c SyncPoint,
 *     builds render commands for UI elements.
 *
 * - Cross-thread access.
 *   Fields that the @a MainThread reads for rendering should be declared as
 *   @c Atomic<T> so the @a LogicThread can write them safely.
 *
 * @code
 *   class HealthBar : public UIWidget {
 *   public:
 *       void setValue(f32 v) { m_value.storeRelease(v); }
 *       f32  getValue() const { return m_value.loadAcquire(); }
 *   private:
 *       Sync::Atomic<f32> m_value { 1.0f };
 *   };
 * @endcode
 *
 * @code
 *   class HUD : public UICanvas {
 *   public:
 *       HUD();
 *   private:
 *       HealthBar* m_healthBar;
 *       HealthBar* m_bossBar;
 *   };
 *
 *   // Logic Thread:
 *   HUD* hud = new HUD();   // SessionArena
 * @endcode
 */

#pragma once

#include "LaManchaObject.h"
#include "core/memory/lifetimes.h"

namespace LaMancha {

  /**
   * @brief Base for root UI containers.
   *
   * A @c UICanvas is the root of a UI hierarchy: a HUD, a pause menu, a
   * dialogue box, an inventory screen. Canvas persists across level
   * transitions so the HUD does not need to be recreated on every load.
   *
   * A canvas owns its child widgets. It is responsible for laying them
   * out, routing input to them, and showing/hiding the group as a whole.
   *
   * @note Use for: HUD root, pause menu, main menu, dialogue system root,
   * inventory screen, map screen.
   */
  struct UICanvas : public LaManchaObject<SessionLifetime> {
    virtual ~UICanvas() noexcept = default;
  protected:
    UICanvas() noexcept = default;
  };

  /**
   * @brief Base for individual UI elements.
   *
   * @c UIWidgets are the leaves of the UI hierarchy: a health bar, a button,
   * a label, an icon, a panel. They are owned by a @c UICanvas or another
   * @c UIWidget (for nested layouts).
   *
   * Render facing state (position, size, color, opacity) should be stored
   * as @c Atomic<T> fields so the @a MainThread can read them safely while the
   * @a LogicThread updates them.
   *
   * @note Use for: health bars, stamina bars, buttons, labels, icons, text boxes,
   * progress bars, inventory slots, dialogue portraits.
   */
  struct UIWidget : public LaManchaObject<SessionLifetime> {
    virtual ~UIWidget() noexcept = default;
  protected:
    UIWidget() noexcept = default;
  };

}