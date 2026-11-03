/* SPDX-License-Identifier: MIT
 * Real FreeType protected validation/raster frames on the shared C coroutine.
 * Only the cooperative build uses this ABI; native FreeType keeps libc setjmp.
 */
#ifndef JY_FT_PROTECTED_H
#define JY_FT_PROTECTED_H
typedef struct jy_ft_jmp_record { volatile int status; } jy_ft_jmp_record;
typedef jy_ft_jmp_record jy_ft_jmp_buf[1];
int jy_ft_checkpoint(jy_ft_jmp_buf buffer);
void jy_ft_throw(jy_ft_jmp_buf buffer, int status);
void jy_ft_release(jy_ft_jmp_buf buffer);
unsigned jy_ft_entry_count(void);
unsigned jy_ft_throw_count(void);
unsigned jy_ft_release_count(void);
unsigned jy_ft_fault_count(void);
#endif
