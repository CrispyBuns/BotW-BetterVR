[BetterVR_STUB_StereoRenderingDebugging_V208]
moduleMatches = 0x6267BFD0

.origin = codecave


const_cameraHeightChange:
.float 23

stub_hook_GetRenderCamera:
blr
lis r11, currentEyeSide@ha
lwz r11, currentEyeSide@l(r11)
cmpwi r11, 0
beqlr ; don't modify right eye

; camera->pos.y += 2.0; 
lfs f12, 0x34+0x04(r3)
lis r11, const_cameraHeightChange@ha
lfs f13, const_cameraHeightChange@l(r11)
;fadd f13, f13, f12
stfs f13, 0x34+0x04(r3)
; camera->at.y += 2.0;
lfs f12, 0x40+0x04(r3)
lis r11, const_cameraHeightChange@ha
lfs f13, const_cameraHeightChange@l(r11)
;fadd f13, f13, f12
stfs f13, 0x40+0x04(r3)
; camera->mtx.pos_y += 2.0;
lfs f12, 0x1C(r3)
lis r11, const_cameraHeightChange@ha
lfs f13, const_cameraHeightChange@l(r11)
;fadd f13, f13, f12
stfs f13, 0x1C(r3)

blr


example_PerspectiveProjectionMatrix:
.byte 1,1,0,0
.int 0x3F9A678C
.float 0.9300732,-107374176,-107374176,-107374176
.float -107374176,0.86867154,-107374176,-107374176
.float 0.06992682,-0.0352425,-1.000008,-1
.float -107374176,-107374176,-0.2000008,0
.float 0.9300732,-107374176,-107374176,-107374176
.float -107374176,0.86867154,-107374176,-107374176
.float 0.06992682,-0.0352425,-1.000008,-1
.float -107374176,-107374176,-0.2000008,0
.float 1
.float 0
.int 0x1027B54C
.float 0.1
.float 25000
.float 1.7104228
.float 0.7547096
.float 0.65605897
.float 1.1503685
.float 0.95918363
.float 0.034906596
.float -0.017453283

stub_hook_GetRenderProjection:
lis r3, example_PerspectiveProjectionMatrix@ha
addi r3, r3, example_PerspectiveProjectionMatrix@l
blr
