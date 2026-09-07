#include <stdio.h>
#include <nthread-impl.h>
#include "pss.h"
#include "pedir.h"


// Variables globales
NthQueue *espera[2]; // Arreglo de dos colas
int recurso_oc; // 0 libre, 1 ocupado
int owner; // Thread dueño de la categoría

// Función auxiliar para manejar threads
void timeout_handler(nThread th) {
    // Eliminar el thread de ambas colas
    nth_delQueue(espera[0], th);
    nth_delQueue(espera[1], th);
    
    // Borrar la marca
    th->ptr = NULL;
}


void nth_iniciar() {
    // Inicializar colas
    espera[0] = nth_makeQueue();
    espera[1] = nth_makeQueue();
    
    // Inicializar variables (recurso libre)
    recurso_oc = 0;
    owner = -1;
}

void nth_terminar() {
    // Limpiar colas
    nth_destroyQueue(espera[0]);
    nth_destroyQueue(espera[1]);
}

int nPedir(int cat, int timeout) {
    // Iniciar sección crítica
    START_CRITICAL;
    
    // Obtener thread actual 
    nThread self = nSelf();
    
    // Caso recurso ocupado
    if (recurso_oc) {
      if (timeout < 0) { // Sin timeout
        // Encolar thread al final de su categoría
        nth_putBack(espera[cat], self);
        // Suspender thread actual
        suspend(WAIT_REQUEST); 
        // Llamado a scheduler para ejecutar otro thread
        schedule();
        // Al despertar, nDevolver nos dio el recurso
        
      } else { // Con timeout
        // Marcar
        self->ptr = self;
        
        // Encolar thread al final de su categoría
        nth_putBack(espera[cat], self);
        // Suspender thread actual
        suspend(WAIT_REQUEST_TIMEOUT); 
        nth_programTimer(timeout * 1000000LL, timeout_handler);
        
        // Llamado a scheduler
        schedule();
        
        // Verificar la marca
        if (self->ptr == NULL) { // Si es NULL, se acabó y borró
          END_CRITICAL;
          return 0;
        } else { // Si no es NULL, se despertó
          nth_cancelThread(self); 
          // Limpiar la marca
          self->ptr = NULL;
        }
      }
      
    } else { // Caso recurso libre
      // Marcar como ocupado
      recurso_oc = 1;
      // Marcar categoría actual como dueña del recurso
      owner = cat;
    }
    
    // Salir de sección crítica
    END_CRITICAL;
    return 1;
}

void nDevolver() {
    // Iniciar sección crítica
    START_CRITICAL;
    
    // Caso recurso libre
    if (!recurso_oc) {
        END_CRITICAL;
        return;
    } 
    
    // Categorías opuestas 0 o 1
    int cat_opuesta = 1 - owner;
    // Puntero al siguiente thread para despertar
    nThread siguiente = NULL;

    // Casos recurso ocupado
    if (!nth_emptyQueue(espera[cat_opuesta])) {
        // Si hay threads esperando de la categoría opuesta
        // Sacar el primero de la cola opuesta
        siguiente = nth_getFront(espera[cat_opuesta]);
        // Marcar la otra categoría como dueña
        owner = cat_opuesta;
    }
    // Categoría actual
    else if (!nth_emptyQueue(espera[owner])) {
        // Si no hay threads de categoría opuesta pero sí
        // De la actual, sacar el primer thread de la cola
        siguiente = nth_getFront(espera[owner]);
    }
    // No hay categorías en espera
    else {
        // El recurso está libre
        recurso_oc = 0;
        // No hay dueño
        owner = -1;
        // Fin sección crítica
        END_CRITICAL;
        return;
    }
    
    // Si el thread a despertar tiene marca, está esperando con timeout
    if (siguiente->ptr != NULL) { 
        // Cancelamos el timer antes de setReady
        nth_cancelThread(siguiente);
        
    }
    // Si hay siguiente thread, despertarlo
    setReady(siguiente); 
    // Llamado a scheduler
    schedule();
    
    // Fin sección crítica
    END_CRITICAL;
}
