#include "layer.h"

OpenXRRuntime currRuntime = UNKNOWN;

XrInstance xrSharedInstance = XR_NULL_HANDLE;
XrSystemId xrSharedSystemId = XR_NULL_SYSTEM_ID;
XrSession xrSharedSession = XR_NULL_HANDLE;

XrSpace xrSpace = XR_NULL_HANDLE;

XrDebugUtilsMessengerEXT xrDebugMessengerHandle = XR_NULL_HANDLE;
//XrGraphicsBindingVulkanKHR xrVulkanBindings = {};
//VkSurfaceKHR xrSurfaceHandle = XR_NULL_HANDLE;
//uint32_t hmdViewCount = 0;

// Instance Properties/Requirements
uint32_t xrMaxSwapchainWidth = 0;
uint32_t xrMaxSwapchainHeight = 0;
bool xrSupportsOrientational = false;
bool xrSupportsPositional = false;
bool xrSupportsFovMutable = false;


void XR_initPointers(bool timeConvSupported, bool debugUtilsSupported) {
	xrGetInstanceProcAddr(xrSharedInstance, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&func_xrGetD3D12GraphicsRequirementsKHR);

	if (timeConvSupported) {
		xrGetInstanceProcAddr(xrSharedInstance, "xrConvertTimeToWin32PerformanceCounterKHR", (PFN_xrVoidFunction*)&func_xrConvertTimeToWin32PerformanceCounterKHR);
		xrGetInstanceProcAddr(xrSharedInstance, "xrConvertWin32PerformanceCounterToTimeKHR", (PFN_xrVoidFunction*)&func_xrConvertWin32PerformanceCounterToTimeKHR);
	}

	if (debugUtilsSupported) {
		xrGetInstanceProcAddr(xrSharedInstance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&func_xrCreateDebugUtilsMessengerEXT);
		xrGetInstanceProcAddr(xrSharedInstance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&func_xrDestroyDebugUtilsMessengerEXT);
	}
}

void XR_deinitPointers() {
	func_xrGetD3D12GraphicsRequirementsKHR = nullptr;
	func_xrConvertTimeToWin32PerformanceCounterKHR = nullptr;
	func_xrConvertWin32PerformanceCounterToTimeKHR = nullptr;
	func_xrCreateDebugUtilsMessengerEXT = nullptr;
	func_xrDestroyDebugUtilsMessengerEXT = nullptr;
}

void XR_DebugUtilsMessengerCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity, XrDebugUtilsMessageTypeFlagsEXT messageType, const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) {
	logPrint(std::string(callbackData->functionName) + std::string(": ") + std::string(callbackData->message));
}

void XR_initInstance() {
	if (xrSharedInstance != XR_NULL_HANDLE)
		throw std::runtime_error("Shouldn't initialize the instance twice!");

	uint32_t xrExtensionCount = 0;
	xrEnumerateInstanceExtensionProperties(NULL, 0, &xrExtensionCount, NULL);
	std::vector<XrExtensionProperties> instanceExtensions;
	instanceExtensions.resize(xrExtensionCount, { XR_TYPE_EXTENSION_PROPERTIES , NULL });
	checkXRResult(xrEnumerateInstanceExtensionProperties(NULL, xrExtensionCount, &xrExtensionCount, instanceExtensions.data()), "Couldn't enumerate OpenXR extensions!");

	bool d3d12Supported = false;
	bool timeConvSupported = false;
	bool debugUtilsSupported = false;
	for (XrExtensionProperties& extensionProperties : instanceExtensions) {
		if (strcmp(extensionProperties.extensionName, XR_KHR_D3D12_ENABLE_EXTENSION_NAME) == 0) {
			d3d12Supported = true;
		}
		else if (strcmp(extensionProperties.extensionName, XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME) == 0) {
			timeConvSupported = true;
		}
		else if (strcmp(extensionProperties.extensionName, XR_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
			debugUtilsSupported = true;
		}
	}

	if (!d3d12Supported) {
		logPrint("OpenXR runtime doesn't support D3D12 (XR_KHR_D3D12_ENABLE)!");
		throw std::runtime_error("Current OpenXR runtime doesn't support Direct3D 12 (XR_KHR_D3D12_ENABLE). See the Github page's troubleshooting section for a solution!");
	}
	if (!timeConvSupported) {
		logPrint("OpenXR runtime doesn't support converting time from/to XrTime (XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME)!");
		throw std::runtime_error("Current OpenXR runtime doesn't support converting time from/to XrTime (XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME). See the Github page's troubleshooting section for a solution!");
	}
	if (!debugUtilsSupported) {
		logPrint("OpenXR runtime doesn't support debug utils (XR_EXT_DEBUG_UTILS)! Errors/debug information will no longer be able to be shown!");
	}

	// todo: disable debug utils when compiled in release mode
	std::vector<const char*> enabledExtensions = { XR_KHR_D3D12_ENABLE_EXTENSION_NAME, XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME };
	if (debugUtilsSupported) enabledExtensions.emplace_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);

	XrInstanceCreateInfo xrInstanceCreateInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
	xrInstanceCreateInfo.createFlags = 0;
	xrInstanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
	xrInstanceCreateInfo.enabledExtensionNames = enabledExtensions.data();
	xrInstanceCreateInfo.enabledApiLayerCount = 0;
	xrInstanceCreateInfo.enabledApiLayerNames = NULL;
	xrInstanceCreateInfo.applicationInfo = { "BetterVR OpenXR", 1, "Cemu", 1, XR_CURRENT_API_VERSION };
	checkXRResult(xrCreateInstance(&xrInstanceCreateInfo, &xrSharedInstance), "Failed to initialize the OpenXR instance!");

	XR_initPointers(timeConvSupported, debugUtilsSupported);

	if (debugUtilsSupported) {
		XrDebugUtilsMessengerCreateInfoEXT utilsMessengerCreateInfo = { XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		utilsMessengerCreateInfo.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		utilsMessengerCreateInfo.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
		utilsMessengerCreateInfo.userCallback = (PFN_xrDebugUtilsMessengerCallbackEXT)&XR_DebugUtilsMessengerCallback;
		func_xrCreateDebugUtilsMessengerEXT(xrSharedInstance, &utilsMessengerCreateInfo, &xrDebugMessengerHandle);
	}

	// System
	XrSystemGetInfo xrSystemGetInfo = { XR_TYPE_SYSTEM_GET_INFO };
	xrSystemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	checkXRResult(xrGetSystem(xrSharedInstance, &xrSystemGetInfo, &xrSharedSystemId), "No (available) head mounted display found!");

	XrSystemProperties xrSystemProperties = { XR_TYPE_SYSTEM_PROPERTIES };
	checkXRResult(xrGetSystemProperties(xrSharedInstance, xrSharedSystemId, &xrSystemProperties), "Couldn't get system properties of the given VR headset!");
	xrMaxSwapchainWidth = xrSystemProperties.graphicsProperties.maxSwapchainImageWidth;
	xrMaxSwapchainHeight = xrSystemProperties.graphicsProperties.maxSwapchainImageHeight;
	xrSupportsOrientational = xrSystemProperties.trackingProperties.orientationTracking;
	xrSupportsPositional = xrSystemProperties.trackingProperties.positionTracking;

	XrInstanceProperties properties = { XR_TYPE_INSTANCE_PROPERTIES };
	checkXRResult(xrGetInstanceProperties(xrSharedInstance, &properties), "Failed to get runtime details using xrGetInstanceProperties!");
	std::string runtimeUsed = properties.runtimeName;
	if (runtimeUsed == "Oculus") {
		currRuntime = OCULUS_RUNTIME;
	}
	else if (runtimeUsed == "SteamVR" || runtimeUsed == "SteamVR/OpenXR") {
		currRuntime = STEAMVR_RUNTIME;
	}
	else {
		std::string errorMsg = std::format("[ERROR] Using an unsupported OpenXR runtime: {}!", runtimeUsed);
		checkXRResult(XR_ERROR_API_VERSION_UNSUPPORTED, errorMsg.c_str());
	}

	auto xrViewConf = XR_CreateViewConfiguration();

	// Print configuration used, mostly for debugging purposes
	logPrint("Acquired system to be used:");
	logPrint(std::format(" - System Name: ") + xrSystemProperties.systemName);
	logPrint(std::format(" - Runtime Name: {}", properties.runtimeName));
	logPrint(std::format(" - Runtime Version: {}.{}.{}", XR_VERSION_MAJOR(properties.runtimeVersion), XR_VERSION_MINOR(properties.runtimeVersion), XR_VERSION_PATCH(properties.runtimeVersion)));
	logPrint(std::format(" - Supports Mutable FOV: {}", xrSupportsFovMutable ? "Yes" : "No"));
	logPrint(std::format(" - Supports Orientation Tracking: {}", xrSystemProperties.trackingProperties.orientationTracking ? "Yes" : "No"));
	logPrint(std::format(" - Supports Positional Tracking: {}", xrSystemProperties.trackingProperties.positionTracking ? "Yes" : "No"));
	logPrint(std::format(" - Supports Max Swapchain Resolution: w={}, h={}", xrSystemProperties.graphicsProperties.maxSwapchainImageWidth, xrSystemProperties.graphicsProperties.maxSwapchainImageHeight));
	logPrint(std::format(" - [Left] Max View Resolution: w={}, h={} with {} samples", xrViewConf[0].maxImageRectWidth, xrViewConf[0].maxImageRectHeight, xrViewConf[0].maxSwapchainSampleCount));
	logPrint(std::format(" - [Right] Max View Resolution: w={}, h={}  with {} samples", xrViewConf[1].maxImageRectWidth, xrViewConf[1].maxImageRectHeight, xrViewConf[1].maxSwapchainSampleCount));
	logPrint(std::format(" - [Left] Recommended View Resolution: w={}, h={}  with {} samples", xrViewConf[0].recommendedImageRectWidth, xrViewConf[0].recommendedImageRectHeight, xrViewConf[0].recommendedSwapchainSampleCount));
	logPrint(std::format(" - [Right] Recommended View Resolution: w={}, h={}  with {} samples", xrViewConf[1].recommendedImageRectWidth, xrViewConf[1].recommendedImageRectHeight, xrViewConf[0].recommendedSwapchainSampleCount));
}

void XR_deinitInstance() {
	checkAssert(xrSharedInstance != XR_NULL_HANDLE);
	
	checkXRResult(xrDestroyInstance(xrSharedInstance), "Couldn't destroy xr instance!");
	if (xrDebugMessengerHandle != XR_NULL_HANDLE) {
		checkXRResult(func_xrDestroyDebugUtilsMessengerEXT(xrDebugMessengerHandle), "Couldn't destroy debug messenger!");
		xrDebugMessengerHandle = XR_NULL_HANDLE;
	}
	XR_deinitPointers();
	xrSharedInstance = XR_NULL_HANDLE;
	xrSharedSystemId = XR_NULL_SYSTEM_ID;
	xrMaxSwapchainWidth = 0;
	xrMaxSwapchainHeight = 0;
	xrSupportsOrientational = false;
	xrSupportsPositional = false;
}

std::array<XrViewConfigurationView, 2> XR_CreateViewConfiguration() {
	logPrint("Creating the OpenXR view configuration...");

	XrViewConfigurationProperties stereoViewConfiguration = { XR_TYPE_VIEW_CONFIGURATION_PROPERTIES };
	checkXRResult(xrGetViewConfigurationProperties(xrSharedInstance, xrSharedSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &stereoViewConfiguration), "There's no VR headset available that allows stereo rendering!");
	xrSupportsFovMutable = stereoViewConfiguration.fovMutable;

	uint32_t eyeViewsConfigurationCount = 0;
	checkXRResult(xrEnumerateViewConfigurationViews(xrSharedInstance, xrSharedSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &eyeViewsConfigurationCount, nullptr), "Can't get number of individual views for stereo view available");
	if (eyeViewsConfigurationCount != 2) {
		throw std::runtime_error(std::format("Unexpected amount of views for the stereo configuration? This is not supported! {}", eyeViewsConfigurationCount));
	}
	std::array<XrViewConfigurationView, 2> xrViewConf = { XrViewConfigurationView{ XR_TYPE_VIEW_CONFIGURATION_VIEW } , XrViewConfigurationView{ XR_TYPE_VIEW_CONFIGURATION_VIEW } };
	checkXRResult(xrEnumerateViewConfigurationViews(xrSharedInstance, xrSharedSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, eyeViewsConfigurationCount, &eyeViewsConfigurationCount, xrViewConf.data()), "Can't get individual views for stereo view available!");
	
	logPrint("Successfully created the OpenXR view configuration!");
	return xrViewConf;
}

void XR_GetSupportedAdapter(D3D_FEATURE_LEVEL* minFeatureLevel, LUID* adapterLUID) {
	logPrint("Getting OpenXR-supported D3D12 adapter...");
	checkAssert(xrSharedInstance != XR_NULL_HANDLE);

	XrGraphicsRequirementsD3D12KHR xrGraphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
	checkXRResult(func_xrGetD3D12GraphicsRequirementsKHR(xrSharedInstance, xrSharedSystemId, &xrGraphicsRequirements), "Couldn't get D3D12 requirements for the given VR headset!");
	logPrint(std::format("OpenXR implementation requires D3D12 {}.{}", ((xrGraphicsRequirements.minFeatureLevel >> (8 * 0)) & 0xff), ((xrGraphicsRequirements.minFeatureLevel >> (8 * 1)) & 0xff)));

	*minFeatureLevel = xrGraphicsRequirements.minFeatureLevel;
	*adapterLUID = xrGraphicsRequirements.adapterLuid;
}

XrSession XR_CreateSession(XrGraphicsBindingD3D12KHR& d3d12Binding) {
	logPrint("Creating the OpenXR session...");

	XrSessionCreateInfo sessionCreateInfo = { XR_TYPE_SESSION_CREATE_INFO };
	sessionCreateInfo.systemId = xrSharedSystemId;
	sessionCreateInfo.next = &d3d12Binding;
	sessionCreateInfo.createFlags = 0;

	checkXRResult(xrCreateSession(xrSharedInstance, &sessionCreateInfo, &xrSharedSession), "Failed to create Vulkan-based OpenXR session!");
	
	logPrint("Successfully created the OpenXR session!");
	return xrSharedSession;
}

XrSpace XR_CreateSpace() {
	constexpr XrPosef xrIdentityPose = { .orientation = {.x=0,.y=0,.z=0,.w=1}, .position = {.x=0,.y=0,.z=0} };

	XrReferenceSpaceCreateInfo spaceCreateInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	spaceCreateInfo.poseInReferenceSpace = xrIdentityPose;
	checkXRResult(xrCreateReferenceSpace(xrSharedSession, &spaceCreateInfo, &xrSpace), "Failed to create reference space!");
	
	logPrint("Successfully created OpenXR reference space!");
	return xrSpace;
}

void XR_BeginSession() {
	XrSessionBeginInfo sessionBeginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
	sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	checkXRResult(xrBeginSession(xrSharedSession, &sessionBeginInfo), "Failed to begin OpenXR session!");
	logPrint("Successfully begun OpenXR session!");
}