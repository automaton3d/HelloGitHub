#include <SDL3/SDL.h>
#include <stdio.h>

#define L 101
#define WINDOW_SIZE 1000
#define MAX_R2 (3*(L/2)*(L/2))

typedef struct {
    int u;
    int u_old;
} Cell;

Cell grid[L][L][L];
Cell next[L][L][L];

// -------- r2 sem multiplicação --------
int r2_of(int x,int y,int z,int cx,int cy,int cz){
    int dx=x-cx, dy=y-cy, dz=z-cz;

    int r2=0;

    int ax=(dx<0)?-dx:dx;
    for(int i=0;i<ax;i++) r2+=ax;

    int ay=(dy<0)?-dy:dy;
    for(int i=0;i<ay;i++) r2+=ay;

    int az=(dz<0)?-dz:dz;
    for(int i=0;i<az;i++) r2+=az;

    return r2;
}

// -------- passo CA --------
void step(){

    for(int x=1;x<L-1;x++)
    for(int y=1;y<L-1;y++)
    for(int z=1;z<L-1;z++){

        int u = grid[x][y][z].u;
        int u_old = grid[x][y][z].u_old;

        // ---- 6 faces ----
        int f =
            grid[x+1][y][z].u + grid[x-1][y][z].u +
            grid[x][y+1][z].u + grid[x][y-1][z].u +
            grid[x][y][z+1].u + grid[x][y][z-1].u;

        // ---- 12 arestas ----
        int e =
            grid[x+1][y+1][z].u + grid[x+1][y-1][z].u +
            grid[x-1][y+1][z].u + grid[x-1][y-1][z].u +

            grid[x+1][y][z+1].u + grid[x+1][y][z-1].u +
            grid[x-1][y][z+1].u + grid[x-1][y][z-1].u +

            grid[x][y+1][z+1].u + grid[x][y+1][z-1].u +
            grid[x][y-1][z+1].u + grid[x][y-1][z-1].u;

        // 🔥 laplaciano balanceado (ESSENCIAL)
        int lap = f + (e >> 1) - (u * 12);

        // 🔥 leapfrog estável
        int u_new = (u<<1) - u_old + (lap >> 4);

        next[x][y][z].u_old = u;
        next[x][y][z].u = u_new;
    }

    // copiar buffer
    for(int x=1;x<L-1;x++)
    for(int y=1;y<L-1;y++)
    for(int z=1;z<L-1;z++)
        grid[x][y][z] = next[x][y][z];
}

// -------- render --------
void render(SDL_Renderer* r){

    SDL_SetRenderDrawColor(r,0,0,0,255);
    SDL_RenderClear(r);

    int cx=L/2, cy=L/2, cz=L/2;

    // ---- slice ----
    int z=L/2;

    for(int x=0;x<L;x++)
    for(int y=0;y<L;y++){

        int val = grid[x][y][z].u;
        if(val<0) val=-val;

        int c = val >> 4;
        if(c>255) c=255;

        SDL_SetRenderDrawColor(r,c,c,c,255);
        SDL_RenderPoint(r,x+50,y+50);
    }

    // ---- radial bins ----
    static int sum[MAX_R2];
    static int count[MAX_R2];

    for(int i=0;i<MAX_R2;i++){
        sum[i]=0;
        count[i]=0;
    }

    for(int x=0;x<L;x++)
    for(int y=0;y<L;y++)
    for(int z2=0;z2<L;z2++){

        int r2 = r2_of(x,y,z2,cx,cy,cz);

        if(r2 < MAX_R2){
            int val = grid[x][y][z2].u;
            if(val<0) val=-val;

            sum[r2] += val;
            count[r2]++;
        }
    }

    // ---- escala automática ----
    int maxv = 1;

    for(int i=0;i<MAX_R2;i++){
        if(count[i]==0) continue;

        int v = sum[i] / count[i];
        if(v > maxv) maxv = v;
    }

    int scale = maxv >> 8;
    if(scale==0) scale=1;

    // ---- plot radial ----
    int px0 = 400;
    int py0 = 900;

    SDL_SetRenderDrawColor(r,0,255,0,255);

    for(int i=1;i<MAX_R2;i++){

        if(count[i]==0) continue;

        int v = sum[i] / count[i];

        int x = px0 + (i >> 2);
        int y = py0 - (v / scale);

        if(x < WINDOW_SIZE && y > 0)
            SDL_RenderPoint(r,x,y);
    }

    SDL_RenderPresent(r);
}

// -------- main --------
int main(){

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* w = SDL_CreateWindow(
        "CA onda esférica (FINAL CORRETO)",
        WINDOW_SIZE,WINDOW_SIZE,0);

    SDL_Renderer* r = SDL_CreateRenderer(w,NULL);

    int c=L/2;

    grid[c][c][c].u = 2000;
    grid[c][c][c].u_old = 2000;

    SDL_Event e;
    int run=1;

    while(run){

        while(SDL_PollEvent(&e))
            if(e.type==SDL_EVENT_QUIT) run=0;

        for(int i=0;i<3;i++)
            step();

        render(r);

        SDL_Delay(16);
    }

    SDL_Quit();
    return 0;
}
