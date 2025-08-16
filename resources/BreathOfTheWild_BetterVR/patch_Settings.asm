[BetterVR_UpdateSettings_V208]
moduleMatches = 0x6267BFD0

.origin = codecave


data_settingsOffset:
CameraModeSetting:
.int $cameraMode

LeftHandModeSetting:
.int $leftHanded

GUIFollowModeSetting:
.int $guiFollowMode

PlayerHeightSetting:
.float $cameraHeight

Enable2DViewSetting:
.int $enable2DView

CropFlatTo16_9Setting:
.int $cropFlatTo16_9

EnableDebugOverlaySetting:
.int $enableDebugOverlay

BuggyAngularVelocitySetting:
.int $buggyAngularVelocity

vr_updateSettings:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x10(r1)

lis r5, data_settingsOffset@ha
addi r5, r5, data_settingsOffset@l
bl import.coreinit.hook_UpdateSettings

; spawn check
li r3, 0
bl import.coreinit.hook_CreateNewActor
cmpwi r3, 1
bne notSpawnActor
;bl vr_spawnEquipment
notSpawnActor:

;bl checkIfDropWeapon


lwz r6, 0x10(r1)
lwz r5, 0x14(r1)
lwz r4, 0x18(r1)
lwz r3, 0x1C(r1)
lwz r0, 0x24(r1)
addi r1, r1, 0x20
mtlr r0

li r4, -1 ; Execute the instruction that got replaced
blr

0x031FAAF0 = bla vr_updateSettings