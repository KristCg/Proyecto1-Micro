/*
* Universidad del Valle de Gutemala
* Ingenieria en Ciencias de la Computación
* Programación de Microprocesadores
* Proyecto 1: Galaga
*
* Integrantes:
* Kristel Castillo 241294
* Jorge Chupina
* Ivan Morataya    
*/


#include <iostream>
#include <cstdlib>

void mostrarInstrucciones(){
    system("clear");

    std::cout << "Instrucciones" << std::endl;
    std::cout << "-------------" << std::endl;
    std::cout << "Objetivo del juego: Destruir las naves enemigas y obtener la mayor puntuacion posible" << std::endl;
    std::cout << "Controles:" << std::endl;
    std::cout << " - Mover nave: Flechas izquierda y derecha" << std::endl;
    std::cout << " - Disparar: Barra espaciadora" << std::endl;
    std::cout << " - Pausar: Tecla 'P'" << std::endl;
    std::cout << " - Salir: Tecla 'Q'" << std::endl;
    std::cout << "Presiona cualquier tecla para volver al menu" << std::endl;
    std::cin.ignore(); 
    std::cin.get();
}

void mostrarPuntajes(){

}

void menu(){
    int op;
    do {
        system("clear");
        std::cout << "=========================" << std::endl;
        std::cout << "       GALAGA" << std::endl;
        std::cout << "    Menú Principal" << std::endl;
        std::cout << "=========================" << std::endl;
        std::cout << "1. Iniciar Juego" << std::endl;
        std::cout << "2. Instrucciones" << std::endl;
        std::cout << "3. Puntajes" << std::endl;
        std::cout << "4. Salir" << std::endl;
        std::cout << "Seleccione una opcion: ";
        std::cin >> op;

        switch(op){
            case 1:
                std::cout << "Iniciando juego..." << std::endl;
                break;
            case 2:
                mostrarInstrucciones();
                break;
            case 3:
                mostrarPuntajes();
                break;
            case 4:
                std::cout << "Saliendo del juego. ¡Hasta luego!" << std::endl;
                break;
            default:
                std::cout << "Opcion invalida. Intente de nuevo." << std::endl;
                break;
        }
    } while(op != 4);
}

int main() {
    menu();
    return 0;
}