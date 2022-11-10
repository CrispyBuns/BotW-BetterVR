#include "layer.h"

constexpr const char* const_BetterVR_Layer_Name = "VK_LAYER_BetterVR_Layer";
constexpr const char* const_BetterVR_Layer_Description = "Vulkan layer used to breath some VR into BotW for Cemu, using OpenXR!";

// Shared global variables
std::mutex global_lock;
std::map<void*, VkLayerInstanceDispatchTable> instance_dispatch;
std::map<void*, VkLayerDispatchTable> device_dispatch;

VkInstance vkSharedInstance = VK_NULL_HANDLE;
VkPhysicalDevice vkSharedPhysicalDevice = VK_NULL_HANDLE;
VkDevice vkSharedDevice = VK_NULL_HANDLE;

// Create methods
PFN_vkGetInstanceProcAddr saved_GetInstanceProcAddr = nullptr;
PFN_vkGetDeviceProcAddr saved_GetDeviceProcAddr = nullptr;

const std::vector<std::string> additionalInstanceExtensions = {
};

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
	// Get link info from pNext
	VkLayerInstanceCreateInfo* const chain_info = find_layer_info<VkLayerInstanceCreateInfo>(pCreateInfo->pNext, VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO, VK_LAYER_LINK_INFO);
	if (chain_info == nullptr) {
		return VK_ERROR_INITIALIZATION_FAILED;;
	}

	// Get next function from current chain and then move chain to next layer
	PFN_vkGetInstanceProcAddr next_GetInstanceProcAddr = chain_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	chain_info->u.pLayerInfo = chain_info->u.pLayerInfo->pNext;
	
	// Initialize layer-specific stuff
	logInitialize();
	SetEnvironmentVariableA("VK_INSTANCE_LAYERS", NULL);
	XR_initInstance();

	// Call vkCreateInstance
	saved_GetInstanceProcAddr = next_GetInstanceProcAddr;
	PFN_vkCreateInstance func_vkCreateInstance = (PFN_vkCreateInstance)next_GetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");

	// Modify VkInstance with needed extensions
	std::vector<const char*> modifiedExtensions;
	for (uint32_t i=0; i<pCreateInfo->enabledExtensionCount; i++) {
		modifiedExtensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);
	}
	for (const std::string& extension : additionalInstanceExtensions) {
		if (std::find(modifiedExtensions.begin(), modifiedExtensions.end(), extension) != modifiedExtensions.end()) {
			modifiedExtensions.push_back(extension.c_str());
		}
	}

	VkInstanceCreateInfo modifiedCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	modifiedCreateInfo.pNext = pCreateInfo->pNext;
	modifiedCreateInfo.flags = pCreateInfo->flags;
	modifiedCreateInfo.pApplicationInfo = pCreateInfo->pApplicationInfo;
	modifiedCreateInfo.enabledLayerCount = pCreateInfo->enabledLayerCount;
	modifiedCreateInfo.ppEnabledLayerNames = pCreateInfo->ppEnabledLayerNames;
	modifiedCreateInfo.enabledExtensionCount = (uint32_t)modifiedExtensions.size();
	modifiedCreateInfo.ppEnabledExtensionNames = modifiedExtensions.data();
	
	VkResult result = func_vkCreateInstance(&modifiedCreateInfo, pAllocator, pInstance);
	if (result != VK_SUCCESS) {
		logPrint(std::format("Failed to create Vulkan instance! Error {}", (std::underlying_type_t<VkResult>)result));
		XR_deinitInstance();
		logShutdown();
		return result;
	}
	vkSharedInstance = *pInstance;
	logPrint(std::format("Created Vulkan instance (using Vulkan {}.{}.{}) successfully!", VK_VERSION_MAJOR(modifiedCreateInfo.pApplicationInfo->apiVersion), VK_VERSION_MINOR(modifiedCreateInfo.pApplicationInfo->apiVersion), VK_VERSION_PATCH(modifiedCreateInfo.pApplicationInfo->apiVersion)));
	checkAssert(VK_VERSION_MINOR(modifiedCreateInfo.pApplicationInfo->apiVersion) != 0 || VK_VERSION_MAJOR(modifiedCreateInfo.pApplicationInfo->apiVersion) > 1, "Vulkan version needs to be v1.1 or higher!");

	VkLayerInstanceDispatchTable dispatchTable = {};
	dispatchTable.GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)next_GetInstanceProcAddr(*pInstance, "vkGetInstanceProcAddr");
	dispatchTable.DestroyInstance = (PFN_vkDestroyInstance)next_GetInstanceProcAddr(*pInstance, "vkDestroyInstance");
	dispatchTable.EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)next_GetInstanceProcAddr(*pInstance, "vkEnumeratePhysicalDevices");
	dispatchTable.EnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)next_GetInstanceProcAddr(*pInstance, "vkEnumerateDeviceExtensionProperties");
	dispatchTable.GetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)next_GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceQueueFamilyProperties");

	dispatchTable.GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)next_GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceMemoryProperties");
	dispatchTable.GetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)next_GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceProperties2");
	dispatchTable.GetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR)next_GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceProperties2KHR");
	dispatchTable.CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)next_GetInstanceProcAddr(*pInstance, "vkCreateWin32SurfaceKHR");
	dispatchTable.GetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)next_GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
	dispatchTable.GetPhysicalDeviceImageFormatProperties2 = (PFN_vkGetPhysicalDeviceImageFormatProperties2)next_GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceImageFormatProperties2");

	{
		scoped_lock l(global_lock);
		instance_dispatch[GetKey(*pInstance)] = dispatchTable;
	}

	return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL Layer_DestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
	scoped_lock l(global_lock);
	instance_dispatch.erase(GetKey(instance));
}

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_EnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) {
	scoped_lock l(global_lock);
	
	uint32_t internalCount = 0;
	checkVkResult(instance_dispatch[GetKey(instance)].EnumeratePhysicalDevices(instance, &internalCount, nullptr), "Failed to retrieve number of vulkan physical devices!");
	std::vector<VkPhysicalDevice> internalDevices(internalCount);
	checkVkResult(instance_dispatch[GetKey(instance)].EnumeratePhysicalDevices(instance, &internalCount, internalDevices.data()), "Failed to retrieve vulkan physical devices!");

	for (const VkPhysicalDevice& device : internalDevices) {
		VkPhysicalDeviceProperties2 properties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		VkPhysicalDeviceIDProperties deviceId = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
		properties.pNext = &deviceId;
		instance_dispatch[GetKey(instance)].GetPhysicalDeviceProperties2(device, &properties);
		
		D3D_FEATURE_LEVEL xrMinFeatureLevel;
		LUID xrAdapterLUID;
		XR_GetSupportedAdapter(&xrMinFeatureLevel, &xrAdapterLUID);

		if (deviceId.deviceLUIDValid && memcmp(&xrAdapterLUID, deviceId.deviceLUID, VK_LUID_SIZE) == 0) {
			if (pPhysicalDevices != nullptr) {
				if (*pPhysicalDeviceCount < 1) {
					*pPhysicalDeviceCount = 1;
					return VK_INCOMPLETE;
				}
				*pPhysicalDeviceCount = 1;
				pPhysicalDevices[0] = device;
				vkSharedPhysicalDevice = device;
				d3d12SharedAdapter = xrAdapterLUID;
				d3d12SharedFeatureLevel = xrMinFeatureLevel;
				return VK_SUCCESS;
			}
			else {
				*pPhysicalDeviceCount = 1;
				return VK_SUCCESS;
			}
		}
	}
	*pPhysicalDeviceCount = 0;
	return VK_SUCCESS;
}


const std::vector<std::string> additionalDeviceExtensions = {
	VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
	VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
	VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
};

VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_CreateDevice(VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
	// Get link info from pNext
	VkLayerDeviceCreateInfo* const chain_info = find_layer_info<VkLayerDeviceCreateInfo>(pCreateInfo->pNext, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO, VK_LAYER_LINK_INFO);
	if (chain_info == nullptr) {
		return VK_ERROR_INITIALIZATION_FAILED;;
	}

	// Get next function from current chain and then move chain to next layer
	PFN_vkGetInstanceProcAddr next_GetInstanceProcAddr = chain_info->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	PFN_vkGetDeviceProcAddr next_GetDeviceProcAddr = chain_info->u.pLayerInfo->pfnNextGetDeviceProcAddr;
	chain_info->u.pLayerInfo = chain_info->u.pLayerInfo->pNext;

	// Call vkCreateDevice
	saved_GetInstanceProcAddr = next_GetInstanceProcAddr;
	saved_GetDeviceProcAddr = next_GetDeviceProcAddr;
	PFN_vkCreateDevice func_vkCreateDevice = (PFN_vkCreateDevice)next_GetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateDevice");

	// Modify VkInstance with needed extensions
	std::vector<const char*> modifiedExtensions;
	for (uint32_t i=0; i<pCreateInfo->enabledExtensionCount; i++) {
		modifiedExtensions.push_back(pCreateInfo->ppEnabledExtensionNames[i]);
	}
	for (const std::string& extension : additionalDeviceExtensions) {
		if (std::find(modifiedExtensions.begin(), modifiedExtensions.end(), extension) == modifiedExtensions.end()) {
			modifiedExtensions.push_back(extension.c_str());
		}
	}
	
	VkPhysicalDeviceTimelineSemaphoreFeatures createSemaphoreFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES };
	createSemaphoreFeatures.timelineSemaphore = true;
	createSemaphoreFeatures.pNext = const_cast<void*>(pCreateInfo->pNext);

	VkDeviceCreateInfo modifiedCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	modifiedCreateInfo.pNext = &createSemaphoreFeatures;
	modifiedCreateInfo.flags = pCreateInfo->flags;
	modifiedCreateInfo.queueCreateInfoCount = pCreateInfo->queueCreateInfoCount;
	modifiedCreateInfo.pQueueCreateInfos = pCreateInfo->pQueueCreateInfos;
	modifiedCreateInfo.enabledLayerCount = pCreateInfo->enabledLayerCount;
	modifiedCreateInfo.ppEnabledLayerNames = pCreateInfo->ppEnabledLayerNames;
	modifiedCreateInfo.enabledExtensionCount = (uint32_t)modifiedExtensions.size();
	modifiedCreateInfo.ppEnabledExtensionNames = modifiedExtensions.data();

	VkResult result = func_vkCreateDevice(gpu, &modifiedCreateInfo, pAllocator, pDevice);
	if (result != VK_SUCCESS) {
		logPrint(std::format("Failed to create Vulkan device! Error {}", (std::underlying_type_t<VkResult>)result));
		XR_deinitInstance();
		logShutdown();
		return result;
	}
	vkSharedDevice = *pDevice;
	logPrint("Created Vulkan device successfully!");

	VkLayerDispatchTable deviceTable = {};
	deviceTable.GetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)next_GetDeviceProcAddr(*pDevice, "vkGetDeviceProcAddr");
	deviceTable.DestroyDevice = (PFN_vkDestroyDevice)next_GetDeviceProcAddr(*pDevice, "vkDestroyDevice");

	deviceTable.CreateImageView = (PFN_vkCreateImageView)next_GetDeviceProcAddr(*pDevice, "vkCreateImageView");
	deviceTable.UpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)next_GetDeviceProcAddr(*pDevice, "vkUpdateDescriptorSets");
	deviceTable.CmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)next_GetDeviceProcAddr(*pDevice, "vkCmdBindDescriptorSets");
	deviceTable.CreateRenderPass = (PFN_vkCreateRenderPass)next_GetDeviceProcAddr(*pDevice, "vkCreateRenderPass");
	deviceTable.CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)next_GetDeviceProcAddr(*pDevice, "vkCmdBeginRenderPass");
	deviceTable.CmdEndRenderPass = (PFN_vkCmdEndRenderPass)next_GetDeviceProcAddr(*pDevice, "vkCmdEndRenderPass");
	deviceTable.QueueSubmit = (PFN_vkQueueSubmit)next_GetDeviceProcAddr(*pDevice, "vkQueueSubmit");
	deviceTable.QueuePresentKHR = (PFN_vkQueuePresentKHR)next_GetDeviceProcAddr(*pDevice, "vkQueuePresentKHR");

	deviceTable.AllocateMemory = (PFN_vkAllocateMemory)next_GetDeviceProcAddr(*pDevice, "vkAllocateMemory");
	deviceTable.BindImageMemory = (PFN_vkBindImageMemory)next_GetDeviceProcAddr(*pDevice, "vkBindImageMemory");
	deviceTable.GetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)next_GetDeviceProcAddr(*pDevice, "vkGetImageMemoryRequirements");
	deviceTable.CreateImage = (PFN_vkCreateImage)next_GetDeviceProcAddr(*pDevice, "vkCreateImage");
	deviceTable.CmdCopyImage = (PFN_vkCmdCopyImage)next_GetDeviceProcAddr(*pDevice, "vkCmdCopyImage");

	// D3D12 specific
	deviceTable.CreateSemaphore = (PFN_vkCreateSemaphore)next_GetDeviceProcAddr(*pDevice, "vkCreateSemaphore");
	deviceTable.ImportSemaphoreWin32HandleKHR = (PFN_vkImportSemaphoreWin32HandleKHR)next_GetDeviceProcAddr(*pDevice, "vkImportSemaphoreWin32HandleKHR");
	deviceTable.GetMemoryWin32HandlePropertiesKHR = (PFN_vkGetMemoryWin32HandlePropertiesKHR)next_GetDeviceProcAddr(*pDevice, "vkGetMemoryWin32HandlePropertiesKHR");
	deviceTable.GetImageMemoryRequirements2 = (PFN_vkGetImageMemoryRequirements2)next_GetDeviceProcAddr(*pDevice, "vkGetImageMemoryRequirements2");
	deviceTable.CmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)next_GetDeviceProcAddr(*pDevice, "vkCmdPipelineBarrier");


	deviceTable.CmdClearColorImage = (PFN_vkCmdClearColorImage)next_GetDeviceProcAddr(*pDevice, "vkCmdClearColorImage");
	
	{
		scoped_lock l(global_lock);
		device_dispatch[GetKey(*pDevice)] = deviceTable;
	}

	return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL Layer_DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
	scoped_lock l(global_lock);
	device_dispatch.erase(GetKey(device));
}

// Can't be hooked since this is an explicit layer (prefered since implicit means it has to be installed and is non-portable)
VkResult Layer_EnumerateInstanceVersion(const VkEnumerateInstanceVersionChain* pChain, uint32_t* pApiVersion) {
	//XrVersion minVersion = 0;
	//XrVersion maxVersion = 0;
	//XR_GetSupportedVulkanVersions(&minVersion, &maxVersion);
	*pApiVersion = VK_API_VERSION_1_2;
	return pChain->CallDown(pApiVersion);
}

// GetProcAddr hooks
// todo: implement layerGetPhysicalDeviceProcAddr if necessary
// https://github.dev/crosire/reshade/tree/main/source/vulkan
// https://github.dev/baldurk/renderdoc/tree/v1.x/renderdoc/driver/vulkan
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL Layer_GetInstanceProcAddr(VkInstance instance, const char* pName) {	
	HOOK_PROC_FUNC(CreateInstance);
	HOOK_PROC_FUNC(DestroyInstance);
	HOOK_PROC_FUNC(CreateDevice);
	HOOK_PROC_FUNC(DestroyDevice);
	HOOK_PROC_FUNC(EnumeratePhysicalDevices);

	// todo: sort these out later
	//HOOK_PROC_FUNC(EnumerateInstanceVersion);

	// Hook render functions for framebuffer tracking
	// todo: sort out which ones can't be called using a device
	HOOK_PROC_FUNC(CreateImage);
	HOOK_PROC_FUNC(CreateImageView);
	HOOK_PROC_FUNC(UpdateDescriptorSets);
	HOOK_PROC_FUNC(CmdBindDescriptorSets);
	HOOK_PROC_FUNC(CreateRenderPass);
	HOOK_PROC_FUNC(CmdBeginRenderPass);
	HOOK_PROC_FUNC(CmdEndRenderPass);

	HOOK_PROC_FUNC(QueueSubmit);
	HOOK_PROC_FUNC(QueuePresentKHR);

	// Self-intercept for compatibility
	HOOK_PROC_FUNC(GetInstanceProcAddr);

	{
		scoped_lock l(global_lock);
		PFN_vkVoidFunction voidFunc = instance_dispatch[GetKey(instance)].GetInstanceProcAddr(instance, pName);
		//logPrint(std::format("[DEBUG] Layer_GetInstanceProcAddr {} using {}, result = {}", pName, (void*)instance, (void*)voidFunc));
		return voidFunc;
	}
}


VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL Layer_GetDeviceProcAddr(VkDevice device, const char* pName) {
	HOOK_PROC_FUNC(CreateInstance);
	HOOK_PROC_FUNC(DestroyInstance);
	HOOK_PROC_FUNC(CreateDevice);
	HOOK_PROC_FUNC(DestroyDevice);
	HOOK_PROC_FUNC(EnumeratePhysicalDevices);

	HOOK_PROC_FUNC(CreateImage);
	HOOK_PROC_FUNC(CreateImageView);
	HOOK_PROC_FUNC(UpdateDescriptorSets);
	HOOK_PROC_FUNC(CmdBindDescriptorSets);
	HOOK_PROC_FUNC(CreateRenderPass);
	HOOK_PROC_FUNC(CmdBeginRenderPass);
	HOOK_PROC_FUNC(CmdEndRenderPass);

	HOOK_PROC_FUNC(QueueSubmit);
	HOOK_PROC_FUNC(QueuePresentKHR);


	// Required to self-intercept for compatibility
	HOOK_PROC_FUNC(GetDeviceProcAddr);

	//HOOK_PROC_FUNC(EnumeratePhysicalDevices);

	{
		scoped_lock l(global_lock);
		PFN_vkVoidFunction voidFunc = device_dispatch[GetKey(device)].GetDeviceProcAddr(device, pName);
		//logPrint(std::format("[DEBUG] Layer_GetDeviceProcAddr {} using {}, result = {}", pName, (void*)device, (void*)voidFunc));
		return voidFunc;
	}
}

// Required for loading negotiations
VK_LAYER_EXPORT VkResult VKAPI_CALL Layer_NegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct) {
    if (pVersionStruct->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
		return VK_ERROR_INITIALIZATION_FAILED;

    if (pVersionStruct->loaderLayerInterfaceVersion >= 2) {
        pVersionStruct->pfnGetInstanceProcAddr = Layer_GetInstanceProcAddr;
        pVersionStruct->pfnGetDeviceProcAddr = Layer_GetDeviceProcAddr;
        pVersionStruct->pfnGetPhysicalDeviceProcAddr = NULL;
    }
	
    static_assert(CURRENT_LOADER_LAYER_INTERFACE_VERSION == 2);

    if (pVersionStruct->loaderLayerInterfaceVersion > 2)
        pVersionStruct->loaderLayerInterfaceVersion = 2;

    return VK_SUCCESS;
}