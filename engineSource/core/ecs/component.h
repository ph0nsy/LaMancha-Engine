/**
 * @file component.h
 * @brief Basic component-related definitions for LaMancha Engine's ECS
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 *
 * Each entity has a bitmask indicating which component types it owns that lets
 * the engine query via bitwise AND between entity masks and the query mask
 */

#pragma once
#include "core/pch.h"

namespace LaMancha {
  namespace ECS {
    constexpr usize MAX_COMPONENT_TYPES = 256;   ///< maximum number of distinct component types the engine supports
    using ComponentID = u32;                     ///< index of a component type in the component arrays

    /**
     * @brief ComponentType<T> is a template that associates a C++ type with a runtime ID
     *
     * The static member `id` must be defined (and assigned a unique value) once
     * per component type, typically in a .cpp file or via a registration system.
     *
     * @tparam T value type of ComponentID
     *
     * Example:
     * @code
     * ComponentType<TransformComponent>::id = 0;
     * ComponentType<RenderComponent>::id = 1;
     * @endcode
     */
    class ComponentRegistry {
    public:
      static ComponentID next() {
        static ComponentID counter = 0;
        return counter++;
      }
    };

    /**
     * @brief ComponentType<T> is a template that associates a C++ type with a runtime ID
     *
     * The static member `id` must be defined (and assigned a unique value) once
     * per component type, typically in a .cpp file or via a registration system.
     *
     * @tparam T value type of ComponentID
     *
     * Example:
     * @code
     * ComponentType<TransformComponent>::id = 0;
     * ComponentType<RenderComponent>::id = 1;
     * @endcode
     */
    template <typename T>
    struct ComponentType {
      static ComponentID id;
    };

    //TODO: using ComponentMask = BitMask<MAX_COMPONENT_TYPES>; ///< Used to access the ComponentIDs of a Entity

    struct AIDomain {};
    struct CoreDomain {};
    struct GameplayDomain {};
    struct PhysicsDomain {};
    struct UIDomain {};
    struct NullDomain {}; ///< For raw Component if ever queried directly

    template<typename Domain>
    struct Component {};

    using AIComponent = Component<AIDomain>;
    using CoreComponent = Component<CoreDomain>;
    using GameplayComponent = Component<GameplayDomain>;
    using PhysicsComponent = Component<PhysicsDomain>;
    using UIComponent = Component<UIDomain>;
  }
}
