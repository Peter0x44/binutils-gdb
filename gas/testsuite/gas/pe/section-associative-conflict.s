	.section .assoc_conflict,"dr",associative,parent_a
	.long 0x11223344

	.section .text$parent_a,"xr"
	.linkonce discard
parent_a:
	.byte 0

	.section .text$parent_b,"xr"
	.linkonce discard
parent_b:
	.byte 0

	.section .assoc_conflict,"dr",associative,parent_b
	.long 0x55667788
