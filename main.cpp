// Includes
#include <webgpu/webgpu.h>
#include <cassert>
#include <vector>
#include <iostream>
#include "webgpu-utils.h"



int main (int, char**) 
{
	// Instance setup
	WGPUInstance instance = GetInstance();

	// Adapter setup
	WGPUAdapter adapter = GetAdapter(instance);
	SetAdapterLimits(adapter);
	InspectAdapter(adapter);

	// Device setup
	WGPUDevice device = GetDevice(instance, adapter);

	// We no longer need to access the adapter once we have the device
	wgpuAdapterRelease(adapter);
	wgpuDeviceRelease(device);

	// We clean up the WebGPU instance
	wgpuInstanceRelease(instance);








}