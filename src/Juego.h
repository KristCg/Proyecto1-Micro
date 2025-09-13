#ifndef JUEGO_H
#define JUEGO_H

#include <vector>
using namespace std;

struct Entidad {
    int x, y;
    int width, height;
    bool vivo;
    const char **sprite;
};

struct Proyectil {
    int x, y, dir;
    bool activo;
    void mover(int maxY);
};

struct Enemigo {
    int x, y;
    bool vivo;
};

class Juego {
private:
    bool running;
    int maxX;
    int maxY;
    int frameDelay;

    Entidad jugadorEntidad;
    vector<Enemigo> enemigos;
    vector<Proyectil> proyectiles;

public:
    Juego();
    void run();
    void handleInput();
    void moverProyectiles();
    void moverEnemigos();
    void dibujar();
    void inicializarEnemigos();
    bool colision(const Entidad &a, const Enemigo &b);
    void clearScreen();
};

#endif