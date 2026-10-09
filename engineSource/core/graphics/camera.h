/**
 * @file camera.h
 * @brief Camera handler (Projection and View) for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#include "core/ecs/components/transform.h"

namespace LaMancha {
  namespace Graphics {
    enum class Projection {
      Orthographic,   ///< Parallel projection (no perspective divide effect). Used for 2D or isometric.
      Perspective     ///< Frustum projection (Z affects apparent size). Used for 3D.
    };

    struct PerspectiveParams {
      f32 fovX = 0.f;       ///< Vertical field of view in radians (perspective only).
      f32 fovY = 0.f;       ///< Vertical field of view in radians (perspective only).
      f32 aspect = 0.f;     ///< Viewport width / height.
      f32 nearPlane = 0.f;  ///< Near clip distance.
      f32 farPlane = 0.f;   ///< Far clip distance.
    };

    /**
     * @brief Represents a viewpoint in the scene. Owns the Projection and View matrices.
     *
     * The camera is the sole owner of the P*V matrix. It does not know about individual
     * objects. It only knows about the viewer's position, orientation, and projection mode.
     *
     * Consumers (renderers, shader systems) call getViewProjection() and combine it with
     * each object's model matrix: finalMatrix = camera.getViewProjection() * model.
     */
    struct Camera {
      ECS::Transform transform; ///< Camera position and orientation in world space.
      Projection mode;          ///< Which projection to build.

      f32 orthoSize = 0.f;            ///< Half-height of the orthographic view volume. Width = orthoSize * aspect.
      PerspectiveParams data;

      /**
       * @brief Builds and returns the combined P·V matrix.
       *
       * Projection is rebuilt from the camera parameters each call.
       * View is the inverse of the camera's model matrix (it transforms
       * world space into camera space).
       *
       * For a Transform with no scale, the view matrix inverse is just:
       *
       * @code
       *   V = transpose(R) with translation = -transpose(R) * position
       * @endcode
       */
      Result<Math::mat4_4> getViewProjection() const;

      /**
       * @brief Returns the View matrix alone (world > camera space).
       */
      Result<Math::mat4_4> getView() const;

      /**
       * @brief Returns the Projection matrix alone (camera > clip space).
       */
      Result<Math::mat4_4> getProjection() const;
    };
  }
}