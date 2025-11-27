// In Application.cpp
#include "Application.h"
#include "webgpu-utils.h"

#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
// In Application.cpp
#include <glfw3webgpu.h>


const char* shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
	if (in_vertex_index == 0u) {
		return vec4f(-0.45, 0.5, 0.0, 1.0);
	} else if (in_vertex_index == 1u) {
		return vec4f(0.45, 0.5, 0.0, 1.0);
	} else {
		return vec4f(0.0, -0.5, 0.0, 1.0);
	}
}
// Add this in the same shaderSource literal than the vertex entry point
@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(0.0, 0.4, 0.7, 1.0);
}
)";

bool Application::Initialize()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // <-- extra info for glfwCreateWindow
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

	// Instance setup
	m_instance = wgpuCreateInstance(nullptr);

	// Adapter setup
	m_surface = glfwCreateWindowWGPUSurface(m_instance, m_window);
	WGPUAdapter adapter = SetupAdapter();
	//SetAdapterLimits(adapter);
	//InspectAdapter(adapter);

	// Device setup
	SetupDevice(adapter);
	// Command queue
	m_queue = wgpuDeviceGetQueue(m_device);

	// Surface setup
	SetupSurfaceConfig(adapter);

	// We no longer need to access the adapter once we have the device
	wgpuAdapterRelease(adapter);

	// At the end of Initialize()
	if (!InitializePipeline()) return false;

	return true;
}

void Application::Terminate()
{
	wgpuSurfaceUnconfigure(m_surface);
	wgpuQueueRelease(m_queue);
	wgpuSurfaceRelease(m_surface);
	wgpuDeviceRelease(m_device);
	glfwDestroyWindow(m_window);
	glfwTerminate();
	wgpuRenderPipelineRelease(m_pipeline);
}



bool Application::IsRunning()
{
	return !glfwWindowShouldClose(m_window);
}



WGPUTextureView Application::GetNextSurfaceView()
{
	WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
	wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);
	if (
		surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
		surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal
		) {
		return nullptr;
	}
	WGPUTextureViewDescriptor viewDescriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
	viewDescriptor.label = toWgpuStringView("Surface texture view");
	viewDescriptor.dimension = WGPUTextureViewDimension_2D; // not to confuse with 2DArray
	WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);
	// We no longer need the texture, only its view,
	// so we release it at the end of GetNextSurfaceViewData
	wgpuTextureRelease(surfaceTexture.texture);
	return targetView;

}



void Application::RenderPassEncoder(const WGPUTextureView& targetView)
{
	// - encoder descriptor
	WGPUCommandEncoderDescriptor encoderDesc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
	encoderDesc.label = toWgpuStringView("My command encoder");
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &encoderDesc);

	// renderpass descriptor
	WGPURenderPassDescriptor renderPassDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;

	// color attachment
	WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
	colorAttachment.view = targetView;
	colorAttachment.loadOp = WGPULoadOp_Clear;
	colorAttachment.storeOp = WGPUStoreOp_Store;
	colorAttachment.clearValue = WGPUColor{ 0, 0, 0, 1.0 };
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &colorAttachment;

	// render pass encoder
	WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
	wgpuRenderPassEncoderSetPipeline(renderPass, m_pipeline);
	// Draw 1 instance of a 3-vertices shape
	wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);

	wgpuRenderPassEncoderEnd(renderPass);
	wgpuRenderPassEncoderRelease(renderPass);

	// - command buffer descriptor
	WGPUCommandBufferDescriptor cmdBufferDescriptor = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
	cmdBufferDescriptor.label = toWgpuStringView("Command buffer");
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
	wgpuCommandEncoderRelease(encoder);

	// Submit the command queue
	std::cout << "Submitting command..." << std::endl;
	wgpuQueueSubmit(m_queue, 1, &command);
	wgpuCommandBufferRelease(command);
	std::cout << "Command submitted." << std::endl;


}

void Application::MainLoop()
{
	glfwPollEvents();
	wgpuInstanceProcessEvents(m_instance);

	// Get the next target texture view
	WGPUTextureView targetView = GetNextSurfaceView();
	if (!targetView) return;


	RenderPassEncoder(targetView);


	// At the end of the frame
	wgpuTextureViewRelease(targetView);
#ifndef __EMSCRIPTEN__
	wgpuSurfacePresent(m_surface);
#endif

}


#pragma region SurfaceConfig
bool Application::SetupSurfaceConfig(const WGPUAdapter& adapter)
{
	WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
	// Texture parameters
	config.width = 640;
	config.height = 480;
	config.device = m_device;
	WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;

	WGPUStatus status = wgpuSurfaceGetCapabilities(m_surface, adapter, &capabilities);
	if (status != WGPUStatus_Success)
	{
		return false;
	}

	// From the capabilities, we get the preferred format: it is always the first one!
	// (NB: There is always at least 1 format if the GetCapabilities was successful)
	config.format = capabilities.formats[0];

	// We no longer need to access the capabilities, so we release their memory.
	wgpuSurfaceCapabilitiesFreeMembers(capabilities);
	config.presentMode = WGPUPresentMode_Fifo;
	config.alphaMode = WGPUCompositeAlphaMode_Auto;

	wgpuSurfaceConfigure(m_surface, &config);

	m_surfaceFormat = config.format;

	return true;


}

void Application::SetupDevice(const WGPUAdapter& adapter)
{
	std::cout << "Requesting device..." << std::endl;

	WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;
	deviceDesc.label = toWgpuStringView("My Device");
	std::vector<WGPUFeatureName> features;
	deviceDesc.requiredFeatureCount = features.size();
	deviceDesc.requiredFeatures = features.data();
	WGPULimits requiredLimits = WGPU_LIMITS_INIT;
	deviceDesc.requiredLimits = &requiredLimits;
	deviceDesc.defaultQueue.label = toWgpuStringView("The Default Queue");

	auto onDeviceLost = [](
		WGPUDevice const* device,
		WGPUDeviceLostReason reason,
		struct WGPUStringView message,
		void* /* userdata1 */,
		void* /* userdata2 */
		) {
			// All we do is display a message when the device is lost
			std::cout
				<< "Device " << device << " was lost: reason " << reason
				<< " (" << toStdStringView(message) << ")"
				<< std::endl;
		};
	deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
	deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	auto onDeviceError = [](
		WGPUDevice const* device,
		WGPUErrorType type,
		struct WGPUStringView message,
		void* /* userdata1 */,
		void* /* userdata2 */
		) {
			std::cout
				<< "Uncaptured error in device " << device << ": type " << type
				<< " (" << toStdStringView(message) << ")"
				<< std::endl;
		};
	deviceDesc.uncapturedErrorCallbackInfo.callback = onDeviceError;
	// NB: 'device' is now declared at the class level
	m_device = requestDeviceSync(m_instance, adapter, &deviceDesc);
	std::cout << "Got device: " << m_device << std::endl;


}


WGPUAdapter Application::SetupAdapter()
{
	std::cout << "Requesting adapter..." << std::endl;

	WGPURequestAdapterOptions adapterOpts = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
	adapterOpts.compatibleSurface = m_surface;
	WGPUAdapter adapter = requestAdapterSync(m_instance, &adapterOpts);

	std::cout << "Got adapter: " << adapter << std::endl;


	return adapter;

}

bool Application::InitializePipeline()
{
	WGPUShaderSourceWGSL wgslDesc = WGPU_SHADER_SOURCE_WGSL_INIT;
	wgslDesc.code = toWgpuStringView(shaderSource);
	WGPUShaderModuleDescriptor shaderDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
	shaderDesc.nextInChain = &wgslDesc.chain; // connect the chained extension
	shaderDesc.label = toWgpuStringView("Shader source from Application.cpp");
	WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(m_device, &shaderDesc);

	// render pipeline descriptor
	WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = toWgpuStringView("vs_main");

	// fragment state descriptor
	WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;


	fragmentState.module = shaderModule;
	fragmentState.entryPoint = toWgpuStringView("fs_main");

	// color targets
	WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
	colorTarget.format = m_surfaceFormat;

	// fragment blending
	WGPUBlendState blendState = WGPU_BLEND_STATE_INIT;
	colorTarget.blend = &blendState;
	fragmentState.targetCount = 1;

	fragmentState.targets = &colorTarget;
	pipelineDesc.fragment = &fragmentState;



	m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);
	wgpuShaderModuleRelease(shaderModule);

	return true;
}
