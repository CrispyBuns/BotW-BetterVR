#include "layer.h"

#undef CreateSemaphore

VkSemaphore RNDVK_CreateSemaphore(HANDLE d3d12Fence) {
    VkSemaphore semaphore;
	
    // Create the timeline semaphore to synchronize Vulkan synchronizing with D3D12s rendering
    VkSemaphoreTypeCreateInfo timelineCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    createInfo.pNext = &timelineCreateInfo;
    checkVkResult(device_dispatch[GetKey(vkSharedDevice)].CreateSemaphore(vkSharedDevice, &createInfo, nullptr, &semaphore));

    VkImportSemaphoreWin32HandleInfoKHR semaphoreImportInfo = { VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR };
    semaphoreImportInfo.semaphore = semaphore;
    semaphoreImportInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
    semaphoreImportInfo.handle = d3d12Fence;
    checkVkResult(device_dispatch[GetKey(vkSharedDevice)].ImportSemaphoreWin32HandleKHR(vkSharedDevice, &semaphoreImportInfo));

    return semaphore;
}

VkImage RNDVK_ImportImage(HANDLE d3d12Texture, uint32_t width, uint32_t height, VkFormat format) {
    auto findMemoryTypeIdx = [](uint32_t memoryTypeBitsRequirement, VkFlags requirementsMask) -> uint32_t {
        VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties = {};
        instance_dispatch.begin()->second.GetPhysicalDeviceMemoryProperties(vkSharedPhysicalDevice, &physicalDeviceMemoryProperties);
		
        for (uint32_t memoryIndex = 0; memoryIndex < VK_MAX_MEMORY_TYPES; memoryIndex++) {
            const uint32_t memoryTypeBits = (1 << memoryIndex);
            const bool isRequiredMemoryType = memoryTypeBitsRequirement & memoryTypeBits;
            const bool satisfiesFlags = (physicalDeviceMemoryProperties.memoryTypes[memoryIndex].propertyFlags & requirementsMask) == requirementsMask;
			
            if (isRequiredMemoryType && satisfiesFlags) {
                return memoryIndex;
            }
        }

        checkAssert(false);
        return 0;
    };

    // Prepare the Vulkan image that the app will use.
    VkImage image;

    VkExternalMemoryImageCreateInfo externalCreateInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
    externalCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;

    VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &externalCreateInfo };
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = format;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    checkVkResult(device_dispatch[GetKey(vkSharedDevice)].CreateImage(vkSharedDevice, &createInfo, nullptr, &image));

	// Import the memory from the D3D12 texture
    VkDeviceMemory memory;

    VkImageMemoryRequirementsInfo2 requirementInfo = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
    requirementInfo.image = image;
    VkMemoryRequirements2 requirements = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
    device_dispatch[GetKey(vkSharedDevice)].GetImageMemoryRequirements2(vkSharedDevice, &requirementInfo, &requirements);

    VkMemoryWin32HandlePropertiesKHR handleProperties = { VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR };
    checkVkResult(device_dispatch[GetKey(vkSharedDevice)].GetMemoryWin32HandlePropertiesKHR(vkSharedDevice, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT, d3d12Texture, &handleProperties));

    VkImportMemoryWin32HandleInfoKHR importInfo = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
    importInfo.handle = d3d12Texture;
	
    VkMemoryDedicatedAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, &importInfo };
    memoryAllocateInfo.image = image;
	
    VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &memoryAllocateInfo };
    allocateInfo.allocationSize = requirements.memoryRequirements.size;
    allocateInfo.memoryTypeIndex = findMemoryTypeIdx(handleProperties.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
    checkVkResult(device_dispatch[GetKey(vkSharedDevice)].AllocateMemory(vkSharedDevice, &allocateInfo, nullptr, &memory));

    checkVkResult(device_dispatch[GetKey(vkSharedDevice)].BindImageMemory(vkSharedDevice, image, memory, 0));
    return image;
}


void RNDVK_CopyImage(VkCommandBuffer currCmdBuffer, VkImage srcImage, VkImage dstImage) {
    VkImageMemoryBarrier copyToBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    copyToBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyToBarrier.subresourceRange.baseMipLevel = 0;
    copyToBarrier.subresourceRange.levelCount = 1;
    copyToBarrier.subresourceRange.baseArrayLayer = 0;
    copyToBarrier.subresourceRange.layerCount = 1;
    copyToBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    copyToBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    copyToBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    copyToBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    copyToBarrier.image = srcImage;
    device_dispatch[GetKey(vkSharedDevice)].CmdPipelineBarrier(currCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &copyToBarrier);

    VkImageCopy copyRegion = {};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.dstSubresource.mipLevel = 0;
    copyRegion.extent.width = FB_GetFrameDimensions().width;
    copyRegion.extent.height = FB_GetFrameDimensions().height;
    copyRegion.extent.depth = 1;
    device_dispatch[GetKey(vkSharedDevice)].CmdCopyImage(currCmdBuffer, srcImage, VK_IMAGE_LAYOUT_UNDEFINED, dstImage, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
    logPrint("Copied the source image to the VR presenting image!");

 //   const VkClearColorValue pColor = {
	//	1.0f, 1.0f, 1.0f, 1.0f
	//};
 //   VkImageSubresourceRange pRanges = {
	//	VK_IMAGE_ASPECT_COLOR_BIT,
	//	0,
	//	1,
	//	0,
	//	1
 //   };

	//device_dispatch[GetKey(vkSharedDevice)].CmdClearColorImage(currCmdBuffer, dstImage, VK_IMAGE_LAYOUT_GENERAL, &pColor, 1, &pRanges);

    VkImageMemoryBarrier optimalFormatBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    optimalFormatBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    optimalFormatBarrier.subresourceRange.baseMipLevel = 0;
    optimalFormatBarrier.subresourceRange.levelCount = 1;
    optimalFormatBarrier.subresourceRange.baseArrayLayer = 0;
    optimalFormatBarrier.subresourceRange.layerCount = 1;
    optimalFormatBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    optimalFormatBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    optimalFormatBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    optimalFormatBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    optimalFormatBarrier.image = dstImage;
    device_dispatch[GetKey(vkSharedDevice)].CmdPipelineBarrier(currCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &optimalFormatBarrier);
}