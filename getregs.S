.globl get_regs
.func get_regs
get_regs:
        mov     %rsp, (%rdi)
        mov     %rbx, 8(%rdi)
        mov     %rbp, 16(%rdi)
        mov     %r12, 24(%rdi)
        mov     %r13, 32(%rdi)
        mov     %r14, 40(%rdi)
        mov     %r15, 48(%rdi)
        ret 
.endfunc
        
