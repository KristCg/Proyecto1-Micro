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

### Menu principal
<img width="650" alt="image" src="https://github.com/user-attachments/assets/522dc30d-6744-41c3-a104-94182ab6d603" />

### Seleccionar modo de juego
<img width="650" alt="image" src="https://github.com/user-attachments/assets/62756437-ed2d-4b0b-a8f1-925a156c7120" />

### Modo 1: 40 aliens
<img width="650" alt="image" src="https://github.com/user-attachments/assets/45fc2f40-a344-4bd5-814c-736e8d080f29" />

### Modo 2: 50 aliens
<img width="650" alt="image" src="https://github.com/user-attachments/assets/8d9c20f1-7031-4eec-8e81-6a986fd7013b" />

### Pantalla de victoria
<img width="650" alt="image" src="https://github.com/user-attachments/assets/19bdea64-97d1-46be-9698-41d943ce924d" />
