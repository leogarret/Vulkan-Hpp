// Copyright(c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// VulkanHpp Samples : 13_InitVertexBuffer
//                     Initialize vertex buffer

#include "../utils/geometries.hpp"
#include "../utils/utils.hpp"
#include "vulkan/vulkan.hpp"
#include <iostream>

static char const* AppName = "13_InitVertexBuffer";
static char const* EngineName = "Vulkan.hpp";

int main(int /*argc*/, char ** /*argv*/)
{
  try
  {
    vk::UniqueInstance instance = vk::su::createInstance(AppName, EngineName, vk::su::getInstanceExtensions());
#if !defined(NDEBUG)
    vk::UniqueDebugReportCallbackEXT debugReportCallback = vk::su::createDebugReportCallback(instance);
#endif

    std::vector<vk::PhysicalDevice> physicalDevices = instance->enumeratePhysicalDevices();
    assert(!physicalDevices.empty());

    uint32_t width = 64;
    uint32_t height = 64;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    HWND window = vk::su::initializeWindow(AppName, AppName, width, height);
    vk::UniqueSurfaceKHR surface = instance->createWin32SurfaceKHRUnique(vk::Win32SurfaceCreateInfoKHR({}, GetModuleHandle(nullptr), window));
#else
#pragma error "unhandled platform"
#endif

    std::pair<uint32_t, uint32_t> graphicsAndPresentQueueFamilyIndex = vk::su::findGraphicsAndPresentQueueFamilyIndex(physicalDevices[0], surface);
    vk::UniqueDevice device = vk::su::createDevice(physicalDevices[0], graphicsAndPresentQueueFamilyIndex.first, vk::su::getDeviceExtensions());

    vk::UniqueCommandPool commandPool = vk::su::createCommandPool(device, graphicsAndPresentQueueFamilyIndex.first);
    std::vector<vk::UniqueCommandBuffer> commandBuffers = device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(commandPool.get(), vk::CommandBufferLevel::ePrimary, 1));

    vk::Queue graphicsQueue = device->getQueue(graphicsAndPresentQueueFamilyIndex.first, 0);

    vk::Format colorFormat = vk::su::pickColorFormat(physicalDevices[0].getSurfaceFormatsKHR(surface.get()));
    vk::UniqueSwapchainKHR swapChain = vk::su::createSwapChain(physicalDevices[0], surface, device, width, height, colorFormat, graphicsAndPresentQueueFamilyIndex.first, graphicsAndPresentQueueFamilyIndex.second);
    std::vector<vk::UniqueImageView> swapChainImageViews = vk::su::createSwapChainImageViews(device, swapChain, colorFormat);

    vk::Format depthFormat = vk::Format::eD16Unorm;
    vk::UniqueImage depthImage = vk::su::createImage(device, depthFormat, width, height);
    vk::UniqueDeviceMemory depthMemory = vk::su::allocateMemory(device, physicalDevices[0].getMemoryProperties(), device->getImageMemoryRequirements(depthImage.get()), vk::MemoryPropertyFlagBits::eDeviceLocal);
    device->bindImageMemory(depthImage.get(), depthMemory.get(), 0);
    vk::UniqueImageView depthViewImage = vk::su::createImageView(device, depthImage, depthFormat);

    vk::UniqueRenderPass renderPass = vk::su::createRenderPass(device, colorFormat, depthFormat);

    std::vector<vk::UniqueFramebuffer> framebuffers = vk::su::createFramebuffers(device, renderPass, swapChainImageViews, depthViewImage, width, height);

    /* VULKAN_KEY_START */

    // create a vertex buffer for some vertex and color data
    vk::UniqueBuffer vertexBuffer = device->createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(), sizeof(coloredCubeData), vk::BufferUsageFlagBits::eVertexBuffer));

    // allocate device memory for that buffer
    vk::MemoryRequirements memoryRequirements = device->getBufferMemoryRequirements(vertexBuffer.get());
    uint32_t memoryTypeIndex = vk::su::findMemoryType(physicalDevices[0].getMemoryProperties(), memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::UniqueDeviceMemory deviceMemory = device->allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));

    // copy the vertex and color data into that device memory
    uint8_t *pData = static_cast<uint8_t*>(device->mapMemory(deviceMemory.get(), 0, memoryRequirements.size));
    memcpy(pData, coloredCubeData, sizeof(coloredCubeData));
    device->unmapMemory(deviceMemory.get());

    // and bind the device memory to the vertex buffer
    device->bindBufferMemory(vertexBuffer.get(), deviceMemory.get(), 0);

    vk::UniqueSemaphore imageAcquiredSemaphore = device->createSemaphoreUnique(vk::SemaphoreCreateInfo(vk::SemaphoreCreateFlags()));
    vk::ResultValue<uint32_t> currentBuffer = device->acquireNextImageKHR(swapChain.get(), vk::su::FenceTimeout, imageAcquiredSemaphore.get(), nullptr);
    assert(currentBuffer.result == vk::Result::eSuccess);
    assert(currentBuffer.value < framebuffers.size());

    vk::ClearValue clearValues[2];
    clearValues[0].color = vk::ClearColorValue(std::array<float, 4>({ 0.2f, 0.2f, 0.2f, 0.2f }));
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    commandBuffers[0]->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

    vk::RenderPassBeginInfo renderPassBeginInfo(renderPass.get(), framebuffers[currentBuffer.value].get(), vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)), 2, clearValues);
    commandBuffers[0]->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    
    VkDeviceSize offset = 0;
    commandBuffers[0]->bindVertexBuffers(0, vertexBuffer.get(), offset);

    commandBuffers[0]->endRenderPass();
    commandBuffers[0]->end();
    vk::su::submitAndWait(device, graphicsQueue, commandBuffers[0]);

    // Note: No need to explicitly destroy the vertexBuffer, deviceMemory, or semaphore, as the destroy functions are called
    // by the destructor of the UniqueBuffer, UniqueDeviceMemory, and UniqueSemaphore, respectively, on leaving this scope.

    /* VULKAN_KEY_END */

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    DestroyWindow(window);
#else
#pragma error "unhandled platform"
#endif
  }
  catch (vk::SystemError err)
  {
    std::cout << "vk::SystemError: " << err.what() << std::endl;
    exit(-1);
  }
  catch (std::runtime_error err)
  {
    std::cout << "std::runtime_error: " << err.what() << std::endl;
    exit(-1);
  }
  catch (...)
  {
    std::cout << "unknown error\n";
    exit(-1);
  }
  return 0;
}
