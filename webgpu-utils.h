#pragma once

#include <webgpu/webgpu.h>
#include <string_view>
/**
 * Convert a WebGPU string view into a C++ std::string_view.
 */
std::string_view toStdStringView(WGPUStringView wgpuStringView);

/**
 * Convert a C++ std::string_view into a WebGPU string view.
 */
WGPUStringView toWgpuStringView(std::string_view stdStringView);

/**
 * Convert a C string into a WebGPU string view
 */
WGPUStringView toWgpuStringView(const char* cString);

/**
 * Sleep for a given number of milliseconds.
 * This works with both native builds and emscripten, provided that -sASYNCIFY
 * compile option is provided when building with emscripten.
 */
void sleepForMilliseconds(unsigned int milliseconds);



#pragma region Adapter
/**
 * Utility function to get a WebGPU adapter, so that
 *     WGPUAdapter adapter = requestAdapter(options);
 * is roughly equivalent to
 *     const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options);

void SetAdapterLimits(const WGPUAdapter& adapter);

WGPUAdapter GetAdapter(const WGPUInstance& instance);

void InspectAdapter(const WGPUAdapter& adapter);
#pragma endregion
