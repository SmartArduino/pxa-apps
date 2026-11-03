/* SPDX-License-Identifier: MIT
 * Runtime-copy cache retains its real byte semantics against the exact source
 * file copied by original LoadSMap. All user save slots retain their own
 * original packaged baselines; backend verifies/encodes every actual write.
 */
#include <string.h>
const char *jy_pxa_base_alias(const char *path){
 if(path&&!strcmp(path,"data/allsinbk.grp"))return "data/allsin.grp";
 return path;
}
const char *jy_pxa_base_revision(void){return "89962d7fc2837c143e176c306d83b4e38dab6e77";}
