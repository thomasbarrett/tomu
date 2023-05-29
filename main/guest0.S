.globl _start
.code16
_start:
  xorw %ax, %ax
loop:
	out %ax, $0x10
	inc %ax
	jmp loop
