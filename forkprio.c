#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/times.h>      // times()
#include <sys/time.h>       // getrusage()
#include <sys/resource.h>   // setpriority(), getpriority(), getrusage()
#include <sys/types.h>
#include <sys/wait.h>

/* Manejador de SIGTERM en los hijos */
static void sigterm_handler(int signo)
{
    (void) signo;

    /* Obtener tiempo de CPU consumido (user + system) en segundos */
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    long secs = ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;

    int prio = getpriority(PRIO_PROCESS, 0);

    printf("Child %d (nice %2d):\t%3li\n", getpid(), prio, secs);
    fflush(stdout);
    exit(EXIT_SUCCESS);
}

int busywork(void)
{
    struct tms buf;
    for (;;) {
        times(&buf);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <num_hijos> <segundos> <prioridad>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int  n_children = atoi(argv[1]);
    int  secs       = atoi(argv[2]);
    int  use_prio   = atoi(argv[3]);

    if (n_children <= 0) {
        fprintf(stderr, "El número de hijos debe ser mayor a 0\n");
        exit(EXIT_FAILURE);
    }

    pid_t *pids = malloc((size_t) n_children * sizeof(pid_t));
    if (!pids) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n_children; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            /* --- Proceso hijo --- */

            /* Registrar manejador de SIGTERM */
            struct sigaction sa;
            sa.sa_handler = sigterm_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGTERM, &sa, NULL);

            /* Ajustar prioridad si se pidió */
            if (use_prio) {
                if (setpriority(PRIO_PROCESS, 0, i) == -1) {
                    perror("setpriority");
                }
            }

            /* Consumir CPU indefinidamente hasta recibir SIGTERM */
            busywork();//comentar
            exit(EXIT_SUCCESS);   /* nunca llega aquí */
        }

        /* --- Proceso padre: guarda el PID del hijo --- */
        pids[i] = pid;
    }

    /* El padre duerme el tiempo indicado (0 = indefinido) */
    if (secs > 0) {
        sleep((unsigned int) secs);

        /* Enviar SIGTERM a todos los hijos */
        for (int i = 0; i < n_children; i++) {
            kill(pids[i], SIGTERM);
        }

        /* Esperar a que todos los hijos terminen */
        for (int i = 0; i < n_children; i++) {
            wait(NULL);
        }
    } else {
        /* Ejecutar indefinidamente: esperar a que los hijos terminen
         * (alguien externo deberá enviar la señal) */
        for (int i = 0; i < n_children; i++) {
            wait(NULL);
        }
    }

    free(pids);
    exit(EXIT_SUCCESS);
}
