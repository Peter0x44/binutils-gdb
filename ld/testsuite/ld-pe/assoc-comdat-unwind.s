	.text
	.globl main
main:
	.long 0x12345678

	.section .pdata,"dr",associative,dead
	.long 0xaaaaaaaa

	.section .xdata,"dr",associative,dead
	.long 0xbbbbbbbb

	.section .text$dead,"xr"
	.linkonce discard
	.globl dead
dead:
	.long 0xdeadbeef

	.section .pdata$legacy,"dr"
	.long 0x11223344

	.section .xdata$legacy,"dr"
	.long 0x55667788
