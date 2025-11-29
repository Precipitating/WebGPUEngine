// In Application.cpp
#include "Application.h"
#include "webgpu-utils.h"

#include <GLFW/glfw3.h>
#include <cassert>
#include <iostream>
#include <vector>
#include "ResourceManager.h"
// In Application.cpp
#include <glfw3webgpu.h>

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
	if (!InitializeBuffers()) return false;

	InitializeBindGroups();
	return true;
}

void Application::Terminate()
{
	wgpuBufferRelease(m_pointBuffer);
	wgpuBufferRelease(m_indexBuffer);
	wgpuBufferRelease(m_vertexBuffer);
	wgpuRenderPipelineRelease(m_pipeline);
	wgpuSurfaceUnconfigure(m_surface);
	wgpuQueueRelease(m_queue);
	wgpuSurfaceRelease(m_surface);
	wgpuDeviceRelease(m_device);
	wgpuBufferRelease(m_uniformBuffer);
	wgpuPipelineLayoutRelease(m_layout);
	wgpuBindGroupLayoutRelease(m_bindGroupLayout);
	glfwDestroyWindow(m_window);
	glfwTerminate();

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
	// Set vertex buffers while encoding the render pass
	wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_pointBuffer, 0, wgpuBufferGetSize(m_pointBuffer));
	wgpuRenderPassEncoderSetIndexBuffer(renderPass, m_indexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(m_indexBuffer));

	wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_bindGroup, 0, nullptr);

	// Replace `draw()` with `drawIndexed()` and `m_vertexCount` with `m_indexCount`
	// The extra argument is an offset within the index buffer.
	wgpuRenderPassEncoderDrawIndexed(renderPass, m_indexCount, 1, 0, 0, 0);

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

	float time = static_cast<float>(glfwGetTime());
	// Only update the 1-st float of the buffer
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, offsetof(MyUniforms, time), &time, sizeof(time));

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
	// In Initialize() or in a dedicated InitializePipeline()
	WGPUShaderSourceWGSL wgslDesc = WGPU_SHADER_SOURCE_WGSL_INIT;
	std::cout << "Creating shader module..." << std::endl;
	WGPUShaderModule shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", m_device);
	std::cout << "Shader module: " << shaderModule << std::endl;
	if (shaderModule == nullptr) return false;


	WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
	// Vertex fetch
	WGPUVertexBufferLayout vertexBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
	// We now have 2 attributes
	std::vector<WGPUVertexAttribute> vertexAttribs(2);

	// Describe the position attribute
	vertexAttribs[0].shaderLocation = 0; // @location(0)
	vertexAttribs[0].format = WGPUVertexFormat_Float32x3;
	vertexAttribs[0].offset = 0;
	// Describe the color attribute
	vertexAttribs[1].shaderLocation = 1; // @location(1)
	vertexAttribs[1].format = WGPUVertexFormat_Float32x3; // different type!
	vertexAttribs[1].offset = 3 * sizeof(float); // non null offset!

	vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
	vertexBufferLayout.attributes = vertexAttribs.data();

	vertexBufferLayout.arrayStride = 6 * sizeof(float);
	//                               ^^^^^^^^^^^^^^^^^ The new stride
	vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

	// When describing the render pipeline:
	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = toWgpuStringView("vs_main");
	WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = toWgpuStringView("fs_main");
	WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
	colorTarget.format = m_surfaceFormat;
	WGPUBlendState blendState = WGPU_BLEND_STATE_INIT;
	colorTarget.blend = &blendState;
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;
	pipelineDesc.fragment = &fragmentState;


	// Define binding layout
	WGPUBindGroupLayoutEntry bindingLayout = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
	// The binding index as used in the @binding attribute in the shader
	bindingLayout.binding = 0;
	bindingLayout.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
	bindingLayout.buffer.type = WGPUBufferBindingType_Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

	// Create a bind group layout
	WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
	bindGroupLayoutDesc.nextInChain = nullptr;
	bindGroupLayoutDesc.entryCount = 1;
	bindGroupLayoutDesc.entries = &bindingLayout;
	m_bindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bindGroupLayoutDesc);

	// Create the pipeline layout
	WGPUPipelineLayoutDescriptor layoutDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
	layoutDesc.nextInChain = nullptr;
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = &m_bindGroupLayout;
	m_layout = wgpuDeviceCreatePipelineLayout(m_device, &layoutDesc);

	pipelineDesc.layout = m_layout;


	m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);
	wgpuShaderModuleRelease(shaderModule);








	return true;
}

bool Application::InitializeBuffers()
{
	std::vector<float> pointData;
	std::vector<uint16_t> indexData;

	// 1. Load from disk into CPU-side vectors pointData and indexData
	bool success = ResourceManager::loadGeometry(RESOURCE_DIR "/pyramid.txt", pointData, indexData, 3);
	if (!success) return false;

	m_indexCount = static_cast<uint32_t>(indexData.size());

	// 2. Create GPU buffers and upload data to them
	WGPUBufferDescriptor bufferDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
	bufferDesc.size = pointData.size() * sizeof(float);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
	m_pointBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);

	wgpuQueueWriteBuffer(m_queue, m_pointBuffer, 0, pointData.data(), bufferDesc.size);
	// It is not easy with the auto-generation of code to remove the previously
	// defined `vertexBuffer` attribute, but at the same time some compilers
	// (rightfully) complain if we do not use it. This is a hack to mark the
	// variable as used and have automated build tests pass.
	(void)m_vertexBuffer;
	(void)m_vertexCount;

	bufferDesc.size = indexData.size() * sizeof(uint16_t);
	bufferDesc.size = (bufferDesc.size + 3) & ~3; // round up to the next multiple of 4
	indexData.resize((indexData.size() + 1) & ~1); // round up to the next multiple of 2
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;;
	m_indexBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);


	wgpuQueueWriteBuffer(m_queue, m_indexBuffer, 0, indexData.data(), bufferDesc.size);


	// 3. Create and fill uniform buffer
	// Create uniform buffer (reusing bufferDesc from other buffer creations)
	// The buffer will only contain 1 float with the value of uTime
	// then 3 floats left empty but needed by alignment constraints
	bufferDesc.size = sizeof(MyUniforms);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
	m_uniformBuffer = wgpuDeviceCreateBuffer(m_device, &bufferDesc);
	MyUniforms uniforms;
	uniforms.time = 1.0f;
	uniforms.color = { 0.0f, 1.0f, 0.4f, 1.0f };
	wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0, &uniforms, sizeof(uniforms));

	return true;
}
void Application::InitializeBindGroups()
{
	// Create a binding
	WGPUBindGroupEntry binding = WGPU_BIND_GROUP_ENTRY_INIT;

	// The index of the binding (the entries in bindGroupDesc can be in any order)
	binding.binding = 0;
	// The buffer it is actually bound to
	binding.buffer = m_uniformBuffer;
	// We can specify an offset within the buffer, so that a single buffer can hold
	// multiple uniform blocks.
	binding.offset = 0;
	// And we specify again the size of the buffer.
	binding.size = sizeof(MyUniforms);

	// A bind group contains one or multiple bindings
	WGPUBindGroupDescriptor bindGroupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
	bindGroupDesc.layout = m_bindGroupLayout;
	// There must be as many bindings as declared in the layout!
	bindGroupDesc.entryCount = 1;
	bindGroupDesc.entries = &binding;
	m_bindGroup = wgpuDeviceCreateBindGroup(m_device, &bindGroupDesc);
}

