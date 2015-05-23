	.file	"context_switch.s"
	.text
	.align	2
	.global	KernelExit
	.type	KernelExit, %function
KernelExit:
	@ args = 0, pretend = 0, frame = 12
	@ frame_needed = 1, uses_anonymous_args = 0
	
    # r0 - task, r1 - *request

    # 1) store all kernel registers onto kernel stack
    stmfd   sp!, {r0-r12, lr}

    # change to system mode
    msr cpsr_c, #0xdf

    # Put sp from TD+16 to sp
    ldr sp, [r0, #16]

    # Put task->ret to r0
    ldr r0, [r0, #12]

    # 2) load all user registers
    # note: r2 is loaded first from stack; sp descends
    ldmfd   sp!, {r2-r12, lr}

    # change back to supervisor mode
    msr cpsr_c, #0xd3

    # Put r3 (cpsr_usr) it to spsr_svc 
    msr spsr, r3
    
    # execute user code
    movs pc, r2

	.size	KernelExit, .-KernelExit
	.align	2
	.global	KernelEnter
	.type	KernelEnter, %function
KernelEnter:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
 
    # r0: request address

    # Put lr_svc (pc_usr) in r2
    mov r2, lr

    # Put spsr (cpsr_usr) in r3
    mrs r3, spsr

    # change to system mode
    msr cpsr_c, #0xdf

    # 2) store all user registers to user stack
    stmfd   sp!, {r2-r12, lr}
    
    # put sp_usr in r2
    mov r2, sp
    mov r3, r0

    # change back to supervisor mode
    msr cpsr_c, #0xd3

    # now:
    # r2: sp_usr
    # r3: &request

    # load r0 (task) and r1 (request)
    ldmfd sp!, {r0, r1}

    # hand sp_usr, swi_arg to task->sp, request
    str r2, [r0, #16]
    str r3, [r1]

    # 1) load the rest of the kernel registers from stack
    ldmfd sp!, {r2-r12, pc}

	.size	KernelEnter, .-KernelEnter
	.ident	"GCC: (GNU) 4.0.2"