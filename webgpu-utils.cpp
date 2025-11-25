#include "webgpu-utils.h"

#include <iostream>
#include <vector>
#include <cassert>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#else // __EMSCRIPTEN__
#  include <thread>
#  include <chrono>
#endif // __EMSCRIPTEN__

std::string_view toStdStringView(WGPUStringView wgpuStringView) 
{
	return
		wgpuStringView.data == nullptr
		? std::string_view()
		: wgpuStringView.length == WGPU_STRLEN
		? std::string_view(wgpuStringView.data)
		: std::string_view(wgpuStringView.data, wgpuStringView.length);
}
WGPUStringView toWgpuStringView(std::string_view stdStringView) 
{
	return { stdStringView.data(), stdStringView.size() };
}
WGPUStringView toWgpuStringView(const char* cString) 
{
	return { cString, WGPU_STRLEN };
}

void sleepForMilliseconds(unsigned int milliseconds)
{
#ifdef __EMSCRIPTEN__
	emscripten_sleep(milliseconds);
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
}


#pragma region Device

WGPUInstance GetInstance()
{
	WGPUInstanceDescriptor desc = {};
	desc.nextInChain = nullptr;

	// We create the instance using this descriptor
#ifdef WEBGPU_BACKEND_EMSCRIPTEN
	WGPUInstance instance = wgpuCreateInstance(nullptr);
#else  //  WEBGPU_BACKEND_EMSCRIPTEN
	WGPUInstance instance = wgpuCreateInstance(&desc);
#endif //  WEBGPU_BACKEND_EMSCRIPTEN

	// We can check whether there is actually an instance created
	if (!instance)
	{
		std::cerr << "Could not initialize WebGPU!" << std::endl;
		return nullptr;
	}

	// Display the object (WGPUInstance is a simple pointer, it may be
	// copied around without worrying about its size).
	std::cout << "WGPU instance: " << instance << std::endl;

	return instance;
}

#pragma endregion


#pragma region Adapter
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const* options) {
	// A simple structure holding the local information shared with the
	// onAdapterRequestEnded callback.
	struct UserData {
		WGPUAdapter adapter = nullptr;
		bool requestEnded = false;
	};
	UserData userData;

	// Callback called by wgpuInstanceRequestAdapter when the request returns
	// This is a C++ lambda function, but could be any function defined in the
	// global scope. It must be non-capturing (the brackets [] are empty) so
	// that it behaves like a regular C function pointer, which is what
	// wgpuInstanceRequestAdapter expects (WebGPU being a C API). The workaround
	// is to convey what we want to capture through the userdata1 pointer,
	// provided as the last argument of wgpuInstanceRequestAdapter and received
	// by the callback as its last argument.
	auto onAdapterRequestEnded = [](
		WGPURequestAdapterStatus status,
		WGPUAdapter adapter,
		WGPUStringView message,
		void* userdata1,
		void* /* userdata2 */
		) {
			UserData& userData = *reinterpret_cast<UserData*>(userdata1);
			if (status == WGPURequestAdapterStatus_Success) {
				userData.adapter = adapter;
			}
			else {
				std::cerr << "Error while requesting adapter: " << toStdStringView(message) << std::endl;
			}
			userData.requestEnded = true;
		};

	// Build the callback info
	WGPURequestAdapterCallbackInfo callbackInfo = {
		/* nextInChain = */ nullptr,
		/* mode = */ WGPUCallbackMode_AllowProcessEvents,
		/* callback = */ onAdapterRequestEnded,
		/* userdata1 = */ &userData,
		/* userdata2 = */ nullptr
	};

	// Call to the WebGPU request adapter procedure
	wgpuInstanceRequestAdapter(instance, options, callbackInfo);

	// We wait until userData.requestEnded gets true

	// Hand the execution to the WebGPU instance so that it can check for
	// pending async operations, in which case it invokes our callbacks.
	// NB: We test once before the loop not to wait for 200ms in case it is
	// already ready
	wgpuInstanceProcessEvents(instance);

	while (!userData.requestEnded) {
		// Waiting for 200 ms to avoid asking too often to process events
		sleepForMilliseconds(200);

		wgpuInstanceProcessEvents(instance);
	}

	return userData.adapter;
}


void SetAdapterLimits(const WGPUAdapter& adapter)
{

#ifndef __EMSCRIPTEN__
	WGPULimits supportedLimits = {};
	supportedLimits.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
	bool success = wgpuAdapterGetLimits(adapter, &supportedLimits) == WGPUStatus_Success;
#else
	bool success = wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif

	if (success) {
		std::cout << "Adapter limits:" << std::endl;
		std::cout << " - maxTextureDimension1D: " << supportedLimits.maxTextureDimension1D << std::endl;
		std::cout << " - maxTextureDimension2D: " << supportedLimits.maxTextureDimension2D << std::endl;
		std::cout << " - maxTextureDimension3D: " << supportedLimits.maxTextureDimension3D << std::endl;
		std::cout << " - maxTextureArrayLayers: " << supportedLimits.maxTextureArrayLayers << std::endl;
	}
#endif // NOT __EMSCRIPTEN__

}


WGPUAdapter GetAdapter(const WGPUInstance& instance)
{
	std::cout << "Requesting adapter..." << std::endl;

	WGPURequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);

	std::cout << "Got adapter: " << adapter << std::endl;


	return adapter;

}


void InspectAdapter(const WGPUAdapter& adapter)
{
	// Prepare the struct where features will be listed
	WGPUSupportedFeatures features;

	// Get adapter features. This may allocate memory that we must later free with wgpuSupportedFeaturesFreeMembers()
	wgpuAdapterGetFeatures(adapter, &features);

	std::cout << "Adapter features:" << std::endl;
	std::cout << std::hex; // Write integers as hexadecimal to ease comparison with webgpu.h literals
	for (size_t i = 0; i < features.featureCount; ++i) {
		std::cout << " - 0x" << features.features[i] << std::endl;
	}
	std::cout << std::dec; // Restore decimal numbers

	// Free the memory that had potentially been allocated by wgpuAdapterGetFeatures()
	wgpuSupportedFeaturesFreeMembers(features);
	// One shall no longer use features beyond this line.
	WGPUAdapterInfo properties;
	properties.nextInChain = nullptr;
	wgpuAdapterGetInfo(adapter, &properties);
	std::cout << "Adapter properties:" << std::endl;
	std::cout << " - vendorID: " << properties.vendorID << std::endl;
	std::cout << " - vendorName: " << toStdStringView(properties.vendor) << std::endl;
	std::cout << " - architecture: " << toStdStringView(properties.architecture) << std::endl;
	std::cout << " - deviceID: " << properties.deviceID << std::endl;
	std::cout << " - name: " << toStdStringView(properties.device) << std::endl;
	std::cout << " - driverDescription: " << toStdStringView(properties.description) << std::endl;
	std::cout << std::hex;
	std::cout << " - adapterType: 0x" << properties.adapterType << std::endl;
	std::cout << " - backendType: 0x" << properties.backendType << std::endl;
	std::cout << std::dec; // Restore decimal numbers
	wgpuAdapterInfoFreeMembers(properties);

}
#pragma endregion


#pragma region Device
WGPUDevice requestDeviceSync(WGPUInstance instance, WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor)
{
	{
		struct UserData {
			WGPUDevice device = nullptr;
			bool requestEnded = false;
		};
		UserData userData;

		auto onDeviceRequestEnded = [](
			WGPURequestDeviceStatus status,
			WGPUDevice device,
			WGPUStringView message,
			void* userdata1,
			void* /* userdata2 */
			) {
				UserData& userData = *reinterpret_cast<UserData*>(userdata1);
				if (status == WGPURequestDeviceStatus_Success) {
					userData.device = device;
				}
				else {
					std::cout << "Could not get WebGPU device: " << toStdStringView(message) << std::endl;
				}
				userData.requestEnded = true;
			};

		// Build the callback info
		WGPURequestDeviceCallbackInfo callbackInfo = {
			/* nextInChain = */ nullptr,
			/* mode = */ WGPUCallbackMode_AllowProcessEvents,
			/* callback = */ onDeviceRequestEnded,
			/* userdata1 = */ &userData,
			/* userdata2 = */ nullptr
		};

		// Call to the WebGPU request adapter procedure
		wgpuAdapterRequestDevice(adapter, descriptor, callbackInfo);

		// Hand the execution to the WebGPU instance until the request ended
		wgpuInstanceProcessEvents(instance);
		while (!userData.requestEnded) {
			sleepForMilliseconds(200);
			wgpuInstanceProcessEvents(instance);
		}

		return userData.device;

	}
}

// We create a utility function to inspect the device:
void inspectDevice(WGPUDevice device) {

	WGPUSupportedFeatures features = {};
	wgpuDeviceGetFeatures(device, &features);
	std::cout << "Device features:" << std::endl;
	std::cout << std::hex;
	for (size_t i = 0; i < features.featureCount; ++i) {
		std::cout << " - 0x" << features.features[i] << std::endl;
	}
	std::cout << std::dec;
	wgpuSupportedFeaturesFreeMembers(features);

	WGPULimits limits = {};
	limits.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
	bool success = wgpuDeviceGetLimits(device, &limits) == WGPUStatus_Success;
#else
	bool success = wgpuDeviceGetLimits(device, &limits);
#endif

	if (success) {
		std::cout << "Device limits:" << std::endl;
		std::cout << " - maxTextureDimension1D: " << limits.maxTextureDimension1D << std::endl;
		std::cout << " - maxTextureDimension2D: " << limits.maxTextureDimension2D << std::endl;
		std::cout << " - maxTextureDimension3D: " << limits.maxTextureDimension3D << std::endl;
		std::cout << " - maxTextureArrayLayers: " << limits.maxTextureArrayLayers << std::endl;
		std::cout << " - maxBindGroups: " << limits.maxBindGroups << std::endl;
		std::cout << " - maxBindGroupsPlusVertexBuffers: " << limits.maxBindGroupsPlusVertexBuffers << std::endl;
		std::cout << " - maxBindingsPerBindGroup: " << limits.maxBindingsPerBindGroup << std::endl;
		std::cout << " - maxDynamicUniformBuffersPerPipelineLayout: " << limits.maxDynamicUniformBuffersPerPipelineLayout << std::endl;
		std::cout << " - maxDynamicStorageBuffersPerPipelineLayout: " << limits.maxDynamicStorageBuffersPerPipelineLayout << std::endl;
		std::cout << " - maxSampledTexturesPerShaderStage: " << limits.maxSampledTexturesPerShaderStage << std::endl;
		std::cout << " - maxSamplersPerShaderStage: " << limits.maxSamplersPerShaderStage << std::endl;
		std::cout << " - maxStorageBuffersPerShaderStage: " << limits.maxStorageBuffersPerShaderStage << std::endl;
		std::cout << " - maxStorageTexturesPerShaderStage: " << limits.maxStorageTexturesPerShaderStage << std::endl;
		std::cout << " - maxUniformBuffersPerShaderStage: " << limits.maxUniformBuffersPerShaderStage << std::endl;
		std::cout << " - maxUniformBufferBindingSize: " << limits.maxUniformBufferBindingSize << std::endl;
		std::cout << " - maxStorageBufferBindingSize: " << limits.maxStorageBufferBindingSize << std::endl;
		std::cout << " - minUniformBufferOffsetAlignment: " << limits.minUniformBufferOffsetAlignment << std::endl;
		std::cout << " - minStorageBufferOffsetAlignment: " << limits.minStorageBufferOffsetAlignment << std::endl;
		std::cout << " - maxVertexBuffers: " << limits.maxVertexBuffers << std::endl;
		std::cout << " - maxBufferSize: " << limits.maxBufferSize << std::endl;
		std::cout << " - maxVertexAttributes: " << limits.maxVertexAttributes << std::endl;
		std::cout << " - maxVertexBufferArrayStride: " << limits.maxVertexBufferArrayStride << std::endl;
		std::cout << " - maxInterStageShaderVariables: " << limits.maxInterStageShaderVariables << std::endl;
		std::cout << " - maxColorAttachments: " << limits.maxColorAttachments << std::endl;
		std::cout << " - maxColorAttachmentBytesPerSample: " << limits.maxColorAttachmentBytesPerSample << std::endl;
		std::cout << " - maxComputeWorkgroupStorageSize: " << limits.maxComputeWorkgroupStorageSize << std::endl;
		std::cout << " - maxComputeInvocationsPerWorkgroup: " << limits.maxComputeInvocationsPerWorkgroup << std::endl;
		std::cout << " - maxComputeWorkgroupSizeX: " << limits.maxComputeWorkgroupSizeX << std::endl;
		std::cout << " - maxComputeWorkgroupSizeY: " << limits.maxComputeWorkgroupSizeY << std::endl;
		std::cout << " - maxComputeWorkgroupSizeZ: " << limits.maxComputeWorkgroupSizeZ << std::endl;
		std::cout << " - maxComputeWorkgroupsPerDimension: " << limits.maxComputeWorkgroupsPerDimension << std::endl;
	}
}


WGPUDevice GetDevice(WGPUInstance& instance, WGPUAdapter& adapter)
{
	std::cout << "Requesting device..." << std::endl;

	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = toWgpuStringView("My Device"); // anything works here, that's your call
	deviceDesc.requiredFeatureCount = 0; // we do not require any specific feature
	deviceDesc.requiredLimits = nullptr; // we do not require any specific limit
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = toWgpuStringView("The default queue");

	// device lost callbacks
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
	WGPUDevice device = requestDeviceSync(instance, adapter, &deviceDesc);

	std::cout << "Got device: " << device << std::endl;

	inspectDevice(device);


	return device;

}
#pragma endregion


#pragma region Command_Queue
WGPUQueue SetupCommandQueue(const WGPUDevice& device)
{
	WGPUQueue queue = wgpuDeviceGetQueue(device);

	return queue
;

}

WGPUCommandBuffer BuildCommandBuffer(const WGPUDevice& device)
{
	// - encoder
	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoderDesc.label = toWgpuStringView("My command encoder");
	encoderDesc.nextInChain = nullptr;
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

	// commands
	wgpuCommandEncoderInsertDebugMarker(encoder, toWgpuStringView("Do one thing"));
	wgpuCommandEncoderInsertDebugMarker(encoder, toWgpuStringView("Do another thing"));

	// - buffer
	WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.nextInChain = nullptr;
	cmdBufferDescriptor.label = toWgpuStringView("Command buffer");
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);

	wgpuCommandEncoderRelease(encoder); // release encoder after it's finished

	return command;


}

void SubmitCommandQueue(const WGPUInstance& instance, const WGPUQueue& queue, const WGPUCommandBuffer& command)
{
	// - submit command queue
	std::cout << "Submitting command..." << std::endl;
	wgpuQueueSubmit(queue, 1, &command);
	wgpuCommandBufferRelease(command);
	std::cout << "Command submitted." << std::endl;

	// Device polling
	// Our callback invoked when GPU instructions have been executed
	auto onQueuedWorkDone = [](
		WGPUQueueWorkDoneStatus status,
		void* userdata1,
		void* /* userdata2 */
		) {
			// Display a warning when status is not success
			if (status != WGPUQueueWorkDoneStatus_Success) {
				std::cout << "Warning: wgpuQueueOnSubmittedWorkDone failed, this is suspicious!" << std::endl;
			}

			// Interpret userdata1 as a pointer to a boolean (and turn it into a
			// mutable reference), then turn it to 'true'
			bool& workDone = *reinterpret_cast<bool*>(userdata1);
			workDone = true;
		};

	// Create the boolean that will be passed to the callback as userdata1
	// and initialize it to 'false'
	bool workDone = false;

	// Create the callback info
	WGPUQueueWorkDoneCallbackInfo callbackInfo = {};
	callbackInfo.nextInChain = nullptr;
	callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	callbackInfo.callback = onQueuedWorkDone;
	callbackInfo.userdata1 = &workDone; // pass the address of workDone

	// Add the async operation to the queue
	wgpuQueueOnSubmittedWorkDone(queue, callbackInfo);

	// Hand the execution to the WebGPU instance until onQueuedWorkDone gets invoked
	wgpuInstanceProcessEvents(instance);
	while (!workDone) {
		sleepForMilliseconds(200);
		wgpuInstanceProcessEvents(instance);
	}

	std::cout << "All queued instructions have been executed!" << std::endl;

}

#pragma endregion
