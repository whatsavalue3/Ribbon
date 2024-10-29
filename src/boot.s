.globl _start
_start:

.globl main
main:
.fill 0x200, 1, 0

	mov gp,0x8000c000
	mov sp,0x8000d663
	not r1,0x3
	and sp,sp,r1
	bl Main