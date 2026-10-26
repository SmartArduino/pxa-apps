extern long random(long);extern long random(long,long);extern int getLineVx();
int titleCase(){return getLineVx()*(random(2)*2-1);}
int laserCase(){return random(16,49)*(random(2)*2-1);}
int sparkCase(int i){return i*(360*16)/64;}
int logoCase(unsigned char counter){return (0xEEE3AA3AUL>>((counter-60/4)/(60/20)))&1;}
