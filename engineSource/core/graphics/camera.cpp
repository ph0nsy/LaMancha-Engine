#include "core/pch.h"
#include "camera.h"

namespace LaMancha {
  Result<Math::mat4_4> Graphics::Camera::getViewProjection() const
  {
    return Ok(Math::mat4_4());
  }

  Result<Math::mat4_4> Graphics::Camera::getView() const
  {
    return Ok(Math::mat4_4());
  }

  Result<Math::mat4_4> Graphics::Camera::getProjection() const
  {
    return Ok(Math::mat4_4());
  }
}