#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Variables globales inicializadas -> .data */
int variable_data = 42;
const char mensaje_data[] = "Hola desde .data";

/* Variables globales no inicializadas -> .bss */
int variable_bss;
char buffer_bss[100];

/* Constantes y strings literales -> .rodata (read-only) */
const int constante_rodata = 100;
const char* mensaje_rodata = "Hola desde .rodata";

/* Símbolos para límites de secciones */
extern char __executable_start[];
extern char etext[], _etext[], __etext[];
extern char edata[], _edata[];
extern char end[], _end[];

void funcion_stack() {
    /* Variables locales -> stack/pila */
    int local_stack = 10;
    char array_stack[50];
    
    printf("Dentro de funcion_stack():\n");
    printf("  Variable en stack: %d (dirección: %p)\n", local_stack, (void*)&local_stack);
    printf("  Array en stack: dirección: %p\n", (void*)array_stack);
    
    /* Modificando variables en stack */
    local_stack = 20;
    strcpy(array_stack, "Texto en stack");
    printf("  Array en stack: %s\n", array_stack);
}

int main() {
    printf("=== EJEMPLO DE SECCIONES DE MEMORIA EN C (con verificación) ===\n\n");
    
    printf("Límites aproximados de secciones:\n");
    printf("  Inicio ejecutable:  %p\n", (void*)__executable_start);
    printf("  Fin .text (etext):  %p\n", (void*)etext);
    printf("  Fin .data (edata):  %p\n", (void*)edata);
    printf("  Fin .bss (end):     %p\n\n", (void*)end);
    
    printf("1. SECCIÓN .data (variables globales inicializadas):\n");
    printf("   variable_data = %d (dirección: %p)\n", variable_data, (void*)&variable_data);
    printf("   mensaje_data = \"%s\" (dirección: %p)\n\n", mensaje_data, (void*)mensaje_data);
    
    printf("2. SECCIÓN .bss (variables globales no inicializadas):\n");
    printf("   variable_bss = %d (inicialmente 0) (dirección: %p)\n", variable_bss, (void*)&variable_bss);
    printf("   buffer_bss[100] (dirección: %p)\n\n", (void*)buffer_bss);
    
    printf("3. SECCIÓN .rodata (datos de solo lectura):\n");
    printf("   constante_rodata = %d (dirección: %p)\n", constante_rodata, (void*)&constante_rodata);
    printf("   mensaje_rodata (puntero) = %p\n", (void*)&mensaje_rodata);
    printf("   *mensaje_rodata = \"%s\" (dirección del string: %p)\n\n", mensaje_rodata, (void*)mensaje_rodata);
    
    printf("4. STACK/PILA (variables locales):\n");
    int local_main = 99;
    printf("\n   Variable local en main: %d (dirección: %p)\n", local_main, (void*)&local_main);
    
    funcion_stack();
    
    printf("\n5. HEAP/MONTÍCULO (memoria dinámica):\n");
    /* Reserva memoria en el heap */
    int* array_heap = (int*)malloc(5 * sizeof(int));
    char* string_heap = (char*)malloc(50 * sizeof(char));
    
    if (array_heap && string_heap) {
        for (int i = 0; i < 5; i++) {
            array_heap[i] = i * 10;
        }
        strcpy(string_heap, "Texto en heap");
        
        printf("   Array en heap: [");
        for (int i = 0; i < 5; i++) {
            printf("%d", array_heap[i]);
            if (i < 4) printf(", ");
        }
        printf("] (dirección: %p)\n", (void*)array_heap);
        
        printf("   String en heap: %s (dirección: %p)\n\n", string_heap, (void*)string_heap);
        
        /* IMPORTANTE: Liberar memoria del heap */
        free(array_heap);
        free(string_heap);
    }
    
    printf("6. RESUMEN DE UBICACIONES:\n");
    printf("   variable_data:     %p (debería estar en .data)\n", (void*)&variable_data);
    printf("   mensaje_data:      %p (en realidad en .rodata!)\n", (void*)mensaje_data);
    printf("   constante_rodata:  %p (debería estar en .rodata)\n", (void*)&constante_rodata);
    printf("   *mensaje_rodata:   %p (debería estar en .rodata)\n", (void*)mensaje_rodata);
    printf("   &mensaje_rodata:   %p (debería estar en .data)\n", (void*)&mensaje_rodata);
    printf("   variable_bss:      %p (debería estar en .bss)\n", (void*)&variable_bss);
    printf("   buffer_bss:        %p (debería estar en .bss)\n\n", (void*)buffer_bss);
    
    printf("=== COMPROBACIÓN FINAL ===\n");
    printf("1. Ejecuta: objdump -h example4 | grep -E '\\.data|\\.bss|\\.rodata'\n");
    printf("2. Compara las direcciones impresas arriba con los rangos que muestra objdump\n");
    printf("3. Todas deberían estar dentro de sus secciones correspondientes\n");
    
    return 0;
}
