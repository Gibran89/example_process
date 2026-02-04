#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <sched.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <errno.h>
#include <time.h>

// ========== CONSTANTES Y VARIABLES GLOBALES ==========
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Variables en diferentes secciones de memoria (para demostración)
int global_var = 42;                     // .data (inicializada)
int global_uninit_var;                   // .bss (no inicializada)
const int global_const = 100;            // .rodata (solo lectura)
char global_buffer[256] = "Buffer global compartido"; // Para demostrar COW

// ========== FUNCIONES DE UTILIDAD ==========

// Función para obtener PID del kernel (gettid wrapper con nombre correcto)
static inline pid_t get_kernel_pid(void) {
    return syscall(SYS_gettid);
}

// Función para obtener TGID (getpid wrapper con nombre correcto)
static inline pid_t get_tgid(void) {
    return getpid();  // getpid() realmente devuelve TGID
}

// Obtener timestamp para ver concurrencia
void print_timestamp(const char *prefix) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    printf("%s[%ld.%03ld] ", prefix, ts.tv_sec % 1000, ts.tv_nsec / 1000000);
}

// Imprimir cabecera con color
void print_header(const char *title, char color) {
    printf("\n%s", color == 'R' ? COLOR_RED : 
                   color == 'G' ? COLOR_GREEN :
                   color == 'Y' ? COLOR_YELLOW :
                   color == 'B' ? COLOR_BLUE :
                   color == 'M' ? COLOR_MAGENTA : COLOR_CYAN);
    printf("════════════════════════════════════════════════════════════\n");
    printf("  %s%s\n", COLOR_BOLD, title);
    printf("%s════════════════════════════════════════════════════════════%s\n", 
           color == 'R' ? COLOR_RED : 
           color == 'G' ? COLOR_GREEN :
           color == 'Y' ? COLOR_YELLOW :
           color == 'B' ? COLOR_BLUE :
           color == 'M' ? COLOR_MAGENTA : COLOR_CYAN, COLOR_RESET);
}

// Imprimir identificadores de forma clara
void print_identifiers(const char *entity_name, int is_thread) {
    printf("  🏷️  Identificadores de %s:\n", entity_name);
    printf("    ┌─────────────────────────────────┐\n");
    printf("    │ TGID (getpid):    %12d │\n", get_tgid());
    printf("    │ PID kernel:       %12ld │\n", (long)get_kernel_pid());
    if (is_thread) {
        printf("    │ pthread ID:      %12ld │\n", pthread_self());
    }
    printf("    │ En kernel:        task_struct #%ld │\n", (long)get_kernel_pid());
    printf("    └─────────────────────────────────┘\n");
}

// ========== FUNCIONES DE ANÁLISIS DE MEMORIA MEJORADAS ==========

void print_memory_details(const char *entity_name, int is_thread) {
    print_timestamp("");
    printf("%s%s%s analiza su memoria:\n", 
           COLOR_BOLD, entity_name, COLOR_RESET);
    
    // Identificadores con nombres correctos
    print_identifiers(entity_name, is_thread);
    
    // Variables para análisis
    int stack_var = get_tgid() % 1000;  // Variable local en stack
    void *heap_var = malloc(128);       // Alloc en heap
    sprintf((char*)heap_var, "Alloc de %s", entity_name);
    
    printf("\n  📍 Direcciones de variables:\n");
    printf("    Constante (.rodata):   %s%p%s = %d\n", 
           COLOR_CYAN, (void*)&global_const, COLOR_RESET, global_const);
    printf("    Global var (.data):    %s%p%s = %d\n", 
           COLOR_CYAN, (void*)&global_var, COLOR_RESET, global_var);
    printf("    Global buffer:         %s%p%s = \"%.20s...\"\n", 
           COLOR_CYAN, (void*)global_buffer, COLOR_RESET, global_buffer);
    printf("    Stack local:           %s%p%s = %d\n", 
           COLOR_YELLOW, (void*)&stack_var, COLOR_RESET, stack_var);
    printf("    Heap alloc:            %s%p%s = \"%s\"\n", 
           COLOR_YELLOW, heap_var, COLOR_RESET, (char*)heap_var);
    
    // Analizar mapas de memoria de ESTE proceso/thread
    printf("\n  📍 Análisis de /proc/%d/maps:\n", get_tgid());
    
    char maps_file[256];
    snprintf(maps_file, sizeof(maps_file), "/proc/%d/maps", get_tgid());
    FILE *fp = fopen(maps_file, "r");
    if (fp) {
        char line[512];
        int found_heap = 0;
        int found_stack = 0;
        
        while (fgets(line, sizeof(line), fp)) {
            // Buscar heap
            if (strstr(line, "[heap]")) {
                printf("    Heap:    %s", line);
                found_heap = 1;
                
                // Verificar si nuestro malloc está en esta región
                unsigned long heap_start, heap_end;
                if (sscanf(line, "%lx-%lx", &heap_start, &heap_end) == 2) {
                    unsigned long alloc_addr = (unsigned long)heap_var;
                    if (alloc_addr >= heap_start && alloc_addr < heap_end) {
                        printf("            ✅ Nuestro alloc está en este heap\n");
                    } else {
                        printf("            ⚠️  Nuestro alloc NO está en este heap\n");
                        printf("               (probablemente mmap'd por ser grande)\n");
                    }
                }
            }
            // Buscar stack
            else if (strstr(line, "[stack]")) {
                printf("    Stack:   %s", line);
                found_stack = 1;
            }
            // Buscar región de código
            else if (strstr(line, "/proc/self/exe") || strstr(line, ".so")) {
                // Mostrar solo la primera línea de código/libs
                static int code_shown = 0;
                if (!code_shown) {
                    printf("    Código:  %.60s...\n", line);
                    code_shown = 1;
                }
            }
        }
        
        if (!found_heap) {
            printf("    Heap:    (no se encontró sección [heap])\n");
            printf("             Puede que malloc use mmap para allocations grandes\n");
        }
        if (!found_stack) {
            printf("    Stack:   (no se encontró sección [stack])\n");
        }
        
        fclose(fp);
    } else {
        printf("    ❌ No se pudo abrir %s\n", maps_file);
    }
    
    free(heap_var);
}

// Función especial para mostrar diferencia padre/hijo después de COW
void demonstrate_cow_difference(int is_child) {
    print_timestamp("");
    printf("🔬 %s modificando variables para demostrar COW:\n", 
           is_child ? "HIJO" : "PADRE");
    
    // Guardar valores antiguos
    int old_global = global_var;
    char old_buffer[32];
    strncpy(old_buffer, global_buffer, 31);
    old_buffer[31] = '\0';
    
    // Modificar valores
    if (is_child) {
        global_var = 9999;
        strcpy(global_buffer, "MODIFICADO por el HIJO");
    } else {
        global_var = 7777;
        strcpy(global_buffer, "MODIFICADO por el PADRE");
    }
    
    printf("    global_var:    %d → %s%d%s\n", 
           old_global, COLOR_RED, global_var, COLOR_RESET);
    printf("    global_buffer: \"%.20s...\" → \"%s%.20s...%s\"\n", 
           old_buffer, COLOR_RED, global_buffer, COLOR_RESET);
    
    print_timestamp("");
    printf("✅ %s ahora tiene sus propias páginas (COW activado)\n",
           is_child ? "HIJO" : "PADRE");
}

// ========== FUNCIÓN DE THREAD ==========

void *thread_function(void *arg) {
    int thread_id = *(int*)arg;
    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "THREAD-%d", thread_id);
    
    // Cambiar nombre del thread (visible en ps)
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);
    
    usleep((rand() % 500) * 1000); // Delay aleatorio para demostrar concurrencia
    
    print_header(thread_name, 'M');
    
    print_memory_details(thread_name, 1);
    
    // Información de planificación
    print_timestamp("");
    printf("📊 Información de planificación:\n");
    
    int policy = sched_getscheduler(0);
    const char *policy_name = "Desconocida";
    switch(policy) {
        case SCHED_OTHER: policy_name = "SCHED_OTHER (normal)"; break;
        case SCHED_FIFO:  policy_name = "SCHED_FIFO (tiempo real)"; break;
        case SCHED_RR:    policy_name = "SCHED_RR (round-robin)"; break;
    }
    
    struct sched_param param;
    sched_getparam(0, &param);
    
    printf("  Política:   %s%s%s (%d)\n", COLOR_BLUE, policy_name, COLOR_RESET, policy);
    printf("  Prioridad:  %s%d%s\n", COLOR_BLUE, param.sched_priority, COLOR_RESET);
    
    errno = 0;
    int nice_val = getpriority(PRIO_PROCESS, 0);
    if (errno == 0) {
        printf("  Nice value: %s%d%s\n", COLOR_BLUE, nice_val, COLOR_RESET);
    }
    
    print_timestamp("");
    printf("⏸️  %s en pausa (esperando señal)...\n", thread_name);
    
    // Esperar señal de terminación
    while(1) {
        pause(); // Esperar señal
        break;
    }
    
    print_timestamp("");
    printf("✅ %s terminando.\n", thread_name);
    return NULL;
}

// ========== FUNCIÓN DE PROCESO HIJO ==========

void child_process_code() {
    prctl(PR_SET_NAME, "PROCESO-HIJO", 0, 0, 0);
    
    print_header("PROCESO HIJO (fork)", 'G');
    
    print_timestamp("");
    printf("👶 Proceso hijo creado:\n");
    printf("  TGID:  %s%d%s  (Thread Group ID)\n", COLOR_GREEN, get_tgid(), COLOR_RESET);
    printf("  PPID:  %s%d%s  (Parent TGID)\n", COLOR_GREEN, getppid(), COLOR_RESET);
    
    // Mostrar memoria inicial (compartida via COW)
    print_memory_details("PROCESO-HIJO (inicial)", 0);
    
    // Demostrar COW modificando variables
    demonstrate_cow_difference(1); // 1 = es hijo
    
    // Mostrar estado después de COW
    print_timestamp("");
    printf("\n📊 Estado después de COW:\n");
    printf("  El hijo ve: global_var = %d\n", global_var);
    printf("  El padre debería seguir viendo: global_var = 42\n");
    
    print_timestamp("");
    printf("⏸️  Proceso hijo en pausa (esperando SIGTERM)...\n");
    
    // Esperar señal del padre
    while(1) {
        pause();
        break;
    }
    
    print_timestamp("");
    printf("✅ Proceso hijo terminando.\n");
    exit(0);
}

// ========== FUNCIÓN PRINCIPAL ==========

int main() {
    srand(time(NULL));
    
    print_header("PROCESO PADRE INICIADO", 'B');
    
    print_timestamp("");
    printf("TGID (getpid): %s%d%s, PID kernel (gettid): %s%ld%s\n", 
           COLOR_GREEN, get_tgid(), COLOR_RESET,
           COLOR_GREEN, (long)get_kernel_pid(), COLOR_RESET);
    
    // ========== 1. CREAR PROCESO HIJO CON fork() ==========
    print_header("CREANDO PROCESO HIJO (fork)", 'R');
    
    print_timestamp("");
    printf("Ejecutando fork()...\n");
    pid_t child_pid = fork();
    
    if (child_pid == 0) {
        child_process_code();
    }
    
    print_timestamp("");
    printf("🔄 Proceso padre continúa\n");
    printf("   Hijo creado con TGID: %s%d%s\n", COLOR_GREEN, child_pid, COLOR_RESET);
    
    usleep(500000); // Dar tiempo al hijo para que muestre su info
    
    // ========== 2. CREAR THREADS CON pthread_create() ==========
    print_header("CREANDO THREADS (pthread_create)", 'Y');
    
    pthread_t threads[2];
    int thread_ids[2] = {1, 2};
    
    for (int i = 0; i < 2; i++) {
        print_timestamp("");
        printf("Creando thread %d...\n", thread_ids[i]);
        if (pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]) != 0) {
            perror("Error creando thread");
        }
        usleep(100000); // Pequeña pausa entre creación
    }
    
    // Dar tiempo a los threads para ejecutarse
    sleep(1);
    
    // ========== 3. PADRE MODIFICA DESPUÉS DEL HIJO ==========
    print_header("PADRE MODIFICA VARIABLES", 'C');
    
    print_timestamp("");
    printf("📊 Estado del padre ANTES de modificar:\n");
    printf("  global_var = %d (¿debería ser 42?)\n", global_var);
    printf("  global_buffer = \"%.20s...\"\n", global_buffer);
    
    // Demostrar que el padre también activa COW
    demonstrate_cow_difference(0); // 0 = es padre
    
    // ========== 4. MOSTRAR INFORMACIÓN DEL SISTEMA ==========
    print_header("INFORMACIÓN DEL SISTEMA", 'B');
    
    print_timestamp("");
    printf("📊 Estado desde /proc/%d/status:\n", get_tgid());
    char status_cmd[512];
    snprintf(status_cmd, sizeof(status_cmd),
             "echo '--- /proc/%d/status ---'; "
             "cat /proc/%d/status | grep -E '^(Pid|Tgid|PPid|Threads|State|VmSize|VmRSS):'",
             get_tgid(), get_tgid());
    system(status_cmd);
    
    print_timestamp("");
    printf("\n📊 Procesos y threads activos:\n");
    char ps_command[512];
    snprintf(ps_command, sizeof(ps_command),
             "echo '--- ps con threads ---'; "
             "ps -eLf | grep -E \"(%d|%d)\" | head -15",
             get_tgid(), child_pid);
    system(ps_command);
    
    print_timestamp("");
    printf("\n🌳 Árbol de procesos:\n");
    char tree_command[256];
    snprintf(tree_command, sizeof(tree_command),
             "pstree -p %d 2>/dev/null", get_tgid());
    system(tree_command);
    
    // ========== 5. DEMOSTRACIÓN PEDAGÓGICA ==========
    print_header("RESUMEN DE LA DEMOSTRACIÓN", 'G');
    
    printf("\n%s🎯 DIFERENCIAS CLAVE DEMOSTRADAS:%s\n", COLOR_BOLD, COLOR_RESET);
    
    printf("\n%s🏷️  NOMENCLATURA CORRECTA:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("  • getpid() → Thread Group ID (TGID) = \"Process ID\" para userspace\n");
    printf("  • gettid() → PID kernel = Identificador único de tarea en el kernel\n");
    printf("  • MISMO TGID = mismo grupo de threads = memoria compartida\n");
    printf("  • TGID DIFERENTE = proceso diferente = memoria separada (COW)\n");
    
    printf("\n%s1. PROCESOS (fork()):%s\n", COLOR_BOLD, COLOR_RESET);
    printf("   • %sTGID diferente%s: Padre=%d, Hijo=%d\n", COLOR_GREEN, COLOR_RESET, get_tgid(), child_pid);
    printf("   • %sCopy-On-Write (COW)%s: Memoria separada al modificar\n", COLOR_GREEN, COLOR_RESET);
    printf("   • %sAislamiento%s: Hijo ve global_var=%d, Padre ve global_var=%d\n", 
           COLOR_GREEN, COLOR_RESET, 9999, global_var);
    
    printf("\n%s2. THREADS (pthread_create()):%s\n", COLOR_BOLD, COLOR_RESET);
    printf("   • %sMismo TGID%s: Todos comparten TGID=%d\n", COLOR_GREEN, COLOR_RESET, get_tgid());
    printf("   • %sPIDs kernel diferentes%s: Cada thread tiene PID único\n", COLOR_GREEN, COLOR_RESET);
    printf("   • %sMemoria compartida%s: Mismas direcciones de variables globales\n", COLOR_GREEN, COLOR_RESET);
    printf("   • %sStacks separados%s: Cada thread tiene stack propio\n", COLOR_GREEN, COLOR_RESET);
    
    printf("\n%s3. VERIFICACIÓN DESDE OTRA TERMINAL:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("   # Ver threads del proceso padre:\n");
    printf("   %sls -la /proc/%d/task/%s\n", COLOR_CYAN, get_tgid(), COLOR_RESET);
    
    printf("   \n   # Comparar heaps (deberían ser diferentes):\n");
    printf("   %scat /proc/%d/maps | grep heap%s\n", COLOR_CYAN, get_tgid(), COLOR_RESET);
    printf("   %scat /proc/%d/maps | grep heap%s\n", COLOR_CYAN, child_pid, COLOR_RESET);
    
    printf("   \n   # Ver valores de variables en cada proceso:\n");
    printf("   %ssudo gdb -p %d -ex \"p global_var\" -ex \"p global_buffer\" -ex \"q\"%s\n", 
           COLOR_CYAN, get_tgid(), COLOR_RESET);
    printf("   %ssudo gdb -p %d -ex \"p global_var\" -ex \"p global_buffer\" -ex \"q\"%s\n", 
           COLOR_CYAN, child_pid, COLOR_RESET);
    
    // ========== 6. INTERACCIÓN DEL USUARIO ==========
    print_header("CONTROL DEL PROGRAMA", 'R');
    
    printf("\n%s🎯 PROCESOS ACTIVOS:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("   • Proceso Padre:  TGID %s%d%s\n", COLOR_GREEN, get_tgid(), COLOR_RESET);
    printf("   • Proceso Hijo:   TGID %s%d%s (en pausa)\n", COLOR_GREEN, child_pid, COLOR_RESET);
    printf("   • Threads:        2 threads activos\n");
    
    printf("\n%s🛑 OPCIONES DE TERMINACIÓN:%s\n", COLOR_BOLD, COLOR_RESET);
    printf("   1. %sPresiona ENTER%s - Terminar limpiamente (recomendado)\n", COLOR_GREEN, COLOR_RESET);
    printf("   2. %sCtrl+C%s - Terminación forzada\n", COLOR_RED, COLOR_RESET);
    printf("   3. %sDesde otra terminal%s:\n", COLOR_YELLOW, COLOR_RESET);
    printf("      kill %d          # Terminar proceso padre\n", get_tgid());
    printf("      kill -TERM -%d   # Terminar todo el árbol de procesos\n", get_tgid());
    
    printf("\n%s⏎ Presiona ENTER para terminar limpiamente...%s\n", COLOR_BOLD, COLOR_RESET);
    getchar();
    
    // ========== 7. LIMPIEZA ==========
    print_header("TERMINANDO PROCESOS", 'R');
    
    print_timestamp("");
    printf("🧹 Limpiando recursos...\n");
    
    // Terminar proceso hijo
    if (child_pid > 0) {
        print_timestamp("");
        printf("Enviando SIGTERM al proceso hijo %d...\n", child_pid);
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        print_timestamp("");
        printf("✅ Proceso hijo terminado\n");
    }
    
    // Terminar threads
    print_timestamp("");
    printf("Terminando threads...\n");
    for (int i = 0; i < 2; i++) {
        print_timestamp("");
        printf("  Enviando señal al thread %d...\n", i+1);
        pthread_cancel(threads[i]);
        pthread_join(threads[i], NULL);
        print_timestamp("");
        printf("  ✅ Thread %d terminado\n", i+1);
    }
    
    print_header("DEMOSTRACIÓN COMPLETADA", 'G');
    print_timestamp("");
    printf("✅ Todos los procesos e hilos terminados limpiamente.\n\n");
    
    print_timestamp("");
    printf("📚 RESUMEN DE LO APRENDIDO:\n");
    print_timestamp("");
    printf("  1. fork() crea procesos con memoria separada (COW)\n");
    print_timestamp("");
    printf("  2. pthread_create() crea threads que comparten memoria\n");
    print_timestamp("");
    printf("  3. getpid() devuelve TGID (igual para todos los threads)\n");
    print_timestamp("");
    printf("  4. gettid() devuelve PID kernel (único para cada thread)\n");
    print_timestamp("");
    printf("  5. TGID diferente = procesos diferentes\n");
    print_timestamp("");
    printf("  6. Mismo TGID = threads del mismo proceso\n");
    
    return 0;
}