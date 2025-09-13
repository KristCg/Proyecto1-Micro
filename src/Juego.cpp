#include "Juego.h"
#include <iostream>
#include <windows.h>
#include <conio.h>
#include <algorithm>
using namespace std;

// Sprites
const int jugadorWidth = 7;
const int jugadorHeight = 5;
const char jugador[jugadorHeight][jugadorWidth+1] = {
    "   ^   ",
    "  /|\\  ",
    " /_|_\\ ",
    "|_   _|",
    "  | |  "
};

const int enemigo1Width = 7;
const int enemigo1Height = 3;
const char enemigo1[enemigo1Height][enemigo1Width+1] = {
    "  O  ",
    " OOO ",
    "O O O"
};

// ------------------- Proyectil -------------------
void Proyectil::mover(int maxY) {
    y += dir;
    if (y < 0 || y > maxY) activo = false;
}

// ------------------- Constructor -------------------
Juego::Juego() {
    running = true;
    maxX = 40;
    maxY = 20;
    frameDelay = 100;

    jugadorEntidad = {17, 15, jugadorWidth, jugadorHeight, true, (const char **)jugador};

    inicializarEnemigos();
    proyectiles.clear();
}

// ------------------- Enemigos -------------------
void Juego::inicializarEnemigos() {
    enemigos.clear();
    for (int i = 0; i < 5; i++)
        enemigos.push_back({i * 8, 0, true});
}

// ------------------- Colisiones -------------------
bool Juego::colision(const Entidad &a, const Enemigo &b) {
    for (int i = 0; i < a.height; i++)
        for (int j = 0; j < a.width; j++)
            if (a.sprite[i][j] != ' ') {
                int ax = a.x + j;
                int ay = a.y + i;
                if (ax >= b.x && ax < b.x + enemigo1Width &&
                    ay >= b.y && ay < b.y + enemigo1Height)
                    return true;
            }
    return false;
}

// ------------------- Movimiento -------------------
void Juego::moverProyectiles() {
    for (auto &p : proyectiles)
        p.mover(maxY);
}

void Juego::moverEnemigos() {
    for (auto &e : enemigos)
        if (e.vivo) {
            e.y++;
            if (e.y > maxY) e.vivo = false;
        }
}

// ------------------- Input -------------------
void Juego::handleInput() {
    if (_kbhit()) {
        int tecla = _getch();
        if (tecla == 224) {
            tecla = _getch();
            if (tecla == 75) jugadorEntidad.x = max(0, jugadorEntidad.x - 1); // izquierda
            if (tecla == 77) jugadorEntidad.x = min(maxX - jugadorEntidad.width, jugadorEntidad.x + 1); // derecha
        } else {
            if (tecla == ' ') {
                Proyectil p;
                p.x = jugadorEntidad.x + jugadorEntidad.width/2;
                p.y = jugadorEntidad.y - 1;
                p.dir = -1;
                p.activo = true;
                proyectiles.push_back(p);
            }
            if (tecla == 'q') running = false;
        }
    }
}

// ------------------- Dibujar -------------------
void Juego::clearScreen() {
    COORD cursorPos;
    cursorPos.X = 0;
    cursorPos.Y = 0;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), cursorPos);
}

void Juego::dibujar() {
    clearScreen();

    // jugador
    if (jugadorEntidad.vivo)
        for (int i = 0; i < jugadorEntidad.height; i++)
            cout << string(jugadorEntidad.x, ' ') << jugadorEntidad.sprite[i] << "\n";

    // enemigos
    for (auto &e : enemigos)
        if (e.vivo)
            for (int i = 0; i < enemigo1Height; i++)
                cout << string(e.x, ' ') << enemigo1[i] << "\n";

    // proyectiles
    for (auto &p : proyectiles)
        if (p.activo)
            cout << string(p.x, ' ') << "|\n";
}

// ------------------- Loop principal -------------------
void Juego::run() {
    while (running) {
        handleInput();
        moverProyectiles();
        moverEnemigos();

        // Colisiones
        for (auto &p : proyectiles) {
            if (!p.activo) continue;
            for (auto &e : enemigos)
                if (e.vivo && colision(jugadorEntidad, e)) {
                    e.vivo = false;
                    p.activo = false;
                }
        }

        dibujar();
        Sleep(frameDelay); // Esto evita que la CPU se consuma toda
    }
}