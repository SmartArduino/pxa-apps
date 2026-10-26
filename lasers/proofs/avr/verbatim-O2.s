	.file	"avr-verbatim-bodies.cpp"
__SP_H__ = 0x3e
__SP_L__ = 0x3d
__SREG__ = 0x3f
__tmp_reg__ = 0
__zero_reg__ = 1
	.text
.global	__floatsisf
.global	__mulsf3
.global	__fixsfsi
.global	_Z8newLaserP7LASER_T
	.type	_Z8newLaserP7LASER_T, @function
_Z8newLaserP7LASER_T:
	push r16
	push r17
	push r28
	push r29
/* prologue: function */
/* frame size = 0 */
/* stack size = 4 */
.L__stack_usage = 4
	movw r28,r24
	ldi r22,lo8(2)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randoml
	ldi r18,lo8(96)
	ldi r19,lo8(8)
	mul r22,r18
	movw r20,r0
	mul r22,r19
	add r21,r0
	mul r23,r18
	add r21,r0
	clr r1
	movw r24,r20
	sbiw r24,48
	std Y+1,r25
	st Y,r24
	ldi r18,lo8(11)
	ldi r19,0
	ldi r20,0
	ldi r21,0
	ldi r22,lo8(6)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randomll
	ld r24,Y
	ldd r25,Y+1
	cp __zero_reg__,r24
	cpc __zero_reg__,r25
	brge .L2
	neg r22
.L2:
	std Y+2,r22
	ldi r22,lo8(64)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randoml
	std Y+4,r22
	ldi r22,lo8(64)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randoml
	ldd r24,Y+4
	sub r22,r24
	std Y+3,r22
	ldi r22,lo8(-128)
	ldi r23,lo8(22)
	ldi r24,0
	ldi r25,0
	call _Z6randoml
	call __floatsisf
	ldi r18,lo8(53)
	ldi r19,lo8(-6)
	ldi r20,lo8(-114)
	ldi r21,lo8(60)
	call __mulsf3
	call __fixsfsi
	std Y+6,r23
	std Y+5,r22
	ldi r18,lo8(49)
	ldi r19,0
	ldi r20,0
	ldi r21,0
	ldi r22,lo8(16)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randomll
	mov r17,r22
	ldi r22,lo8(2)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randoml
	lsl r22
	rol r23
	rol r24
	rol r25
	subi r22,1
	sbc r23,__zero_reg__
	sbc r24,__zero_reg__
	sbc r25,__zero_reg__
	mul r22,r17
	mov r22,r0
	clr r1
	std Y+7,r22
	std Y+8,__zero_reg__
	ldi r22,lo8(8)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randoml
	movw r16,r22
	movw r18,r24
	lds r25,colorBias
	mov r20,r25
	mov __tmp_reg__,r25
	lsl r0
	sbc r21,r21
	sbc r22,r22
	sbc r23,r23
	ldi r24,lo8(1)
	cp r16,r20
	cpc r17,r21
	cpc r18,r22
	cpc r19,r23
	brge .L3
	ldi r24,0
.L3:
	std Y+9,r24
	lsl r24
	subi r25,lo8(-(-1))
	add r24,r25
	sts colorBias,r24
	lds r24,soundLaserWhite
	lds r25,soundLaserWhite+1
	cp r16,r20
	cpc r17,r21
	cpc r18,r22
	cpc r19,r23
	brge .L5
	lds r24,soundLaserBlack
	lds r25,soundLaserBlack+1
.L5:
	sts pSound+1,r25
	sts pSound,r24
/* epilogue start */
	pop r29
	pop r28
	pop r17
	pop r16
	ret
	.size	_Z8newLaserP7LASER_T, .-_Z8newLaserP7LASER_T
.global	_Z11titleActualv
	.type	_Z11titleActualv, @function
_Z11titleActualv:
	push r17
	push r28
	push r29
/* prologue: function */
/* frame size = 0 */
/* stack size = 3 */
.L__stack_usage = 3
	ldi r28,lo8(lines)
	ldi r29,hi8(lines)
.L8:
	ldi r22,0
	ldi r23,lo8(8)
	ldi r24,0
	ldi r25,0
	call _Z6randoml
	std Y+1,r23
	st Y,r22
	ldi r18,lo8(33)
	ldi r19,0
	ldi r20,0
	ldi r21,0
	ldi r22,lo8(8)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randomll
	mov r17,r22
	ldi r22,lo8(2)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randoml
	lsl r22
	rol r23
	rol r24
	rol r25
	subi r22,1
	sbc r23,__zero_reg__
	sbc r24,__zero_reg__
	sbc r25,__zero_reg__
	mul r22,r17
	mov r22,r0
	clr r1
	std Y+2,r22
	adiw r28,3
	ldi r24,hi8(lines+24)
	cpi r28,lo8(lines+24)
	cpc r29,r24
	brlo .L8
/* epilogue start */
	pop r29
	pop r28
	pop r17
	ret
	.size	_Z11titleActualv, .-_Z11titleActualv
.global	lines
	.section .bss
	.type	lines, @object
	.size	lines, 24
lines:
	.zero	24
	.ident	"GCC: (GNU) 7.3.0"
.global __do_clear_bss
