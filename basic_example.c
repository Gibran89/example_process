// mi_programa.c
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    printf("PID: %d\n", getpid());
    printf("PPID: %d\n", getppid());
    printf("Ejecutando...\n");
    
    // Mantener vivo para inspeccionar
    pause();
    
    return 0;
}
