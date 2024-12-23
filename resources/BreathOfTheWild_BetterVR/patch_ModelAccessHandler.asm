[BetterVR_Hands_V208]
moduleMatches = 0x6267BFD0

.origin = codecave


logModelAccessSearch:
mflr r0
stwu r1, -0x0C(r1)
stw r0, 0x10(r1)

stw r3, 0x08(r1)

; log model access search
lwz r3, 0x00(r5)
bl import.coreinit.hook_modifyHandModelAccessSearch


exit_logModelAccessSearch:
lwz r3, 0x08(r1)

lwz r0, 0x10(r1)
mtlr r0
addi r1, r1, 0x0C
blr


#0x0398865C = bla logModelAccessSearch