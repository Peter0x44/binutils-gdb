	.section .assoc,"dr",associative,parent
	.long 0x11223344

	.section .text$parent,"xr"
	.linkonce one_only
	.globl parent
parent:
	.byte 0
