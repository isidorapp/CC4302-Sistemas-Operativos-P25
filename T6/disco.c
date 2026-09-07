#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pss.h"
#include "spinlocks.h"

typedef struct request {
    char *nombre;        // Nombre de la persona esperando
    char **ppareja;      // Dirección donde se escribirá el nombre de su pareja
    int espera;          // Spinlock personal
    struct request *sig; // Siguiente
} Request;

// Colas de espera
Request *cola_damas_ini;
Request *cola_damas_fin;
Request *cola_varones_ini;
Request *cola_varones_fin;

// Spinlock global
int disco_lock;

void encolar(Request **ini, Request **fin, Request *r) {
    r->sig = NULL;
    if (*fin == NULL)
        *ini = *fin = r;
    else {
        (*fin)->sig = r;
        *fin = r;
    }
}

Request *desencolar(Request **ini, Request **fin) {
    Request *r = *ini;
    if (r == NULL) return NULL;
    *ini = r->sig;
    if (*ini == NULL) *fin = NULL;
    return r;
}

void discoInit() {
    disco_lock = OPEN;
    cola_damas_ini = cola_damas_fin = NULL;
    cola_varones_ini = cola_varones_fin = NULL;
}

void discoDestroy() {
    spinLock(&disco_lock);
    Request *r;
    while ((r = desencolar(&cola_damas_ini, &cola_damas_fin)) != NULL)
        free(r);
    while ((r = desencolar(&cola_varones_ini, &cola_varones_fin)) != NULL)
        free(r);
    spinUnlock(&disco_lock);
}

char *llegada(char *nombre, Request **mi_ini, Request **mi_fin, Request **op_ini, Request **op_fin) {

    char *pareja_local = NULL;

    Request *self = malloc(sizeof(Request));
    self->nombre = nombre;
    self->ppareja = &pareja_local;
    self->espera = CLOSED;
    self->sig = NULL;

    spinLock(&disco_lock);

    Request *op = desencolar(op_ini, op_fin);
    if (op != NULL) {
        // Emparejar
        char *ret = op->nombre;     
        *(op->ppareja) = nombre;    
        spinUnlock(&op->espera);    
        free(self);
        spinUnlock(&disco_lock);
        return ret;
    }

    // Si no hay nadie esperando
    encolar(mi_ini, mi_fin, self);
    spinUnlock(&disco_lock);

    spinLock(&self->espera);
    char *ret = pareja_local;
    free(self);
    return ret;
}

char *dama(char *nom) {
    return llegada(nom, &cola_damas_ini, &cola_damas_fin, &cola_varones_ini, &cola_varones_fin);
}

char *varon(char *nom) {
    return llegada(nom, &cola_varones_ini, &cola_varones_fin, &cola_damas_ini, &cola_damas_fin);
}

