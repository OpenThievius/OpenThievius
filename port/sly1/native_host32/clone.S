/* SPDX-License-Identifier: AGPL-3.0-or-later */
.section .text
.global sly1_clone_thread
.type sly1_clone_thread,@function
sly1_clone_thread:
    pushl %ebx
    pushl %esi
    pushl %edi
    pushl %ebp
    movl 20(%esp), %eax
    movl 24(%esp), %ecx
    movl 28(%esp), %edx
    subl $8, %ecx
    movl %eax, 0(%ecx)
    movl %edx, 4(%ecx)
    movl $120, %eax
    movl $0x50f00, %ebx
    xorl %edx, %edx
    xorl %esi, %esi
    xorl %edi, %edi
    xorl %ebp, %ebp
    int $0x80
    testl %eax, %eax
    jz .Lchild
    popl %ebp
    popl %edi
    popl %esi
    popl %ebx
    ret
.Lchild:
    popl %eax
    popl %edx
    subl $12, %esp
    pushl %edx
    call *%eax
    addl $16, %esp
    movl $1, %eax
    xorl %ebx, %ebx
    int $0x80
    ud2
.size sly1_clone_thread, .-sly1_clone_thread
