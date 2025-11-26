#include <webgpu/webgpu.h>

struct GLFWwindow;


class Application
{
public:
    // Initialize everything and return true if it went all right
    bool Initialize();

    // Uninitialize everything that was initialized
    void Terminate();

    // Draw a frame and handle events
    void MainLoop();

    // Return true as long as the main loop should keep on running
    bool IsRunning();

private:
    GLFWwindow* m_window = nullptr;
    WGPUInstance m_instance = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;
    WGPUSurface m_surface = nullptr;

};