	.section .assoc_alias,"dr",associative,parent_a
	.long 0x11223344

	.section .text$parent,"xr"
	.linkonce discard
parent_a:
parent_b:
	.byte 0

	.section .assoc_alias,"dr",associative,parent_b
	.long 0x55667788
