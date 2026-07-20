	.section .rdata$assoc,"dr",associative,parent_b
	.long 0x22222222

	.section .text$parent_b,"xr"
	.linkonce discard
	.globl parent_b
parent_b:
	movl $2, %eax
	ret
