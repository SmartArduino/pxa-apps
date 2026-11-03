/* SPDX-License-Identifier: MIT
 * No shell exists inside this Guest. Honest optional OS capability failure.
 * File temp support is replaced by the writable VFS once it is bound.
 */
#include <stdio.h>
#include <errno.h>
int __wrap_system(const char *command){if(!command)return 0;errno=ENOSYS;return -1;}
FILE *__wrap_tmpfile(void){errno=ENOSYS;return NULL;}
