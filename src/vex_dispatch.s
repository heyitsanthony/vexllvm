.align 16
.global dispatch_asm_amd64
dispatch_asm_amd64:
/* ENTRY: %rax = next guest addr */
jmp dispatch_asm_amd64
ret
