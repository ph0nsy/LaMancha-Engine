#pragma once
#include "../typesLM.h"

namespace LaMancha {
namespace Graphics {
    
struct WindowConfig {
  const char* title;
  u32 width, height;
  bool fullscreen;
  bool vsync;
};

class IBackend {
 public:
  virtual ~IBackend() = default;

  virtual Result<bool> init(const WindowConfig& config) = 0;
  virtual void shutdown() = 0;

  virtual void pollEvents() = 0;
  virtual void swapBuffers() = 0;
  virtual bool shouldClose() const = 0;

  virtual Time getTime() const = 0;
};

// Factory to return the correct implementation
IBackend* createBackend();

}  // namespace Graphics
}  // namespace LaMancha