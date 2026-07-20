	.section .middle,"dr",associative,root
	.globl middle
middle:
	.long 1

	.section .leaf,"dr",associative,middle
	.long 2

	.section .text$root,"xr"
	.linkonce discard
	.globl root
root:
	.byte 0
