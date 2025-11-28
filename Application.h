#include <webgpu/webgpu.h>
#include <array>
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

    bool SetupSurfaceConfig(const WGPUAdapter& adapter);

    // Return true as long as the main loop should keep on running
    bool IsRunning();


private:
    WGPUTextureView GetNextSurfaceView();
    void RenderPassEncoder(const WGPUTextureView& targetView);
    void SetupDevice(const WGPUAdapter& adapter);
    WGPUAdapter SetupAdapter();
    bool InitializePipeline();
    bool InitializeBuffers();
    void InitializeBindGroups();

private:
    GLFWwindow* m_window = nullptr;

    WGPUInstance m_instance = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;

    WGPUSurface m_surface = nullptr;
    WGPURenderPipeline m_pipeline = nullptr;
    WGPUTextureFormat m_surfaceFormat = WGPUTextureFormat_Undefined;

    WGPUBuffer m_vertexBuffer = nullptr;
    uint32_t m_vertexCount = 0;

    WGPUBuffer m_pointBuffer = nullptr;
    WGPUBuffer m_indexBuffer = nullptr;
    uint32_t m_indexCount = 0;

    WGPUBuffer m_uniformBuffer = nullptr;

    WGPUPipelineLayout m_layout = nullptr;
    WGPUBindGroupLayout m_bindGroupLayout = nullptr;
    WGPUBindGroup m_bindGroup = nullptr;

    uint32_t m_uniformStride = 0;


private:
    struct MyUniforms
    {
        std::array<float, 4> color;  // or float color[4]
        float time;
        // align on a multiple of 16, as the total size must be a multiple of the alignment size of its largest field.
        // this padding makes it 32
        float _pad[3];
    };

    static_assert(sizeof(MyUniforms) % 16 == 0);



};

