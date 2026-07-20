	.section .rdata$assoc,"dr",associative,parent_a
	.long 0x11111111

	.section .text$parent_a,"xr"
	.linkonce discard
	.globl parent_a
parent_a:
	movl $1, %eax
	ret
