.intel_syntax noprefix

	.text

	.global	memcpy
	.type memcpy, @function

memcpy:
	mov	rax, rdi		# dest
					# src = rsi
	mov	rcx, rdx		# count
	rep movsb
	ret

	.size	memcpy, .-memcpy


	.global	memset
	.type memset, @function

memset:
	mov	rcx, rdx		# count
	mov	rdx, rdi		# dest
	mov	al, sil			# val
	rep stosb
	mov	rax, rdx
	ret

	.size	memset, .-memset


	.global strnlen
	.type strnlen, @function

strnlen:
.L.strlen:
	mov	rdx, rdi		# start
	mov	rcx, rsi		# max count
	xor	eax, eax		# null-terminator
	repne scasb
	mov	rax, rdi		# end
	sub	rax, rdx
	ret

	.size	strnlen, .-strnlen


	.global strlen
	.type strlen, @function

strlen:
	or	rsi, -1			# size = max
	jmp	.L.strlen

	.size	strlen, .-strlen
