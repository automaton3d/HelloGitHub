#ifndef PULSATING_H
#define PULSATING_H

#include <SDL3/SDL.h>
#include <stdint.h>

#define L 101
#define WINDOW_W 1280
#define WINDOW_H 720

#define RADIUS (L/2)
#define INF_R2 0x7fffffff

/* parâmetros da onda sinc */
#define DIFF_SHIFT       4
#define DIFF_DIV_SHIFT   5
#define VEL_DAMP_SHIFT   6
#define ABSORB_W         8
#define SHELL_R          (RADIUS/2)
#define SHELL_W          3
#define SHELL_TARGET     200
#define STABILITY_THRESHOLD  5
#define STABILITY_FRAMES     200

/* parâmetros da frente pulsante */
#define PULSE_STEP       3
#define PULSE_TOLERANCE  20
#define YELLOW_VIS_TOL   6

/* parâmetros gráficos */
#define GRAPH_SCALE_X    ((WINDOW_W - 160) / (RADIUS > 1 ? RADIUS : 1))
#define GRAPH_HEIGHT     (WINDOW_H / 2 - 60)

typedef struct {
    int u;          /* deslocamento */
    int v;          /* velocidade */
    int acc;        /* acumulador para Bresenham */
    int sinc_p;     /* numerador */
    int sinc_q;     /* denominador */
    int r;          /* raio inteiro */
    unsigned int r2;/* raio ao quadrado */
    unsigned char ttl;   /* tempo de vida */
    unsigned char trig;  /* flag de disparo */
} Cell;

/* inicialização */
void init(void);

/* evolução */
void sinc_step(void);
void pulse_step(void);
void step_all(void);

/* utilitários */
unsigned int pulse_from_time(unsigned int t);

/* renderização */
void render_frame(SDL_Renderer *ren);

#endif /* PULSATING_H */
