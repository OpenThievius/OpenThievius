/* SPDX-License-Identifier: AGPL-3.0-or-later */
.section .text.start,"ax",@progbits
.global _start
.type _start,@function
_start:
    xorl %ebp, %ebp
    movl (%esp), %eax
    leal 4(%esp), %edx
    andl $-16, %esp
    subl $8, %esp
    pushl %edx
    pushl %eax
    call sly1_native_host32_main
    addl $8, %esp
    movl %eax, %ebx
    movl $1, %eax
    int $0x80
    ud2
.size _start, .-_start

.global sly1_i386_syscall6
.type sly1_i386_syscall6,@function
sly1_i386_syscall6:
    pushl %ebx
    pushl %esi
    pushl %edi
    pushl %ebp
    movl 20(%esp), %eax
    movl 24(%esp), %ebx
    movl 28(%esp), %ecx
    movl 32(%esp), %edx
    movl 36(%esp), %esi
    movl 40(%esp), %edi
    movl 44(%esp), %ebp
    int $0x80
    popl %ebp
    popl %edi
    popl %esi
    popl %ebx
    ret
.size sly1_i386_syscall6, .-sly1_i386_syscall6
