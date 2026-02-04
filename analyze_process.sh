#!/bin/bash
# Script para analizar el proceso en ejecución - Versión simplificada

if [ -z "$1" ]; then
    # Si no se proporciona PID, buscar proc_analysis
    PID=$(pgrep proc_analysis | head -1)
    if [ -z "$PID" ]; then
        echo "❌ No se encontró proceso proc_analysis en ejecución"
        echo ""
        echo "Primero ejecuta: ./proc_analysis &"
        echo "Luego: ./analyze_process.sh"
        exit 1
    fi
else
    PID=$1
fi

echo "=== ANÁLISIS DEL PROCESO $PID ==="
echo ""

# Verificar que el proceso existe
if [ ! -d "/proc/$PID" ]; then
    echo "❌ Proceso $PID no existe o ya terminó"
    exit 1
fi

# Información básica
echo "1. INFORMACIÓN BÁSICA:"
echo "----------------------"
echo "PID: $PID"
echo "Nombre: $(cat /proc/$PID/comm 2>/dev/null || echo 'No disponible')"
echo "Estado: $(cat /proc/$PID/status 2>/dev/null | grep State: | cut -f2 || echo 'No disponible')"
echo "TGID: $(cat /proc/$PID/status 2>/dev/null | grep Tgid: | cut -f2 || echo 'No disponible')"
echo "PPID (Padre): $(cat /proc/$PID/status 2>/dev/null | grep PPid: | cut -f2 || echo 'No disponible')"
echo "Número de hilos: $(ls /proc/$PID/task/ 2>/dev/null | wc -l || echo '0')"
echo ""

# Memoria
echo "2. RESUMEN DE MEMORIA:"
echo "----------------------"
echo "Límites de memoria:"
cat /proc/$PID/limits 2>/dev/null | grep "Max address space" || true
echo ""

echo "Mapas de memoria (resumen):"
echo "[Code]    $(grep -E "/proc/$PID/exe" /proc/$PID/maps 2>/dev/null | head -1 || echo 'No disponible')"
echo "[Heap]    $(grep heap /proc/$PID/maps 2>/dev/null || echo 'No disponible')"
echo "[Stack]   $(grep stack /proc/$PID/maps 2>/dev/null || echo 'No disponible')"
echo ""

# Hilos
echo "3. HILOS (si hay más de 1):"
echo "---------------------------"
THREAD_COUNT=$(ls /proc/$PID/task/ 2>/dev/null | wc -l)
if [ $THREAD_COUNT -gt 1 ]; then
    echo "Lista de TIDs:"
    ls /proc/$PID/task/ 2>/dev/null | xargs -I {} sh -c 'echo "  TID: {}, Nombre: $(cat /proc/$PID/task/{}/comm 2>/dev/null || echo "N/A")"'
else
    echo "Solo hay 1 hilo (proceso principal)"
fi
echo ""

# Archivos abiertos
echo "4. ARCHIVOS ABIERTOS (primeros 5):"
echo "----------------------------------"
ls -la /proc/$PID/fd/ 2>/dev/null | head -6 || echo "No disponible o requiere permisos"
echo ""

echo "=== FIN DEL ANÁLISIS ==="
echo ""
echo "Comandos adicionales para profundizar:"
echo "  cat /proc/$PID/maps          # Ver mapa completo de memoria"
echo "  cat /proc/$PID/status        # Ver estado completo"
echo "  ls -la /proc/$PID/fd/        # Ver todos los archivos abiertos"
echo "  sudo cat /proc/$PID/sched    # Ver info de planificación (necesita root)"
