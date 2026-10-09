#include "core/pch.h"
#include "backend.h"

namespace LaMancha {
  namespace Graphics {
    Backend* createBackend() {
#if defined(LAMANCHA_PLATFORM_ANDROID) || defined(LAMANCHA_PLATFORM_R36S)
      return new DRMBackend();
#endif

#if defined(LAMANCHA_PLATFORM_WINDOWS) || defined(LAMANCHA_PLATFORM_LINUX)
      return new GLFWBackend();
#endif
    };

#if defined(LAMANCHA_PLATFORM_R36S) || defined(LAMANCHA_PLATFORM_ANDROID)
    void DRMBackend::pollEvents()
    {
    }

    bool DRMBackend::shouldClose()
    {
      return false;
    }

    void DRMBackend::shutdown()
    {
    }

    void DRMBackend::updateTime()
    {
    }
#endif
  }
}