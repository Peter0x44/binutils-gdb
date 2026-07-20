	.section .assoc_undefined,"dr",associative,undefined_parent
	.long 2

	.section .text$plain,"xr"
plain_parent:
	.byte 0

	.section .assoc_plain,"dr",associative,plain_parent
	.long 3
