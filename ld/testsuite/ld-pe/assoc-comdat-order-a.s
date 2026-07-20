	.section .assoc1,"dr",associative,order_parent
	.globl order_one
order_one:
	.long 0x11111111

	.section .assoc2,"dr",associative,order_parent
	.globl order_two
order_two:
	.long 0x33333333

	.section .text$order_parent,"xr"
	.linkonce discard
	.globl order_parent
order_parent:
	ret
