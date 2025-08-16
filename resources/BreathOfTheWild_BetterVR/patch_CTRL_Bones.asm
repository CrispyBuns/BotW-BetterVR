[BetterVR_HandBones_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

0x03C6FE5C = copyMatrix34:

0x03821B64 = ksys_phys_ModelBoneAccessor_getBoneName:

custom_gsys_ModelUnit_getBoneLocalMatrix:
; original function prologue
stwu r1, -0x30(r1)
mflr r0
stw r0, 0x34(r1)
stw r31, 0x0C(r1)
stw r30, 0x08(r1)
mr r30, r5
stw r29, 0x14(r1)
stw r28, 0x18(r1)
stw r27, 0x1C(r1)
lwz r9, 0x04(r12) ; store key->modelAccessHandle->name
stw r9, 0x20(r1)

; r3 = gsys::ModelUnit*
; r4 = sead::Matrix34*
; r5 = Vec3*
; r6 = int boneIdx
lwz r9, 0x9C(r3)
lwz r12, 0xC(r9)
slwi r0, r6, 6
lhz r10, 4(r9)
add r31, r12, r0
ori r10, r10, 4
lis r3, copyMatrix34@ha
addi r3, r3, copyMatrix34@l
mtctr r3
addi r3, r31, 0x10
sth r10, 4(r9) ; src transform from model unit
; r4 is already the destination matrix
bctrl ; bl copyMatrix34
lfs f0, 4(r31)
stfs f0, 0(r30)
lfs f0, 8(r31)
stfs f0, 4(r30)
lfs f0, 0xC(r31)
stfs f0, 8(r30)

; call custom bone matrix function
; r6 = char* boneName
; r5 = Vec3* scale
; r4 = sead::Matrix34*
; r3 = gsys::ModelUnit*

mr r27, r6

lwz r6, 0x20(r1) ; load name

lwz r3, 0x0C(r1) ; r31 = ksys::phys::ModelBoneAccessor*
lwz r3, 0x10(r3) ; ModelBoneAccessor::gsysModel

bla import.coreinit.hook_ModifyBoneMatrix

; restore bone index finally
mr r6, r27

; prologue
lwz r0, 0x34(r1)
mtlr r0

lwz r27, 0x1C(r1)
lwz r28, 0x18(r1)
lwz r29, 0x14(r1)
lwz r31, 0x0C(r1)
lwz r30, 0x08(r1)
addi r1, r1, 0x30
blr

; hooks ksys::phys::ModelBoneAccessor::copyModelPoseToHavok at 0382094C to get the custom bone matrix instead of the original one
0x03820C38 = lis r8, custom_gsys_ModelUnit_getBoneLocalMatrix@ha
0x03820C3C = addi r9, r8, custom_gsys_ModelUnit_getBoneLocalMatrix@l


; ----------------------
; hook ModelBoneAccessor::copyHavokPoseToModel, although there's no use for it

custom_gsys__Model__setBoneLocalMatrix:
mr r8, r4
lwz r0, 0x1C(r3)
lha r7, 0(r8)
mr r4, r5
cmplw r7, r0
li r12, 0
bge loc_3985FE8
lwz r0, 0x24(r3)
slwi r12, r7, 2
lwzx r12, r12, r0

loc_3985FE8:
mr r10, r3 ; backup gsys::Model*
lwz r3, 0(r12)
lwz r9, 0x74(r3)
mr r12, r10
lwz r10, 0x4C(r9)
mtctr r10
mr r5, r6
lha r6, 2(r8)
ba import.coreinit.hook_ModifyModelBoneMatrix

0x03985FC0 = ba custom_gsys__Model__setBoneLocalMatrix

;0x03985FE8 = lwz r3, 0x0(r12)
;0x03986000 = ba import.coreinit.hook_ModifyModelBoneMatrix