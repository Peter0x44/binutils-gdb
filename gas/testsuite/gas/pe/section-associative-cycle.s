	.section .first,"dr",associative,second
	.linkonce discard
	.globl first
first:
	.long 1

	.section .second,"dr",associative,first
	.linkonce discard
	.globl second
second:
	.long 2
