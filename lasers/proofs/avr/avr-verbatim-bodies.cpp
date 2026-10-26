typedef signed char int8_t;typedef unsigned char uint8_t;typedef signed short int16_t;typedef unsigned char byte;
extern long random(long);extern long random(long,long);
#define WIDTH 128
#define HEIGHT 64
#define SCALE 16
#define LASER_X_MAX (WIDTH*SCALE)
#define LASER_DIAM (3*SCALE)
#define DEG_MAX (360*SCALE)
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAY_VD_MIN (1*SCALE)
#define RAY_VD_MAX (3*SCALE)
#define LINE_X_MAX (WIDTH*SCALE)
#define LINE_VX_MIN (SCALE/2)
#define LINE_VX_MAX (SCALE*2)
#define LINES_MAX 8
typedef struct{int16_t x;int8_t vx,a,b;int16_t d;int8_t vd;uint8_t r;bool isWhite;} LASER_T;
extern int8_t colorBias;extern const byte*soundLaserWhite;extern const byte*soundLaserBlack;extern const byte*pSound;
void newLaser(LASER_T *p)
{
    p->x = random(2) * (LASER_X_MAX + LASER_DIAM * 2) - LASER_DIAM;
    p->vx = random(6, 11) * ((p->x <= 0) * 2 - 1);
    p->b = random(HEIGHT);
    p->a = random(HEIGHT) - p->b;
    p->d = random(DEG_MAX) * DEG_TO_RAD;
    p->vd = random(RAY_VD_MIN, RAY_VD_MAX + 1) * (random(2) * 2 - 1);
    p->r = 0;
    p->isWhite = (random(8) >= colorBias);
    colorBias += p->isWhite * 2 - 1; 
    pSound = (p->isWhite) ? soundLaserWhite : soundLaserBlack;
}

typedef struct{int16_t x;int8_t vx;} LINE_T;LINE_T lines[LINES_MAX];
static int8_t getLineVx(){return random(LINE_VX_MIN,LINE_VX_MAX+1);}
void titleActual(){
    for (LINE_T *p = &lines[0]; p < &lines[LINES_MAX]; p++) {
        p->x = random(LINE_X_MAX);
        p->vx = getLineVx() * (random(2) * 2 - 1);
    }

}
