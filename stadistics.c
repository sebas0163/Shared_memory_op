#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>


//Leer los datos de la memoria 

//imprimir los datos  

//conversión a segundos

// sincronización

struct rusage ru;
void prueba(){
    int i = 5000;
    int j =0;
    while (i !=0){
        j ++;
        i --;
        printf("Numero %d\n", j);
    }
}

int main(){
    prueba();
    getrusage(RUSAGE_SELF, &ru);
    long t_us =  ru.ru_utime.tv_usec;
    int t_ker = ru.ru_stime.tv_usec;
    printf("valor del usuario %ld\n",t_us);
    printf("valor kernel %d\n", t_ker);
}