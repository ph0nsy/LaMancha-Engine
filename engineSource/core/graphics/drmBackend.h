#include <EGL/egl.h>
#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "IBackend.h"

namespace LaMancha {
namespace Graphics {
    
class DRMBackend : public IBackend {
  int fd;
  gbm_device* gbmDevice;
  gbm_surface* gbmSurface;
  EGLDisplay display;
  EGLContext context;
  EGLSurface surface;
  drmModeModeInfo mode;
  u32 connectorId;

 public:
  virtual Result<bool> init(const WindowConfig& config) override {
    // Open the DRM device (usually card0)
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      return Result<bool>::Fail("Could not open DRM device");
    }

    gbmDevice = gbm_create_device(fd);  // Setup GBM

    // Setup EGL on top of GBM
    display = eglGetDisplay((EGLNativeDisplayType)gbmDevice);
    eglInitialize(display, nullptr, nullptr);

    // Standard EGL Config for GLES2/3
    EGLint attributes[] = {EGL_SURFACE_TYPE,
                           EGL_WINDOW_BIT,
                           EGL_RENDERABLE_TYPE,
                           EGL_OPENGL_ES2_BIT,
                           EGL_BLUE_SIZE,
                           8,
                           EGL_GREEN_SIZE,
                           8,
                           EGL_RED_SIZE,
                           8,
                           EGL_NONE};
    EGLConfig eglConfig;
    EGLint numConfigs;
    eglChooseConfig(display, attributes, &eglConfig, 1, &numConfigs);

    // Create the surface linked to the handheld screen
    // In a full implementation, you'd iterate connectors here to find the LCD
    gbmSurface = gbm_surface_create(gbmDevice, config.width, config.height,
                                    GBM_FORMAT_XRGB8888,
                                    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    surface = eglCreateWindowSurface(display, eglConfig,
                                     (EGLNativeWindowType)gbmSurface, nullptr);

    context = eglCreateContext(display, eglConfig, EGL_NO_CONTEXT, nullptr);
    eglMakeCurrent(display, surface, surface, context);

    return Result<bool>::Ok(true);
  }

  virtual void swapBuffers() override {
    eglSwapBuffers(display, surface);  // On DRM, also handle the page flip here
  }

  // virtual void pollEvents() override;
  // virtual bool shouldClose() override;
  // virtual void shutdown() override;
};

}  // namespace Graphics
}  // namespace LaMancha