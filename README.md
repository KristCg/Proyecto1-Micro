# Proyecto 1 de Microprocesadores - Galaga

Un juego estilo Galaga en C++ usando ncurses y POSIX threads.


## Utilizar los siguientes comandos (adentro del directorio donde se encuentra el archivo .cpp) para probar el juego: 

### Compilar 
```
make
```
### Ejecutar
```
./galaga
```
### Limpiar archivos compilados
```
make clean
```
## Dependencias necesarias:
- `g++` con soporte C++17
- `libncurses-dev`
- `pthread` library

Para instalar ncurses, usar este comando adentro de WSL (si usas Windows) o en Terminal:

```
sudo apt install libncurses-dev
```

## Controles del juego:

- **A / ←**: Mover izquierda
- **D / →**: Mover derecha  
- **Espacio / K**: Disparar
- **Q**: Salir del juego

## Imagenes de funcionamiento: 

### Nivel 1:
<img width="650" alt="Nivel1" src="https://github.com/user-attachments/assets/be61f8c3-2c19-4fba-8761-c4b3e3302202" />

### Nivel 2: 
<img width="650" alt="Nivel2" src="https://github.com/user-attachments/assets/865627db-a841-4d9a-8bf4-4fa56e9a6bb8" />

### Nivel 3: 
<img width="650" alt="Nivel3" src="https://github.com/user-attachments/assets/101aca5b-0de8-40ec-9c62-1eec8aa4b366" />

### Pantalla de victoria:
<img width="650" alt="Ganaste" src="https://github.com/user-attachments/assets/bc9938ff-303e-4ce4-ba8e-94a427e49bf1" />
