#include "setjmp.h"

__attribute__((naked)) int setjmp(jmp_buf env) {
    __asm__ volatile (
        "mov %rbx, 0(%rcx)\n\t"
        "mov %rbp, 8(%rcx)\n\t"
        "mov %rdi, 16(%rcx)\n\t"
        "mov %rsi, 24(%rcx)\n\t"
        "mov %r12, 32(%rcx)\n\t"
        "mov %r13, 40(%rcx)\n\t"
        "mov %r14, 48(%rcx)\n\t"
        "mov %r15, 56(%rcx)\n\t"
        "lea 8(%rsp), %rax\n\t"
        "mov %rax, 64(%rcx)\n\t"
        "mov (%rsp), %rax\n\t"
        "mov %rax, 72(%rcx)\n\t"
        "xor %eax, %eax\n\t"
        "ret\n\t"
    );
}

__attribute__((naked)) void longjmp(jmp_buf env, int val) {
    __asm__ volatile (
        "mov %edx, %eax\n\t"
        "test %eax, %eax\n\t"
        "jnz 1f\n\t"
        "inc %eax\n\t"
        "1:\n\t"
        "mov 0(%rcx), %rbx\n\t"
        "mov 8(%rcx), %rbp\n\t"
        "mov 16(%rcx), %rdi\n\t"
        "mov 24(%rcx), %rsi\n\t"
        "mov 32(%rcx), %r12\n\t"
        "mov 40(%rcx), %r13\n\t"
        "mov 48(%rcx), %r14\n\t"
        "mov 56(%rcx), %r15\n\t"
        "mov 64(%rcx), %rsp\n\t"
        "jmp *72(%rcx)\n\t"
    );
}
