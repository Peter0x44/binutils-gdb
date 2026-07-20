	.section .assoc,"dr",associative,parent
	.long 0x11223344

	.section .text$parent,"xr"
	.linkonce discard
	.globl parent
parent:
	.byte 0
