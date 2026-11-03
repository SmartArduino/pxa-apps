/* SPDX-License-Identifier: MIT. The real upstream process entry runs once per VM. */
int digger_engine_main(int,char **);
void digger_engine_entry(void){
 char name[]="digger";char *argv[]={name,0};
 (void)digger_engine_main(1,argv);
}

