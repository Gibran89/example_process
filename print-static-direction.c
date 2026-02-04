#include <stdio.h>

const int global_rodata = 100; // .rodata
int global_data = 42;          // .data
int global_bss;                // .bss

int main() {
    printf("Direcciones:\n");
    printf("global_rodata (.rodata): %p\n", &global_rodata);
    printf("global_data (.data):   %p\n", &global_data);
    printf("global_bss (.bss):     %p\n", &global_bss);
    
    printf("\nobjdump -t debería mostrar:\n");
    return 0;
}
