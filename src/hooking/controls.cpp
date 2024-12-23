#include "cemu_hooks.h"
#include "../instance.h"

enum VPADButtons : uint32_t {
    VPAD_BUTTON_A                 = 0x8000,
    VPAD_BUTTON_B                 = 0x4000,
    VPAD_BUTTON_X                 = 0x2000,
    VPAD_BUTTON_Y                 = 0x1000,
    VPAD_BUTTON_LEFT              = 0x0800,
    VPAD_BUTTON_RIGHT             = 0x0400,
    VPAD_BUTTON_UP                = 0x0200,
    VPAD_BUTTON_DOWN              = 0x0100,
    VPAD_BUTTON_ZL                = 0x0080,
    VPAD_BUTTON_ZR                = 0x0040,
    VPAD_BUTTON_L                 = 0x0020,
    VPAD_BUTTON_R                 = 0x0010,
    VPAD_BUTTON_PLUS              = 0x0008,
    VPAD_BUTTON_MINUS             = 0x0004,
    VPAD_BUTTON_HOME              = 0x0002,
    VPAD_BUTTON_SYNC              = 0x0001,
    VPAD_BUTTON_STICK_R           = 0x00020000,
    VPAD_BUTTON_STICK_L           = 0x00040000,
    VPAD_BUTTON_TV                = 0x00010000,
    VPAD_STICK_R_EMULATION_LEFT   = 0x04000000,
    VPAD_STICK_R_EMULATION_RIGHT  = 0x02000000,
    VPAD_STICK_R_EMULATION_UP     = 0x01000000,
    VPAD_STICK_R_EMULATION_DOWN   = 0x00800000,
    VPAD_STICK_L_EMULATION_LEFT   = 0x40000000,
    VPAD_STICK_L_EMULATION_RIGHT  = 0x20000000,
    VPAD_STICK_L_EMULATION_UP     = 0x10000000,
    VPAD_STICK_L_EMULATION_DOWN   = 0x08000000,
};

struct BEDir {
    BEVec3 x;
    BEVec3 y;
    BEVec3 z;

    BEDir() = default;
};

struct BETouchData {
    BEType<uint16_t> x;
    BEType<uint16_t> y;
    BEType<uint16_t> touch;
    BEType<uint16_t> validity;
};

struct VPADStatus {
    BEType<uint32_t> hold;
    BEType<uint32_t> trig;
    BEType<uint32_t> release;
    BEVec2 leftStick;
    BEVec2 rightStick;
    BEVec3 acc;
    BEType<float> accMagnitude;
    BEType<float> accAcceleration;
    BEVec2 accXY;
    BEVec3 gyroChange;
    BEVec3 gyroOrientation;
    int8_t vpadErr;
    uint8_t padding1[1];
    BETouchData tpData;
    BETouchData tpProcessed1;
    BETouchData tpProcessed2;
    uint8_t padding2[2];
    BEDir dir;
    uint8_t headphoneStatus;
    uint8_t padding3[3];
    BEVec3 magnet;
    uint8_t slideVolume;
    uint8_t batteryLevel;
    uint8_t micStatus;
    uint8_t slideVolume2;
    uint8_t padding4[8];
};
static_assert(sizeof(VPADStatus) == 0xAC);


void CemuHooks::hook_InjectXRInput(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    // read existing vpad as to not overwrite it
    uint32_t vpadStatusOffset = hCPU->gpr[4];
    VPADStatus vpadStatus = {};
    // todo: revert this to unblock gamepad input
    // readMemory(vpadStatusOffset, &vpadStatus);


    static uint32_t oldCombinedHold = 0;
    // buttons
    uint32_t newXRBtnHold = 0;

    auto mapXRButtonToVpad = [](XrActionStateBoolean& xrState, VPADButtons mapping) -> uint32_t {
        return xrState.currentState ? mapping : 0;
    };

    // todo: fix issue if user has set the jump button to A instead of Y in the game's settings

    auto& left_select = VRManager::instance().XR->m_input.controllers[0].select;
    auto& right_select = VRManager::instance().XR->m_input.controllers[1].select;
    newXRBtnHold |= (mapXRButtonToVpad(left_select, VPAD_BUTTON_A) | mapXRButtonToVpad(right_select, VPAD_BUTTON_A));

    auto& left_grab = VRManager::instance().XR->m_input.controllers[0].grab;
    auto& right_grab = VRManager::instance().XR->m_input.controllers[1].grab;
    // todo: see if select or grab is better

    newXRBtnHold |= mapXRButtonToVpad(VRManager::instance().XR->m_input.cancel, VPAD_BUTTON_B);
    newXRBtnHold |= mapXRButtonToVpad(VRManager::instance().XR->m_input.jump, VPAD_BUTTON_Y);

    newXRBtnHold |= mapXRButtonToVpad(VRManager::instance().XR->m_input.map, VPAD_BUTTON_MINUS);
    newXRBtnHold |= mapXRButtonToVpad(VRManager::instance().XR->m_input.menu, VPAD_BUTTON_PLUS);

    auto& jumpState = VRManager::instance().XR->m_input.jump;
    // todo: jump normally is done by pressing A

    // move stick
    uint32_t newXRStickHold = 0;
    static uint32_t oldXRStickHold = 0;
    constexpr float kAxisThreshold = 0.5f;
    constexpr float kHoldAxisThreshold = 0.1f;

    auto& moveState = VRManager::instance().XR->m_input.move;
    vpadStatus.leftStick = {moveState.currentState.x + vpadStatus.leftStick.x.getLE(), moveState.currentState.y + vpadStatus.leftStick.y.getLE()};

    if (moveState.currentState.x <= -kAxisThreshold || (HAS_FLAG(oldXRStickHold, VPAD_STICK_L_EMULATION_LEFT) && moveState.currentState.x <= -kHoldAxisThreshold))
        newXRStickHold |= VPAD_STICK_L_EMULATION_LEFT;
    else if (moveState.currentState.x >= kAxisThreshold || (HAS_FLAG(oldXRStickHold, VPAD_STICK_L_EMULATION_RIGHT) && moveState.currentState.x >= kHoldAxisThreshold))
        newXRStickHold |= VPAD_STICK_L_EMULATION_RIGHT;

    if (moveState.currentState.y <= -kAxisThreshold || (HAS_FLAG(oldXRStickHold, VPAD_STICK_L_EMULATION_DOWN) && moveState.currentState.y <= -kHoldAxisThreshold))
        newXRStickHold |= VPAD_STICK_L_EMULATION_DOWN;
    else if (moveState.currentState.y >= kAxisThreshold || (HAS_FLAG(oldXRStickHold, VPAD_STICK_L_EMULATION_UP) && moveState.currentState.y >= kHoldAxisThreshold))
        newXRStickHold |= VPAD_STICK_L_EMULATION_UP;

    // camera stick
    auto& cameraState = VRManager::instance().XR->m_input.camera;
    vpadStatus.rightStick = {cameraState.currentState.x + vpadStatus.rightStick.x.getLE(), cameraState.currentState.y + vpadStatus.rightStick.y.getLE()};
    if (cameraState.currentState.x <= -kAxisThreshold || (HAS_FLAG(oldXRStickHold, VPAD_STICK_R_EMULATION_LEFT) && cameraState.currentState.x <= -kHoldAxisThreshold))
        newXRStickHold |= VPAD_STICK_R_EMULATION_LEFT;
    else if (cameraState.currentState.x >= kAxisThreshold || (HAS_FLAG(oldXRStickHold, VPAD_STICK_R_EMULATION_RIGHT) && cameraState.currentState.x >= kHoldAxisThreshold))
        newXRStickHold |= VPAD_STICK_R_EMULATION_RIGHT;

    if (cameraState.currentState.y <= -kAxisThreshold || (HAS_FLAG(oldXRStickHold, VPAD_STICK_R_EMULATION_DOWN) && cameraState.currentState.y <= -kHoldAxisThreshold))
        newXRStickHold |= VPAD_STICK_R_EMULATION_DOWN;
    else if (cameraState.currentState.y >= kAxisThreshold || (HAS_FLAG(oldXRStickHold, VPAD_STICK_R_EMULATION_UP) && cameraState.currentState.y >= kHoldAxisThreshold))
        newXRStickHold |= VPAD_STICK_R_EMULATION_UP;

    oldXRStickHold = newXRStickHold;

    // calculate new hold, trigger and release
    uint32_t combinedHold = (vpadStatus.hold.getLE() | (newXRBtnHold | newXRStickHold));
    vpadStatus.hold = combinedHold;
    vpadStatus.trig = (combinedHold & ~oldCombinedHold);
    vpadStatus.release = (~combinedHold & oldCombinedHold);
    oldCombinedHold = combinedHold;

    // misc
    vpadStatus.vpadErr = 0;
    vpadStatus.batteryLevel = 0xC0;

    // touch
    vpadStatus.tpData.touch = 0;
    vpadStatus.tpData.validity = 3;

    // motion
    vpadStatus.dir.x = {1, 0, 0};
    vpadStatus.dir.y = {0, 1, 0};
    vpadStatus.dir.z = {0, 0, 1};
    vpadStatus.accXY = {1.0f, 0.0f};

    // write the input back to VPADStatus
    writeMemory(vpadStatusOffset, &vpadStatus);

    // set r3 to 1 for hooked VPADRead function to return success
    hCPU->gpr[3] = 1;
}