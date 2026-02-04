#!/bin/bash
# analyze_manual.sh - Comandos para análisis manual

echo "=== ANÁLISIS MANUAL DE PROCESOS ==="
echo ""

# Buscar nuestro proceso
PID=$(pgrep proc_analysis | head -1)

if [ -z "$PID" ]; then
    echo "❌ No se encontró proceso proc_analysis"
    echo ""
    echo "Ejecuta primero en una terminal:"
    echo "  ./proc_analysis"
    echo ""
    echo "Luego en esta terminal:"
    echo "  ./analyze_manual.sh"
    exit 1
fi

echo "📌 Proceso encontrado: PID=$PID"
echo ""

while true; do
    echo ""
    echo "=== MENÚ DE ANÁLISIS ==="
    echo "1. Ver información básica del proceso"
    echo "2. Ver mapas de memoria"
    echo "3. Ver hilos"
    echo "4. Ver archivos abiertos"
    echo "5. Ver estado de planificación"
    echo "6. Hexdump de secciones .text"
    echo "7. Comparar con otros procesos"
    echo "8. Salir"
    echo ""
    read -p "Selecciona una opción (1-8): " choice
    
    case $choice in
        1)
            echo ""
            echo "=== INFORMACIÓN BÁSICA ==="
            cat /proc/$PID/status 2>/dev/null | grep -E "^(Name|State|Pid|Tgid|PPid|Threads):" || echo "No disponible"
            ;;
        2)
            echo ""
            echo "=== MAPAS DE MEMORIA ==="
            echo "Resumen de secciones importantes:"
            grep -E "(heap|stack|/proc/$PID/exe)" /proc/$PID/maps 2>/dev/null || echo "No disponible"
            echo ""
            echo "¿Ver mapa completo? (s/n): "
            read view_full
            if [ "$view_full" = "s" ]; then
                cat /proc/$PID/maps 2>/dev/null | less || echo "No disponible"
            fi
            ;;
        3)
            echo ""
            echo "=== HILOS DEL PROCESO ==="
            echo "Total de hilos: $(ls /proc/$PID/task/ 2>/dev/null | wc -l)"
            echo ""
            echo "Lista de hilos:"
            for TID in $(ls /proc/$PID/task/ 2>/dev/null); do
                echo "  TID: $TID, Estado: $(cat /proc/$PID/task/$TID/status 2>/dev/null | grep State: | cut -f2 || echo 'N/A')"
            done
            ;;
        4)
            echo ""
            echo "=== ARCHIVOS ABIERTOS ==="
            ls -la /proc/$PID/fd/ 2>/dev/null || echo "No disponible"
            ;;
        5)
            echo ""
            echo "=== ESTADO DE PLANIFICACIÓN ==="
            if [ -f "/proc/$PID/sched" ]; then
                echo "Política: $(grep policy /proc/$PID/sched 2>/dev/null || echo 'No disponible')"
                echo "Prioridad: $(grep prio /proc/$PID/sched 2>/dev/null | head -1 || echo 'No disponible')"
            else
                echo "Info de planificación requiere permisos de root"
                echo "Ejecuta: sudo cat /proc/$PID/sched"
            fi
            ;;
        6)
            echo ""
            echo "=== HEXDUMP DE SECCIONES ==="
            echo "Para ver .text del ejecutable:"
            echo "  objdump -s -j .text proc_analysis | head -30"
            echo ""
            echo "Para ver .data (variables inicializadas):"
            echo "  objdump -s -j .data proc_analysis | head -20"
            ;;
        7)
            echo ""
            echo "=== COMPARACIÓN CON OTROS PROCESOS ==="
            echo "Procesos similares en el sistema:"
            ps aux | grep -E "(proc_analysis|PID)" | head -10
            ;;
        8)
            echo "Saliendo..."
            exit 0
            ;;
        *)
            echo "Opción inválida"
            ;;
    esac
done
