[BetterVR_Find3DFrameBuffer_V208]
moduleMatches = 0x6267BFD0

.origin = codecave


magicColorValue:
.float (0.0 / 32.0)
.float 0.123456789
.float 0.987654321
.float 1.0

clearColorBuffer:
mflr r6
stwu r1, -0x20(r1)
stw r6, 0x24(r1)

stfs f1, 0x20(r1)
stfs f2, 0x1C(r1)
stfs f3, 0x18(r1)
stfs f4, 0x14(r1)
stw r3, 0x10(r1)

lis r3, magicColorValue@ha
lfs f1, magicColorValue@l+0x0(r3)
lfs f2, magicColorValue@l+0x4(r3)
lfs f3, magicColorValue@l+0x8(r3)
lfs f4, magicColorValue@l+0xC(r3)

mr r3, r26
addi r3, r3, 0x1C ; r3 is now the agl::RenderBuffer::mColorBuffer array
lwz r3, 0(r3) ; r3 is now the agl::RenderBuffer::mColorBuffer[0] object
cmpwi r3, 0
beq skipClearingColorBuffer
addi r3, r3, 0xBC ; r3 is now the agl::RenderBuffer::mColorBuffer[0]::mGX2FrameBuffer object

bl import.gx2.GX2ClearColor

skipClearingColorBuffer:
lfs f1, 0x20(r1)
lfs f2, 0x1C(r1)
lfs f3, 0x18(r1)
lfs f4, 0x14(r1)
lwz r3, 0x10(r1)

lwz r6, 0x24(r1)
mtlr r6
addi r1, r1, 0x20
blr


magicDepthValue:
.float 0.0123456789

clearDepthBuffer:
mflr r3
stwu r1, -0x20(r1)
stw r3, 0x24(r1)

stfs f1, 0x20(r1)
stw r3, 0x10(r1)
stw r4, 0x14(r1)
stw r5, 0x18(r1)

lis r3, magicDepthValue@ha
lfs f1, magicDepthValue@l+0x0(r3)
li r4, 1
li r5, 3

mr r3, r26
addi r3, r3, 0x3C ; r3 is now the agl::RenderBuffer::mDepthTarget array
lwz r3, 0(r3) ; r3 is now the agl::RenderBuffer::mDepthTarget object
cmpwi r3, 0
beq skipClearingDepthBuffer
addi r3, r3, 0xBC ; r3 is now the agl::RenderBuffer::mDepthTarget::mGX2FrameBuffer object

bl import.gx2.GX2ClearDepthStencilEx

skipClearingDepthBuffer:
lfs f1, 0x20(r1)
lwz r3, 0x10(r1)
lwz r4, 0x14(r1)
lwz r5, 0x18(r1)

lwz r3, 0x24(r1)
mtlr r3
addi r1, r1, 0x20
blr


; r10 holds the agl::RenderBuffer object
hookPostHDRComposedImage:
mflr r3
stwu r1, -0x4(r1)
stw r3, 0x8(r1)

bla clearColorBuffer
bla clearDepthBuffer

lwz r3, 0x8(r1)
mtlr r3
addi r1, r1, 0x4

lmw r18, 0xB0(r1) ; original instruction
blr


0x039B3044 = bla hookPostHDRComposedImage