	.section .assoc,"dr",associative,foo
	.long 0x11111111

	.section .text$foo,"xr"
	.linkonce discard
	.globl foo
foo:
	movl $1, %eax
	ret
