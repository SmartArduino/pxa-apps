	.file	"avr-original-expressions.cpp"
__SP_H__ = 0x3e
__SP_L__ = 0x3d
__SREG__ = 0x3f
__tmp_reg__ = 0
__zero_reg__ = 1
	.text
.global	_Z9titleCasev
	.type	_Z9titleCasev, @function
_Z9titleCasev:
	push r28
	push r29
/* prologue: function */
/* frame size = 0 */
/* stack size = 2 */
.L__stack_usage = 2
	call _Z9getLineVxv
	movw r28,r24
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
	mul r28,r22
	movw r18,r0
	mul r28,r23
	add r19,r0
	mul r29,r22
	add r19,r0
	clr r1
	movw r24,r18
/* epilogue start */
	pop r29
	pop r28
	ret
	.size	_Z9titleCasev, .-_Z9titleCasev
.global	_Z9laserCasev
	.type	_Z9laserCasev, @function
_Z9laserCasev:
	push r12
	push r13
	push r14
	push r15
/* prologue: function */
/* frame size = 0 */
/* stack size = 4 */
.L__stack_usage = 4
	ldi r18,lo8(49)
	ldi r19,0
	ldi r20,0
	ldi r21,0
	ldi r22,lo8(16)
	ldi r23,0
	ldi r24,0
	ldi r25,0
	call _Z6randomll
	movw r12,r22
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
	mul r22,r12
	movw r18,r0
	mul r22,r13
	add r19,r0
	mul r23,r12
	add r19,r0
	clr r1
	movw r24,r18
/* epilogue start */
	pop r15
	pop r14
	pop r13
	pop r12
	ret
	.size	_Z9laserCasev, .-_Z9laserCasev
.global	_Z9sparkCasei
	.type	_Z9sparkCasei, @function
_Z9sparkCasei:
/* prologue: function */
/* frame size = 0 */
/* stack size = 0 */
.L__stack_usage = 0
	ldi r18,lo8(90)
	movw r20,r24
	mul r18,r20
	movw r24,r0
	mul r18,r21
	add r25,r0
	clr __zero_reg__
/* epilogue start */
	ret
	.size	_Z9sparkCasei, .-_Z9sparkCasei
.global	_Z8logoCaseh
	.type	_Z8logoCaseh, @function
_Z8logoCaseh:
/* prologue: function */
/* frame size = 0 */
/* stack size = 0 */
.L__stack_usage = 0
	subi r24,15
	sbc r25,r25
	ldi r22,lo8(3)
	ldi r23,0
	call __divmodhi4
	ldi r24,lo8(58)
	ldi r25,lo8(-86)
	ldi r26,lo8(-29)
	ldi r27,lo8(-18)
	rjmp 2f
	1:
	lsr r27
	ror r26
	ror r25
	ror r24
	2:
	dec r22
	brpl 1b
	andi r24,1
	clr r25
/* epilogue start */
	ret
	.size	_Z8logoCaseh, .-_Z8logoCaseh
	.ident	"GCC: (GNU) 7.3.0"
