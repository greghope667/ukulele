ENTRY(_start)
SECTIONS
{
	/* Use high-half kernel, as required by limine */
	. = 0xffffffffc0000000;
	__start_exe = .;

	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.text)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		__end_exe = .;
		*(.rodata)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		__start_data = .;
		*(.data)
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}

	. = ALIGN(4K);
	__end_data = .;
}
