#include "instance.h"
#include "cemu_hooks.h"
#include "rendering/openxr.h"


void CemuHooks::hook_BeginCameraSide(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    OpenXR::EyeSide side = hCPU->gpr[0] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;

    Log::print("");
    Log::print("===============================================================================");
    Log::print("{0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0}", side == OpenXR::EyeSide::LEFT ? "LEFT" : "RIGHT");

    bool layersInitialized = VRManager::instance().XR->GetRenderer()->m_layer3D && VRManager::instance().XR->GetRenderer()->m_layer2D;
    if (layersInitialized && side == OpenXR::EyeSide::LEFT) {
        if (VRManager::instance().VK->m_imguiOverlay) {
            VRManager::instance().VK->m_imguiOverlay->BeginFrame();
            VRManager::instance().VK->m_imguiOverlay->Update();
        }
        VRManager::instance().XR->GetRenderer()->StartFrame();
    }
}

// TODO FOR NEXT TIME: We change GetRenderCamera() in all cases, but this apparently even causes the camera to be based off of it despite us changing the
// TODO FOR NEXT TIME: Should see if we could disable the double input pulling. We could do it only for the right frame.
// TODO FOR NEXT TIME: I actually wrote down a bunch of addresses for the camera following. At least hooking these to have a camera that is rotated would solve the issue of walking without a head.

// 0x02b8f508 = li r3, 0
// 0x02B8F508 = li r3, 0
// 0x2B9B930 = li r3, 0

static std::pair<glm::quat, glm::quat> swingTwistY(const glm::quat& q) {
    glm::vec3 yAxis(0, 1, 0);
    // Project rotation axis onto Y to get twist
    glm::vec3 r(q.x, q.y, q.z);
    float dot = glm::dot(r, yAxis);
    glm::vec3 proj = yAxis * dot;
    glm::quat twist = glm::normalize(glm::quat(q.w, proj.x, proj.y, proj.z));
    glm::quat swing = q * glm::conjugate(twist);
    return { swing, twist };
}

glm::fvec3 s_wsCameraPosition = glm::fvec3();
glm::fquat s_wsCameraRotation = glm::identity<glm::fquat>();

void CemuHooks::hook_UpdateCameraForGameplay(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    // Read the camera matrix from the game's memory
    uint32_t ppc_cameraMatrixOffsetIn = hCPU->gpr[31];
    OpenXR::EyeSide ppc_cameraSide = hCPU->gpr[3] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;
    ActCamera actCam = {};
    readMemory(ppc_cameraMatrixOffsetIn, &actCam);

    // Existing in-game camera matrix
    glm::fvec3 oldCameraPosition = actCam.finalCamMtx.pos.getLE();
    glm::fvec3 oldCameraTarget = actCam.finalCamMtx.target.getLE();
    glm::fvec3 oldCameraForward = glm::normalize(oldCameraTarget - oldCameraPosition);
    glm::fvec3 oldCameraUp = actCam.finalCamMtx.up.getLE();
    glm::fvec3 oldCameraUnknown = actCam.finalCamMtx.unknown.getLE();    
    float extraValue0 = actCam.finalCamMtx.zNear.getLE();
    float extraValue1 = actCam.finalCamMtx.zFar.getLE();

    //Log::print("!! Existing Camera Address:  {:08X}", ppc_cameraMatrixOffsetIn + (uint32_t)offsetof(ActCamera, finalCamMtx.up));
    //Log::print("!! Existing Camera Position: {}", oldCameraPosition);
    //Log::print("!! Existing Camera Target:   {}", oldCameraTarget);
    //Log::print("!! Existing Camera Up:       {}", oldCameraUp);
    //Log::print("!! Existing Camera Unknown:  {}", oldCameraUnknown);
    //Log::print("!! Existing Camera ZNear:    {}, ZFar: {}", extraValue0, extraValue1);

    oldCameraPosition.y = oldCameraTarget.y;


    glm::mat4 existingGameMtx = glm::lookAtRH(oldCameraPosition, oldCameraTarget, oldCameraUp);

    s_wsCameraPosition = oldCameraPosition;
    s_wsCameraRotation = glm::quat_cast(glm::inverse(existingGameMtx));

    // rebase the rotation to the player position
    if (GetSettings().IsFirstPersonMode()) {
        BEMatrix34 mtx = {};
        readMemory(s_playerMtxAddress, &mtx);
        glm::fvec3 playerPos = mtx.getPos().getLE();

        glm::mat4 playerMtx4 = glm::inverse(glm::translate(glm::identity<glm::mat4>(), playerPos) * glm::mat4(s_wsCameraRotation));
        existingGameMtx = playerMtx4;
    }

    // Current VR headset camera matrix
    auto viewsOpt = VRManager::instance().XR->GetRenderer()->GetMiddlePose();
    if (!viewsOpt) {
        Log::print("[ERROR] hook_updateCamera: No views available for the middle pose.");
        return;
    }
    auto& views = viewsOpt.value();

    // Corrected code
    glm::mat4 finalPose = glm::inverse(existingGameMtx) * views;

    glm::fvec3 camPos = glm::fvec3(finalPose[3]);
    glm::fvec3 forward = -glm::normalize(glm::fvec3(finalPose[2]));
    glm::fvec3 up = glm::normalize(glm::fvec3(finalPose[1]));

    float oldCameraDistance = glm::distance(oldCameraPosition, oldCameraTarget);
    glm::fvec3 target = camPos + forward * oldCameraDistance;

    actCam.finalCamMtx.pos = camPos;
    actCam.finalCamMtx.target = target;
    actCam.finalCamMtx.up = up;
    //actCam.finalCamMtx.up = glm::fvec3(0.0f, 1.0f, 0.0f);

    // Write the camera matrix to the game's memory
    uint32_t ppc_cameraMatrixOffsetOut = hCPU->gpr[31];
    writeMemory(ppc_cameraMatrixOffsetOut, &actCam);
    s_framesSinceLastCameraUpdate = 0;
}

void CemuHooks::hook_UpdateCameraRotation(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;
    // Log::print("[{}] Updated camera rotation", s_currentCameraRotation.second == OpenXR::EyeSide::LEFT ? "left" : "right");

    uint32_t ppc_cameraMatrixOffsetIn = hCPU->gpr[31];
    LookAtMatrix actCam = {};
    readMemory(ppc_cameraMatrixOffsetIn, &actCam);

    //writeMemory(ppc_cameraMatrixOffsetIn, &actCam);
}

glm::mat4 CemuHooks::s_lastCameraMtx = glm::mat4(1.0f);


// ========== Camera Rotation Control ==========
XrTime s_lastTimestamp = -1;
constexpr float kYawSpeed = glm::radians(60.f);
constexpr float kDeadzone = 0.3f;

inline float SecondsBetween(XrTime a, XrTime b) { return static_cast<float>(a - b) * 1e-9f; }
inline glm::vec2 WithDeadzone(glm::vec2 v) { return (glm::length(v) < kDeadzone) ? glm::vec2(0) : v; }


void CemuHooks::hook_CameraRotationControl(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    // set r3 to 0 so that the normal camera chase isn't ran
    hCPU->gpr[3] = 0;

    auto input = VRManager::instance().XR->m_input.load();
    if (!input.inGame.in_game) return;

    const glm::vec2 stick = WithDeadzone(ToGLM(input.inGame.camera.currentState));
    const XrTime now = input.inGame.inputTime;

    if (s_lastTimestamp == -1) {
        s_lastTimestamp = now;
        return;
    }
    const float dt = SecondsBetween(now, s_lastTimestamp);
    s_lastTimestamp = now;
    if (dt <= 0) return;

    const glm::fquat qYaw = glm::angleAxis(-stick.x * kYawSpeed * dt, glm::vec3(0.f, 1.f, 0.f));

    //VRManager::instance().XR->m_inputCameraRotation = glm::normalize(VRManager::instance().XR->m_inputCameraRotation.load() * qYaw);
}

void CemuHooks::hook_ReplaceCameraMode(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t currentCameraMode = hCPU->gpr[3];
    uint32_t cameraTailMode = hCPU->gpr[4]; // works best in VR since it ignores the pivot of the camera
    uint32_t currentCameraVtbl = hCPU->gpr[5];

    constexpr uint32_t kCameraChaseVtbl = 0x101B34F4;

    if (hCPU->gpr[5] == kCameraChaseVtbl) {
        //Log::print("Current camera mode: {:#X}, tail mode: {:#X}, vtbl: {:#X}", currentCameraMode, cameraTailMode, currentCameraVtbl);
        if (GetSettings().IsFirstPersonMode()) {
            // overwrite to tail mode
            //hCPU->gpr[3] = cameraTailMode;
        }
    }
}
// ============================================


void CemuHooks::hook_GetRenderCamera(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;
    uint32_t cameraIn = hCPU->gpr[3];
    uint32_t cameraOut = hCPU->gpr[12];
    OpenXR::EyeSide cameraSide = hCPU->gpr[11] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;

    BESeadLookAtCamera camera = {};
    readMemory(cameraIn, &camera);

    Log::print("[PPC] Getting render camera for {} side", cameraSide == OpenXR::EyeSide::LEFT ? "left" : "right");

    //s_lastCameraMtx = glm::fmat4x3(glm::inverse(glm::mat4(camera.mtx.getLEMatrix()))); // glm::inverse(glm::lookAtRH(camera.pos.getLE(), camera.at.getLE(), camera.up.getLE()));

    // in-game camera
    glm::mat4x3 viewMatrix = camera.mtx.getLEMatrix();
    glm::mat4 worldGame = glm::inverse(glm::mat4(viewMatrix));
    glm::vec3 basePos = glm::vec3(worldGame[3]);
    glm::quat baseRot = glm::quat_cast(worldGame);

    // overwrite with our stored camera pos/rot
    basePos = s_wsCameraPosition;
    baseRot = s_wsCameraRotation;
    auto [swing, baseYaw] = swingTwistY(baseRot);

    s_lastCameraMtx = glm::fmat4x3(glm::translate(glm::identity<glm::fmat4>(), basePos) * glm::mat4(baseRot));



    // take link's direction, then rotate the headset position
    BEMatrix34 mtx = {};
    readMemory(s_playerMtxAddress, &mtx);
    glm::fvec3 playerPos = mtx.getPos().getLE();
    glm::fquat playerRot = mtx.getRotLE();

    // vr camera
    std::optional<XrPosef> currPoseOpt = VRManager::instance().XR->GetRenderer()->GetPose(cameraSide);
    if (!currPoseOpt.has_value())
        return;
    glm::fvec3 eyePos = ToGLM(currPoseOpt.value().position);
    glm::fquat eyeRot = ToGLM(currPoseOpt.value().orientation);

    if (GetSettings().IsFirstPersonMode()) {
        basePos = playerPos;
    }

    glm::vec3 newPos = basePos + (baseYaw * eyePos);
    glm::fquat newRot = baseYaw * eyeRot;


    glm::mat4 newWorldVR = glm::translate(glm::mat4(1.0f), newPos) * glm::mat4_cast(newRot);
    glm::mat4 newViewVR = glm::inverse(newWorldVR);

    camera.mtx.setLEMatrix(newViewVR);

    camera.pos = newPos;

    // Set look-at point by offsetting position in view direction
    glm::vec3 viewDir = -glm::vec3(newViewVR[2]); // Forward direction is -Z in view space
    camera.at = newPos + viewDir;

    // Transform world up vector by new rotation
    glm::vec3 upDir = glm::vec3(newViewVR[1]); // Up direction is +Y in view space
    camera.up = upDir;


    //glm::mat4 workingMtx = glm::inverse(glm::lookAtRH(newPos, newPos + glm::vec3(newViewVR[2]), glm::fvec3(0, 1, 0)));
    //s_lastCameraMtx = workingMtx;

    writeMemory(cameraOut, &camera);
    hCPU->gpr[3] = cameraOut;
}

constexpr uint32_t seadOrthoProjection = 0x1027B5BC;
constexpr uint32_t seadPerspectiveProjection = 0x1027B54C;


// https://github.com/KhronosGroup/OpenXR-SDK/blob/858912260ca616f4c23f7fb61c89228c353eb124/src/common/xr_linear.h#L564C1-L632C2
// https://github.com/aboood40091/sead/blob/45b629fb032d88b828600a1b787729f2d398f19d/engine/library/modules/src/gfx/seadProjection.cpp#L166

static data_VRProjectionMatrixOut calculateFOVAndOffset(XrFovf viewFOV) {
    float totalHorizontalFov = viewFOV.angleRight - viewFOV.angleLeft;
    float totalVerticalFov = viewFOV.angleUp - viewFOV.angleDown;

    float aspectRatio = totalHorizontalFov / totalVerticalFov;
    float fovY = totalVerticalFov;
    float projectionCenter_offsetX = (viewFOV.angleRight + viewFOV.angleLeft) / 2.0f;
    float projectionCenter_offsetY = (viewFOV.angleUp + viewFOV.angleDown) / 2.0f;

    data_VRProjectionMatrixOut ret = {};
    ret.aspectRatio = aspectRatio;
    ret.fovY = fovY;
    ret.offsetX = projectionCenter_offsetX;
    ret.offsetY = projectionCenter_offsetY;

    return ret;
}

static glm::mat4 calculateProjectionMatrix(float nearZ, float farZ, const XrFovf& fov) {
    float l = tanf(fov.angleLeft) * nearZ;
    float r = tanf(fov.angleRight) * nearZ;
    float b = tanf(fov.angleDown) * nearZ;
    float t = tanf(fov.angleUp) * nearZ;

    float invW = 1.0f / (r - l);
    float invH = 1.0f / (t - b);
    float invD = 1.0f / (farZ - nearZ);

    glm::mat4 dst = {};
    dst[0][0] = 2.0f * nearZ * invW;
    dst[1][1] = 2.0f * nearZ * invH;
    dst[0][2] = (r + l) * invW;
    dst[1][2] = (t + b) * invH;
    dst[2][2] = -(farZ + nearZ) * invD;
    dst[2][3] = -(2.0f * farZ * nearZ) * invD;
    dst[3][2] = -1.0f;
    dst[3][3] = 0.0f;

    return dst;
}

void CemuHooks::hook_GetRenderProjection(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t projectionIn = hCPU->gpr[3];
    uint32_t projectionOut = hCPU->gpr[12];
    OpenXR::EyeSide side = hCPU->gpr[0] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;

    BESeadPerspectiveProjection perspectiveProjection = {};
    readMemory(projectionIn, &perspectiveProjection);

    if (perspectiveProjection.zFar == 10000.0f) {
        return;
    }

    perspectiveProjection.zFar = GetSettings().GetZFar();
    perspectiveProjection.zNear = GetSettings().GetZNear();

    // Log::print("Render Proj. (LR: {:08X}): {}", hCPU->sprNew.LR, perspectiveProjection);
    // Log::print("[PPC] Getting render projection for {} side", side == OpenXR::EyeSide::LEFT ? "left" : "right");

    if (!VRManager::instance().XR->GetRenderer()->GetFOV(side).has_value()) {
        return;
    }
    XrFovf currFOV = VRManager::instance().XR->GetRenderer()->GetFOV(side).value();
    auto newProjection = calculateFOVAndOffset(currFOV);

    perspectiveProjection.aspect = newProjection.aspectRatio;
    perspectiveProjection.fovYRadiansOrAngle = newProjection.fovY;
    float halfAngle = newProjection.fovY.getLE() * 0.5f;
    perspectiveProjection.fovySin = sinf(halfAngle);
    perspectiveProjection.fovyCos = cosf(halfAngle);
    perspectiveProjection.fovyTan = tanf(halfAngle);
    perspectiveProjection.offset.x = newProjection.offsetX;
    perspectiveProjection.offset.y = newProjection.offsetY;

    glm::fmat4 newMatrix = calculateProjectionMatrix(perspectiveProjection.zNear.getLE(), perspectiveProjection.zFar.getLE(), currFOV);
    perspectiveProjection.matrix = newMatrix;

    // calculate device matrix
    glm::fmat4 newDeviceMatrix = newMatrix;

    float zScale = perspectiveProjection.deviceZScale.getLE();
    float zOffset = perspectiveProjection.deviceZOffset.getLE();

    newDeviceMatrix[2][0] *= zScale;
    newDeviceMatrix[2][1] *= zScale;
    newDeviceMatrix[2][2] = (newDeviceMatrix[2][2] + newDeviceMatrix[3][2] * zOffset) * zScale;
    newDeviceMatrix[2][3] = newDeviceMatrix[2][3] * zScale + newDeviceMatrix[3][3] * zOffset;

    perspectiveProjection.deviceMatrix = newDeviceMatrix;

    perspectiveProjection.dirty = false;
    perspectiveProjection.deviceDirty = false;

    writeMemory(projectionOut, &perspectiveProjection);
    hCPU->gpr[3] = projectionOut;
}


void CemuHooks::hook_ModifyLightPrePassProjectionMatrix(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t projectionIn = hCPU->gpr[3];
    OpenXR::EyeSide side = hCPU->gpr[11] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;

    BESeadPerspectiveProjection perspectiveProjection = {};
    readMemory(projectionIn, &perspectiveProjection);

    if (!VRManager::instance().XR->GetRenderer()->GetFOV(side).has_value()) {
        return;
    }

    XrFovf currFOV = VRManager::instance().XR->GetRenderer()->GetFOV(side).value();
    auto newProjection = calculateFOVAndOffset(currFOV);

    perspectiveProjection.aspect = newProjection.aspectRatio;
    perspectiveProjection.fovYRadiansOrAngle = newProjection.fovY;
    float halfAngle = newProjection.fovY.getLE() * 0.5f;
    perspectiveProjection.fovySin = sinf(halfAngle);
    perspectiveProjection.fovyCos = cosf(halfAngle);
    perspectiveProjection.fovyTan = tanf(halfAngle);
    perspectiveProjection.offset.x = newProjection.offsetX;
    perspectiveProjection.offset.y = newProjection.offsetY;

    glm::fmat4 newMatrix = calculateProjectionMatrix(perspectiveProjection.zNear.getLE(), perspectiveProjection.zFar.getLE(), currFOV);
    perspectiveProjection.matrix = newMatrix;

    // calculate device matrix
    glm::fmat4 newDeviceMatrix = newMatrix;

    float zScale = perspectiveProjection.deviceZScale.getLE();
    float zOffset = perspectiveProjection.deviceZOffset.getLE();

    newDeviceMatrix[2][0] *= zScale;
    newDeviceMatrix[2][1] *= zScale;
    newDeviceMatrix[2][2] = (newDeviceMatrix[2][2] + newDeviceMatrix[3][2] * zOffset) * zScale;
    newDeviceMatrix[2][3] = newDeviceMatrix[2][3] * zScale + newDeviceMatrix[3][3] * zOffset;

    perspectiveProjection.deviceMatrix = newDeviceMatrix;

    perspectiveProjection.dirty = false;
    perspectiveProjection.deviceDirty = false;

    writeMemory(projectionIn, &perspectiveProjection);
}

void CemuHooks::hook_FixSomeCamerasForGameplayReasons(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t seadCameraPtr = hCPU->gpr[3];
    uint32_t currentEyeSide = hCPU->gpr[4];
    uint32_t parentCalleeLr = hCPU->gpr[5];
    uint32_t alreadyModifiedCameraPtr = hCPU->gpr[6];

    BESeadLookAtCamera camera = {};
    readMemory(seadCameraPtr, &camera);

    //Log::print("!! FixSomeCamerasForGameplayReasons: EyeSide = {}, Parent LR = {:08X}", currentEyeSide, parentCalleeLr);

    if (parentCalleeLr == 0x02C65784) {
    }
    //hCPU->gpr[3] = alreadyModifiedCameraPtr;
}

//float previousAddedAngle = 0.0f;
void CemuHooks::hook_ApplyCameraRotation(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    // float degrees = hCPU->fpr[1].fp0;
    // float addedAngle = degrees - previousAddedAngle;
    // previousAddedAngle = degrees;
    //
    // // rotate the camera with whatever the OpenXR rotation is
    // XrPosef currFOV = VRManager::instance().XR->GetRenderer()->GetPose(OpenXR::EyeSide::LEFT).value();
    // glm::fquat currQuat = ToGLM(currFOV.orientation);
    // glm::fquat rotateHorizontalCounter = glm::angleAxis(glm::radians(addedAngle), glm::fvec3(0.0f, 1.0f, 0.0f));
    // currQuat = rotateHorizontalCounter * currQuat;
    //
    // float rotatedDegrees = glm::degrees(glm::atan(currQuat.x, currQuat.w));
    //
    // hCPU->fpr[1].fp0 = rotatedDegrees;
}

void CemuHooks::hook_EndCameraSide(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    OpenXR::EyeSide side = hCPU->gpr[3] == 0 ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT;

    // todo: sometimes this can deadlock apparently?
    if (VRManager::instance().XR->GetRenderer()->IsInitialized() && side == OpenXR::EyeSide::RIGHT) {
        VRManager::instance().XR->GetRenderer()->EndFrame();
        CemuHooks::m_heldWeaponsLastUpdate[0] = CemuHooks::m_heldWeaponsLastUpdate[0]++;
        CemuHooks::m_heldWeaponsLastUpdate[1] = CemuHooks::m_heldWeaponsLastUpdate[1]++;
        if (CemuHooks::m_heldWeaponsLastUpdate[0] >= 6) {
            CemuHooks::m_heldWeapons[0] = 0;
        }
        if (CemuHooks::m_heldWeaponsLastUpdate[1] >= 6) {
            CemuHooks::m_heldWeapons[1] = 0;
        }
    }

    Log::print("{0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0} {0}", side == OpenXR::EyeSide::LEFT ? "LEFT" : "RIGHT");
    Log::print("===============================================================================");
    Log::print("");
}

void CemuHooks::hook_UseCameraDistance(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    if (GetSettings().IsFirstPersonMode()) {
        hCPU->fpr[13].fp0 = 0.0f;
    }
    else {
        hCPU->fpr[13].fp0 = hCPU->fpr[10].fp0;
    }
}

constexpr uint32_t playerVTable = 0x101E5FFC;

void CemuHooks::hook_SetActorOpacity(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    //uint32_t sceneStatus = getMemory<uint32_t>(0x1046F57C).getLE();
    //Log::print("!! scene status {}", sceneStatus);

    double toBeSetOpacity = hCPU->fpr[1].fp0;
    uint32_t actorPtr = hCPU->gpr[3];

    ActorWiiU actor;
    readMemory(actorPtr, &actor);

    float actual_transparency = 1.0f;
    //Log::print("!! Currently held weapon ptrs: {:08X} {:08X}", CemuHooks::m_heldWeapons[0], CemuHooks::m_heldWeapons[1]);

    if (GetSettings().IsFirstPersonMode() && false) {
        float modelOpacity = actor.modelOpacity.getLE();
        float startModelOpacity = actor.startModelOpacity.getLE();
        uint32_t modelOpacityRelated = (uint32_t)actor.modelOpacityRelated.getLE();
        uint8_t opacityOrDoFlushOpacityToGPU = actor.opacityOrDoFlushOpacityToGPU;

        Log::print("!! Set opacity to {} for actor {} (currently opacity={}, startModel={}, related={}, toFlush={})", toBeSetOpacity, actor.name.getLE(), modelOpacity, startModelOpacity, modelOpacityRelated, opacityOrDoFlushOpacityToGPU);

        uint32_t actorVTable = actor.vtable.getLE();
        if (actorVTable == playerVTable) {
            //Log::print("[PPC] Setting player opacity to {} for actor at {:08X}", toBeSetOpacity, actorPtr);
            toBeSetOpacity = 1.0;
            float negativeOpacity = 0;
            uint8_t opacityOrDoFlushOpacityToGPU = 0;
            writeMemoryBE(actorPtr + offsetof(ActorWiiU, startModelOpacity), &toBeSetOpacity);
            writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacity), &toBeSetOpacity);
            writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacityRelated), &negativeOpacity);
            writeMemoryBE(actorPtr + offsetof(ActorWiiU, opacityOrDoFlushOpacityToGPU), &opacityOrDoFlushOpacityToGPU);
            return;
        }


        if (m_heldWeapons[0] == actorPtr || m_heldWeapons[1] == actorPtr) {
            toBeSetOpacity = 1.0;
            float negativeOpacity = 0;
            uint8_t opacityOrDoFlushOpacityToGPU = 1;
            writeMemoryBE(actorPtr + offsetof(ActorWiiU, startModelOpacity), &toBeSetOpacity);
            writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacity), &toBeSetOpacity);
            writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacityRelated), &negativeOpacity);
            writeMemoryBE(actorPtr + offsetof(ActorWiiU, opacityOrDoFlushOpacityToGPU), &opacityOrDoFlushOpacityToGPU);
            return;
        }

        // prevent the game from hiding weapons that clip into the player model or are hidden for some other reason
        if (m_heldWeapons[0] == actorPtr || m_heldWeapons[1] == actorPtr) {
            //if (hCPU->sprNew.LR == 0x024A410C) {
            //    //Log::print("!! PREVENTING setting the weapon opacity to {} for a held weapon {} for actor at {:08X} (LR = {:08X})", toBeSetOpacity, actor.name.getLE(), actorPtr, hCPU->sprNew.LR);
            //    toBeSetOpacity = 1.0;
            //    uint8_t opacityOrDoFlushOpacityToGPU = 0;
            //    writeMemoryBE(actorPtr + offsetof(ActorWiiU, startModelOpacity), &toBeSetOpacity);
            //    writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacity), &toBeSetOpacity);
            //    toBeSetOpacity = 1.0;
            //    writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacityRelated), &toBeSetOpacity);
            //    writeMemoryBE(actorPtr + offsetof(ActorWiiU, opacityOrDoFlushOpacityToGPU), &opacityOrDoFlushOpacityToGPU);
            //    return;
            //}

            //if (hCPU->sprNew.LR == 0x024A4118) {
            //    //Log::print("!! PREVENTING setting the weapon opacity to {} for a held weapon {} for actor at {:08X} (LR = {:08X})", toBeSetOpacity, actor.name.getLE(), actorPtr, hCPU->sprNew.LR);
            //    toBeSetOpacity = 1.0f;
            //    uint8_t opacityOrDoFlushOpacityToGPU = 0;
            //    writeMemoryBE(actorPtr + offsetof(ActorWiiU, startModelOpacity), &toBeSetOpacity);
            //    writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacity), &toBeSetOpacity);
            //    toBeSetOpacity = 1.0;
            //    writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacityRelated), &toBeSetOpacity);
            //    writeMemoryBE(actorPtr + offsetof(ActorWiiU, opacityOrDoFlushOpacityToGPU), &opacityOrDoFlushOpacityToGPU);

            //    //hCPU->fpr[30].fp0 = 0.0f;
            //    //hCPU->fpr[1].fp0 = 1.0f;
            //    //hCPU->instructionPointer = 0x024A4128;
            //}

            //Log::print("!! PREVENTING setting the weapon opacity to {} for a held weapon {} for actor at {:08X} (LR = {:08X})", toBeSetOpacity, actor.name.getLE(), actorPtr, hCPU->sprNew.LR);
            //toBeSetOpacity = 1.0f;
            //uint8_t opacityOrDoFlushOpacityToGPU = 0;
            ////if (hCPU->sprNew.LR == 0x024A6B18) {
            ////    float negativeOpacity = 0.0;
            ////    hCPU->fpr[31].fp0 = 1.0f;
            ////    hCPU->fpr[1].fp0 = 1.0f;
            ////    opacityOrDoFlushOpacityToGPU = 0;
            ////    writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacityRelated), &negativeOpacity);
            ////}
            //writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacity), &toBeSetOpacity);
            //writeMemoryBE(actorPtr + offsetof(ActorWiiU, startModelOpacity), &toBeSetOpacity);
            //writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacityRelated), &toBeSetOpacity);
            //writeMemoryBE(actorPtr + offsetof(ActorWiiU, opacityOrDoFlushOpacityToGPU), &opacityOrDoFlushOpacityToGPU);

            //if (hCPU->sprNew.LR == 0x024A4118) {
            //    hCPU->fpr[30].fp0 = 0.0f;
            //    hCPU->fpr[1].fp0 = 1.0f;
            //    hCPU->instructionPointer = 0x024A4128;
            //}
            return;
        }
    }

    // normal behavior if it wasn't the player or a held weapon
    if (actor.modelOpacity.getLE() != toBeSetOpacity) {
        uint8_t opacityOrDoFlushOpacityToGPU = 1;
        writeMemoryBE(actorPtr + offsetof(ActorWiiU, modelOpacity), &toBeSetOpacity);
        writeMemoryBE(actorPtr + offsetof(ActorWiiU, opacityOrDoFlushOpacityToGPU), &opacityOrDoFlushOpacityToGPU);
    }
}

void CemuHooks::hook_GetEventName(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    uint32_t isEventActive = hCPU->gpr[3];
    if (isEventActive) {
        uint32_t stringPtr = getMemory<uint32_t>(hCPU->gpr[4]).getLE();
        const char* eventNamePtr = (const char*)(hCPU->gpr[4] + s_memoryBaseAddress);
        Log::print("!! Event name {} from {:08X}", eventNamePtr, hCPU->gpr[4]);
        // shrine going down is Demo008_2
        // camera zoom when opening door is Demo024_0
    }
    else {
        //Log::print("!! There's no active event");
    }
}
