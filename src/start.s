.intel_syntax noprefix

	.text
	.global	_hcf
	.global	_start

	.type _start, @function
_start:
	push	rax
	call	kernel_main
_hcf:
	cli
	hlt
	jmp	_hcf

	.size	_start, .-_start
