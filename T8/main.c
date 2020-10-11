#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "queue.h"

#define CAJEROS_NORMALES  5
#define CAJEROS_EMPRESARIALES  3
#define CLIENTES_NORMALES  100
#define CLIENTES_EMPRESARIALES 50

struct Cajero
{
    char* ID;
    char* tipoCajero;
    int numeroClientesAtendidos;
    int atendidosTotales;
};

struct Cliente
{
    char* ID;
    int tipoCliente;
};

int atendidos = 0;
int atendidosEm = 0;
pthread_mutex_t mutexFilaNormal = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexFilaEmpresariales = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexRevisionCajeros = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAsignacionClienteCajero = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAsignacionClienteCajeroEmp = PTHREAD_MUTEX_INITIALIZER;

node_t* inicioColaNormal = NULL;
node_t* inicioColaEmpresarial = NULL;

void* filaBancoNormal(void*);
void* filaBancoEmpresarial(void*);
void* cajeroNormal(void*);
void* cajeroEmpresarial(void*);
struct Cajero* crearCajero(char*, int);
struct Cliente* crearCliente(int, int);
void descansarCajero(struct Cajero*);
void atenderCliente(struct Cajero*, struct Cliente*);
struct Cliente* asignarCliente(void*, struct Cajero*);

int main(int args, char* const* argv)
{
    srand(time(0));
    
    //Creacion Hilos Cajeros
    pthread_t* hilosCajeros = (pthread_t *) malloc((CAJEROS_EMPRESARIALES + CAJEROS_NORMALES) * sizeof(pthread_t));
    pthread_t* auxCajeros = hilosCajeros;
    int j = 0;
    for(int i = 0; i<(CAJEROS_NORMALES + CAJEROS_EMPRESARIALES); i++)
    {
        if(i<CAJEROS_NORMALES)
        {
            pthread_create(auxCajeros, NULL, cajeroNormal, (void*) i);
        }
        else
        {
            pthread_create(auxCajeros, NULL, cajeroEmpresarial, (void*) j);
            j++;
        }
        auxCajeros++;
    }

    //Creacion Hilos Colas
    pthread_t* hilosColas = (pthread_t *) malloc(2 * sizeof(pthread_t));
    pthread_t* auxHilos = hilosColas;
    pthread_create(auxHilos, NULL, filaBancoNormal, (void*) 0);
    auxHilos++;
    pthread_create(auxHilos, NULL, filaBancoEmpresarial, (void*) 0);
    
    //Join Hilos Colas
    auxHilos = hilosColas;
    pthread_join(*auxHilos, NULL);
    auxHilos++;
    pthread_join(*auxHilos, NULL);
    
    //Join Hilos Cajeros
    auxCajeros = hilosCajeros;
    for(int i = 0; i<(CAJEROS_NORMALES + CAJEROS_EMPRESARIALES); i++)
    {
        pthread_join(*auxCajeros, NULL);
        auxCajeros++;
    }

    printf("Clientes Normales atendidos: %d --- Clientes Empresariales atendidos: %d. \n", atendidos, atendidosEm);
    free(hilosCajeros);
    return 0;
}

void* cajeroNormal(void* arg)
{
    struct Cajero* cajero = crearCajero("Normal", (int)arg);
    while(1)
    {
        pthread_mutex_lock(&mutexRevisionCajeros);
        int aten = atendidos;
        int atenEmp = atendidosEm;
        pthread_mutex_unlock(&mutexRevisionCajeros);
        double porcentajeParticipacion = 0.0;
        struct Cliente* cliente;
        void* clienteAAsignar = NULL;
        if(aten < CLIENTES_NORMALES)
        {
            if(cajero->numeroClientesAtendidos == 5)
            {
                descansarCajero(cajero);
            }
            else
            {
                porcentajeParticipacion = (double)cajero->atendidosTotales / (double)aten;
                if(porcentajeParticipacion <= .16 || aten == 0)
                {
                    pthread_mutex_lock(&mutexAsignacionClienteCajero);
                    clienteAAsignar=dequeue(&inicioColaNormal);
                    if(clienteAAsignar!=NULL)
                    {  
                        cliente = asignarCliente(clienteAAsignar, cajero);
                        atendidos = atendidos + 1;
                        (cajero->atendidosTotales++);
                    }
                    pthread_mutex_unlock(&mutexAsignacionClienteCajero);
                }
                if(clienteAAsignar == NULL)
                {
                    continue;
                }
                else
                {
                    atenderCliente(cajero, cliente);
                    usleep(15);
                }
            }
        }
        else
        {
            pthread_exit(0);
        } 
    }
}

void* cajeroEmpresarial(void* arg)
{
    struct Cajero* cajero = crearCajero("Empresarial", (int)arg);
    while(1)
    {
        pthread_mutex_lock(&mutexRevisionCajeros);
        int aten = atendidos;
        int atenEmp = atendidosEm;
        pthread_mutex_unlock(&mutexRevisionCajeros);
        double porcentajeParticipacion = 0.0;
        if(aten < CLIENTES_NORMALES || atenEmp < CLIENTES_EMPRESARIALES)
        {
            if(cajero->numeroClientesAtendidos == 5)
            {
                descansarCajero(cajero);
            }
            else
            {
                pthread_mutex_lock(&mutexAsignacionClienteCajeroEmp);
                void* clienteAAsignar;
                void* clienteAAsignarRespaldo = NULL;
                struct Cliente* cliente;
                clienteAAsignar=dequeue(&inicioColaEmpresarial);
                if(clienteAAsignar!=NULL)
                {  
                    cliente = asignarCliente(clienteAAsignar, cajero);
                    atendidosEm++;
                }
                pthread_mutex_unlock(&mutexAsignacionClienteCajeroEmp);
                if(clienteAAsignar==NULL && aten > 0)
                {
                    pthread_mutex_lock(&mutexAsignacionClienteCajero);
                    porcentajeParticipacion = (double)cajero->atendidosTotales / (double)aten;
                    if(porcentajeParticipacion <= .07)
                    {
                        clienteAAsignar = dequeue(&inicioColaNormal);
                        if(clienteAAsignar!=NULL)
                        {
                            cliente = asignarCliente(clienteAAsignar, cajero);
                            atendidos++;
                            (cajero->atendidosTotales++);
                        }
                    }
                    pthread_mutex_unlock(&mutexAsignacionClienteCajero);
                }
                if(clienteAAsignar == NULL && clienteAAsignarRespaldo == NULL)
                {
                    continue;
                }
                else
                {
                    atenderCliente(cajero, cliente);
                    usleep(95);
                } 
            }
        }
        else
        {
            pthread_exit(0);
        }
    }
}

void* filaBancoNormal(void* arg)
{
    for(int i = 0; i<CLIENTES_NORMALES; i++)
    {   
        int waitTime = ((rand() %  (22 - 5 + 1)) + 5) * 1000000; 

        pthread_mutex_lock(&mutexFilaNormal);
        
        usleep(waitTime);
        struct Cliente* cliente = crearCliente((i + 1), 0);
        enqueue(&inicioColaNormal, cliente);

        pthread_mutex_unlock(&mutexFilaNormal);
    }
    pthread_exit(0);
}

void* filaBancoEmpresarial(void* arg)
{
    for(int i = 0; i<CLIENTES_EMPRESARIALES; i++)
    {
        int waitTime = ((rand() %  (34 - 9 + 1)) + 9) * 1000000; 

        pthread_mutex_lock(&mutexFilaEmpresariales);

        usleep(waitTime);
        struct Cliente* cliente = crearCliente((i + 1), 1);
        enqueue(&inicioColaEmpresarial, cliente);

        pthread_mutex_unlock(&mutexFilaEmpresariales);
    }
    pthread_exit(0);
}

struct Cajero* crearCajero(char* tipo, int id)
{
    struct Cajero* cajero = (struct Cajero*)malloc(1 * sizeof(struct Cajero));
    cajero->numeroClientesAtendidos = 0;
    cajero->tipoCajero = tipo;
    cajero->atendidosTotales = 0;
    char* inicio = (char*)malloc(25 * sizeof(char));
    char *numeroCajero = (char*)malloc(2 * sizeof(char));
    sprintf(inicio, "%s%d", cajero->tipoCajero, id);
    cajero->ID = inicio;
    printf("%s %s\n", cajero->tipoCajero, cajero->ID);
    return cajero;
}

struct Cliente* crearCliente(int id, int tipo)
{
    struct Cliente* cliente = (struct Cliente*)malloc(1 * sizeof(struct Cliente));
    cliente->tipoCliente = tipo;
    char* inicio = (char*)malloc(25 * sizeof(char));
    char *numeroCliente = (char*)malloc(2 * sizeof(char));
    char* tipoo;
    if(tipo==0)
    {
        tipoo = "Normal";
    }
    else
    {
        tipoo = "Empresarial";
    }
    sprintf(inicio, "%s%d%s", "Cliente", id, tipoo);
    cliente->ID = inicio;
    printf("%s %d\n", cliente->ID, cliente->tipoCliente);
    return cliente;
}

void descansarCajero(struct Cajero* cajero)
{
    printf("Cajero %s se va a descansar\n", cajero->ID);
    usleep(3000000);
    cajero->numeroClientesAtendidos = 0;
    printf("Cajero %s ha regresado de descansar\n", cajero->ID);
    return;
}

void atenderCliente(struct Cajero* cajero, struct Cliente* cliente)
{
    printf("Cajero %s esta atendiendo a cliente %s\n", cajero->ID, cliente->ID);
    int waitTime = ((rand() %  (5 - 3 + 1)) + 3) * 1000000;
    usleep(waitTime);
    printf("Cajero %s y cliente %s han acabado su operacion\n", cajero->ID, cliente->ID);
    return;
}

struct Cliente* asignarCliente(void* clienteAAsignar, struct Cajero* cajero)
{
    struct Cliente* cliente;
    cliente =((struct Cliente*)clienteAAsignar);
    (cajero->numeroClientesAtendidos)++;
    return cliente; 
}
