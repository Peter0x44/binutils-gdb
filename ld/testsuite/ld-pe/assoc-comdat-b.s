	.section .assoc,"dr",associative,foo
	.long 0x22222222

	.section .text$foo,"xr"
	.linkonce discard
	.globl foo
foo:
	movl $2, %eax
	ret
