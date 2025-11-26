// In Application.cpp
#include "Application.h"
#include "webgpu-utils.h"

#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
// In Application.cpp
#include <glfw3webgpu.h>


bool Application::Initialize()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // <-- extra info for glfwCreateWindow
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

	// Instance setup
	m_instance = GetInstance();

	// Adapter setup
	m_surface = glfwCreateWindowWGPUSurface(m_instance, m_window);
	WGPUAdapter adapter = GetAdapter(m_instance, m_surface);
	SetAdapterLimits(adapter);
	InspectAdapter(adapter);

	// Device setup
	m_device = GetDevice(m_instance, adapter);

	// We no longer need to access the adapter once we have the device
	wgpuAdapterRelease(adapter);


	// Command queue
	m_queue = SetupCommandQueue(m_device);
	WGPUCommandBuffer command = BuildCommandBuffer(m_device);
	SubmitCommandQueue(m_instance, m_queue, command);

	return true;
}

void Application::Terminate()
{
	wgpuQueueRelease(m_queue);
	wgpuSurfaceRelease(m_surface);
	wgpuDeviceRelease(m_device);
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void Application::MainLoop()
{
	while (IsRunning())
	{

	}
}

bool Application::IsRunning()
{
	return !glfwWindowShouldClose(m_window);
}
