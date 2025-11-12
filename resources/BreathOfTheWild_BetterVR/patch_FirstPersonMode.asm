[BetterVR_FirstPersonMode_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; the patch below makes these deprecated
;0x031CB340 = li r3, 1 ; forces EventMgr::isActiveAtFirstPersonMode to always be true, which disables opacity but also disables interactions
;0x02C056F4 = li r3, 1 ; this should disable the CameraPhysQuery to always show link in first person mode
;0x02C07844 = li r3, 1 ; this should disable the CameraGfxQuery to always show link in first person mode

; replaces the blr in Actor/Weapon/OptionalWeapon::CalculateModelOpacity
0x024A6F20 = ba import.coreinit.hook_CalculateModelOpacity
0x033AFA9C = ba import.coreinit.hook_CalculateModelOpacity
0x037B13A8 = ba import.coreinit.hook_CalculateModelOpacity

; hooks Actor::setOpacity() and replace it with a custom C++ function
; todo: technically this might not be required anymore
0x037B13AC = ba import.coreinit.hook_SetActorOpacity

; sets distance from the camera
0x10216594 = const_05_10216594:
0x10216598 = const_3_10216598:

customCameraDistance:
.float $cameraDistance

hook_UseCustomCameraDistance:
mflr r0
stwu r1, -0x10(r1)
stw r0, 0x14(r1)
stw r3, 0x08(r1)
stw r4, 0x04(r1)

; load the user-selected camera distance
lis r9, customCameraDistance@ha
lfs f10, customCameraDistance@l(r9)

; load camera distance like normal
lis r9, const_05_10216594@ha
lfs f13, const_05_10216594@l(r9)
lis r12, const_3_10216598@ha
lfs f12, const_3_10216598@l(r12)

bl import.coreinit.hook_UseCameraDistance

lwz r4, 0x04(r1)
lwz r3, 0x08(r1)
lwz r0, 0x14(r1)
addi r1, r1, 0x10
mtlr r0
blr

0x02E5FEB8 = bla hook_UseCustomCameraDistance
0x02E5FEBC = nop
0x02E5FEC0 = nop
0x02E5FEC4 = nop


; TODO: check if weapon opacity is still not set to 0.0 whenever they're inside the camera
; TODO: we should add a check to see if the vtable is a bow, weapon or shield that's held in the hand, and then force the opacity to 1.0


; disables the transition effect when an object goes out of view/near the camera
; 0x4182003C = beq 0x02D53130
; 0x60000000 = nop
;0x02D530F4 = .int ((($cameraMode == 1) * 0x4182003C) + (($cameraMode == 0) * 0x60000000))

; disables the opacity fade effect when it gets near any graphics
;0x02C05A2C = .int ((($cameraMode == 0) * 0x2C040000) + (($cameraMode == 1) * 0x7C042000))

; disables camera collision fading when it collides with objects
;0x02C07848 = .int ((($cameraMode == 0) * 0x2C030000) + (($cameraMode == 1) * 0x2C010000))

; prevents the translucent opacity to be set for the weapon (and maybe more?)
;0x024A69CC = _setWeaponOpacity:
;conditionalSetWeaponOpacityJump:
;mflr r0
;stwu r1, -0x10(r1)
;stw r0, 0x14(r1)
;stw r8, 0x08(r1)
;
;li r8, $cameraMode
;cmpwi r8, 1
;beq exitSetWeaponOpacity
;
;lis r8, _setWeaponOpacity@ha
;addi r8, r8, _setWeaponOpacity@l
;mtctr r8
;bctrl
;
;exitSetWeaponOpacity:
;lwz r8, 0x08(r1)
;lwz r0, 0x14(r1)
;addi r1, r1, 0x10
;mtlr r0
;blr
;
;0x024ae2b4 = bla conditionalSetWeaponOpacityJump
;
;; disables more SetWeaponOpacity calls
;0x02D55084 = .int ((($cameraMode == 0) * 0x4B751A55) + (($cameraMode == 1) * 0x60000000))
;


; disables the transition effect when an object goes out of view/near the camera
; 0x4182003C = beq 0x02D53130
; 0x60000000 = nop
;0x02D530F4 = .int ((($cameraMode == 0) * 0x4182003C) + (($cameraMode == 1) * 0x60000000))

; disables the opacity fade effect when it gets near any graphics
;0x02C05A2C = .int ((($cameraMode == 0) * 0x2C040000) + (($cameraMode == 1) * 0x7C042000))

; disables camera collision fading when it collides with objects
;0x02C07848 = .int ((($cameraMode == 0) * 0x2C030000) + (($cameraMode == 1) * 0x2C010000))
