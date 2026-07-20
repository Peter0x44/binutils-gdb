	.section .assoc2,"dr",associative,order_parent
b_order_two:
	.long 0x44444444

	.section .assoc1,"dr",associative,order_parent
b_order_one:
	.long 0x22222222

	.section .rdata,"dr"
	.globl order_refs
order_refs:
	.quad b_order_one
	.quad b_order_two

	.section .text$order_parent,"xr"
	.linkonce discard
	.globl order_parent
order_parent:
	ret
