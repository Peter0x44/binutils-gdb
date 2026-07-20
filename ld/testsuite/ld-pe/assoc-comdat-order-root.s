	.text
	.globl main
main:
	call order_parent
	movl order_one(%rip), %eax
	addl order_two(%rip), %eax
	leaq order_refs(%rip), %rcx
	ret
