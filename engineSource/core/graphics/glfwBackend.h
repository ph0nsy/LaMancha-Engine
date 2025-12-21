#include <GLFW/glfw3.h>

#include "IBackend.h"

namespace LaMancha {
namespace Graphics {

class GLFWBackend : public IBackend {
 private:
  GLFWwindow* m_window = nullptr;

 public:
  virtual Result<bool> init(const WindowConfig& config) override {
    if (!glfwInit()) return Result<bool>::Fail("GLFW Init Failed");

    m_window =
        glfwCreateWindow(config.width, config.height, config.title, NULL, NULL);
    if (!m_window) return Result<bool>::Fail("Window Creation Failed");

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(config.vsync ? 1 : 0);
    return Result<bool>::Ok(true);
  }

  virtual void pollEvents() override { glfwPollEvents(); }
  virtual void swapBuffers() override { glfwSwapBuffers(m_window); }
  virtual bool shouldClose() const override {
    return glfwWindowShouldClose(m_window);
  }
  virtual void shutdown() override { glfwTerminate(); }
  // virtual Time getTime() const override { return glfwGetTime(); // this
  // returns f32 }
};

}  // namespace Graphics
}  // namespace LaMancha