/**
 * @file backend.h
 * @brief Graphics backend and context handler for LaMancha Engine
 * @author Alonso Moreno <ph0nsy>
 * @date 2026
 * @copyright Copyright (C) 2026 Alonso Moreno
 */

#pragma once

#include "core/time.h"
#include "core/config/generalConfigVars.h"

namespace LaMancha {
  namespace Graphics {
    class Backend {
    protected:
      Time time;

    public:
      Backend() = default;
      virtual ~Backend() = default;

      virtual ResultVoid init(const Config::WindowConfig& _config) = 0;
      virtual void shutdown() = 0;

      virtual void pollEvents() = 0;
      virtual void swapBuffers() = 0;
      virtual bool shouldClose() const = 0;
      virtual void updateTime() = 0;

      Time getTime() const { return time; }
      Time getTotalTime() const { return time.asSeconds(); }
      Time getDeltaTime() const { return time.deltaTime(); }
    };

#if defined(LAMANCHA_PLATFORM_WINDOWS) || defined(LAMANCHA_PLATFORM_LINUX)
#include "deps/GLFW/glfw3.h"

    class GLFWBackend final : public Backend {
    private:
      GLFWwindow* m_window = nullptr; 

    public:

      virtual ResultVoid init(const Config::WindowConfig& _config) override {
        if (!glfwInit()) { return ErrorVoid(ErrorCode::Unknown, "GLFW Init Failed"); }

        m_window = glfwCreateWindow(_config.width, _config.height, _config.title, NULL, NULL);
        if (!m_window) { return ErrorVoid(ErrorCode::Unknown, "Window Creation Failed"); }

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(_config.vsync ? 1 : 0);
        return Ok();
      }

      virtual void pollEvents() override { glfwPollEvents(); }
      virtual void swapBuffers() override { glfwSwapBuffers(static_cast<GLFWwindow*>(this->m_window)); }
      virtual bool shouldClose() const override { return glfwWindowShouldClose(m_window); }
      virtual void shutdown() override { glfwTerminate(); }
      virtual void updateTime() override { time.update(glfwGetTime()); }
    };
#endif

#if defined(LAMANCHA_PLATFORM_R36S) || defined(LAMANCHA_PLATFORM_ANDROID)
#include <EGL/egl.h>
#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

    class DRMBackend final : public Backend {
      i32 fd;
      gbm_device* gbmDevice;
      gbm_surface* gbmSurface;
      EGLDisplay display;
      EGLContext context;
      EGLSurface surface;
      drmModeModeInfo mode;
      u32 connectorId;

    public:
      virtual ResultVoid init(const WindowConfig& config) override {
        // Open the DRM device (usually card0)
        fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
        if (fd < 0) {
          return ErrorVoid(ErrorCode::Unknown, "Could not open DRM device");
        }

        gbmDevice = gbm_create_device(fd);  // Setup GBM

        // Setup EGL on top of GBM
        display = eglGetDisplay((EGLNativeDisplayType)gbmDevice);
        eglInitialize(display, nullptr, nullptr);

        // Standard EGL Config for GLES2/3
        EGLint attributes[] = { EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_RED_SIZE,
                               8,
                               EGL_NONE };
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

        return Ok();
      }

      virtual void swapBuffers() override {
        eglSwapBuffers(display, surface);  // On DRM, also handle the page flip here
      }

      virtual void pollEvents() override;
      virtual bool shouldClose() override;
      virtual void shutdown() override;
      virtual void updateTime() override;
    };

#endif

    // Factory to return the correct implementation
    Backend* createBackend();
  } 
}
