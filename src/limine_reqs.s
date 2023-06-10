.intel_syntax noprefix

	.section .limine_reqs
	.quad blinfo
	.quad kainfo
	.quad mmapinfo
	.quad fbinfo
	.quad rsdpinfo
	.quad hhdminfo
	.quad kfdinfo
	.quad 0
