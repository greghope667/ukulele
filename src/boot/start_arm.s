	.text
	.global	_hcf
	.global	_start

	.type _start, @function
_start:
	bl	kernel_main
_hcf:
	b	_hcf

	.size	_start, .-_start
