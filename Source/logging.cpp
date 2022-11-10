#include "layer.h"

HANDLE consoleHandle = NULL;
double timeFrequency;

#ifndef _DEBUG
#define _DEBUG
#endif

void logInitialize() {
#ifdef _DEBUG
	AllocConsole();
	SetConsoleTitleA("BetterVR Debugging Console");
	consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
	logPrint("Successfully started BetterVR!");
	LARGE_INTEGER timeLI;
	QueryPerformanceFrequency(&timeLI);
	timeFrequency = double(timeLI.QuadPart) / 1000.0;
}

void logShutdown() {
	logPrint("Shutting down BetterVR application...");
#ifdef _DEBUG
	FreeConsole();
#endif
}

void logPrint(const std::string_view& message_view) {
#ifdef _DEBUG
	std::string message(message_view);
	message += "\n";
	DWORD charsWritten = 0;
	WriteConsoleA(consoleHandle, message.c_str(), (DWORD)message.size(), &charsWritten, NULL);
	OutputDebugStringA(message.c_str());
#endif
}

void logPrint(const char* message) {
	std::string stringLine(message);
	logPrint(stringLine);
}

void logTimeElapsed(char* prefixMessage, LARGE_INTEGER time) {
	LARGE_INTEGER timeNow;
	QueryPerformanceCounter(&timeNow);
	logPrint((std::string(prefixMessage) + std::to_string(double(time.QuadPart - timeNow.QuadPart) / timeFrequency) + std::string("ms")).c_str());
}

void checkXRResult(XrResult result, const char* errorMessage) {
	XrResult resultCheck = result;
	if (XR_FAILED(result)) {
		if (errorMessage == nullptr) {
#ifdef _DEBUG
			logPrint(std::format("[Error] An unknown error (result was {}) has occurred!", (std::underlying_type_t<XrResult>)result));
			__debugbreak();
#endif
			MessageBoxA(NULL, std::format("An unknown error {} has occurred which caused a fatal crash!", (std::underlying_type_t<XrResult>)result).c_str(), "An error occurred!", MB_OK | MB_ICONERROR);
			throw std::runtime_error("Undescribed error occurred!");
		}
		else {
#ifdef _DEBUG
			logPrint(std::format("[Error] Error {}: {}", (std::underlying_type_t<XrResult>)result, errorMessage));
			__debugbreak();
#endif
			MessageBoxA(NULL, errorMessage, "A fatal error occurred!", MB_OK | MB_ICONERROR);
			throw std::runtime_error(errorMessage);
		}
	}
}

void checkHResult(HRESULT result, const char* errorMessage) {
	if (FAILED(result)) {
		if (errorMessage == nullptr) {
#ifdef _DEBUG
			logPrint(std::format("[Error] An unknown error (result was {}) has occurred!", result));
			__debugbreak();
#endif
			MessageBoxA(NULL, std::format("An unknown error {} has occurred which caused a fatal crash!", result).c_str(), "A fatal error occurred!", MB_OK | MB_ICONERROR);
			throw std::runtime_error("Undescribed error occurred!");
		}
		else {
#ifdef _DEBUG
			logPrint(std::format("[Error] Error {}: {}", result, errorMessage));
			__debugbreak();
#endif
			MessageBoxA(NULL, errorMessage, "A fatal error occurred!", MB_OK | MB_ICONERROR);
			throw std::runtime_error(errorMessage);
		}
	}
}

void checkVkResult(VkResult result, const char* errorMessage) {
	if (result != VK_SUCCESS) {
		if (errorMessage == nullptr) {
#ifdef _DEBUG
			__debugbreak();
			logPrint(std::format("[Error] An unknown error (result was {}) has occurred!", (std::underlying_type_t<VkResult>)result));
#endif
			MessageBoxA(NULL, std::format("An unknown error {} has occurred which caused a fatal crash!", (std::underlying_type_t<VkResult>)result).c_str(), "A fatal error occurred!", MB_OK | MB_ICONERROR);
			throw std::runtime_error("Undescribed error occurred!");
		}
		else {
#ifdef _DEBUG
			logPrint(std::format("[Error] Error {}: {}", (std::underlying_type_t<VkResult>)result, errorMessage));
			__debugbreak();
#endif
			MessageBoxA(NULL, errorMessage, "A fatal error occurred!", MB_OK | MB_ICONERROR);
			throw std::runtime_error(errorMessage);
		}
	}
}

void checkAssert(bool assert, const char* errorMessage) {
	if (!assert) {
		if (errorMessage == nullptr) {
#ifdef _DEBUG
			__debugbreak();
			logPrint("[Error] Something unexpected happened that prevents further execution!");
#endif
			MessageBoxA(NULL, "Something unexpected happened that prevents further execution!", "A fatal error occurred!", MB_OK | MB_ICONERROR);
			throw std::runtime_error("Unexpected assertion occurred!");
		}
		else {
#ifdef _DEBUG
			logPrint(std::format("[Error] {}", errorMessage));
			__debugbreak();
#endif
			MessageBoxA(NULL, errorMessage, "A fatal error occurred!", MB_OK | MB_ICONERROR);
			throw std::runtime_error(errorMessage);
		}
	}
}

#undef _DEBUG