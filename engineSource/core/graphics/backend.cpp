#include "IBackend.h"

#if defined(LAMANCHA_PLATFORM_R36S)
#include "drmBackend.h"
#else
#include "glfwBackend.h"
#endif

namespace LaMancha {
namespace Graphics {
    
IBackend* createBackend() {
#if defined(LAMANCHA_PLATFORM_R36S)
  return new DRMBackend();
#else
  return new GLFWBackend();
#endif
};

}  // namespace Graphics
}  // namespace LaMancha