#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>


#define MAX_SECTORS 32

static volatile int keepRunning = 1;

// Структура для общей памяти
// sectors: 0 - еще не проверили, 1 - проверяют, 2 - нашли Винни Пуха
typedef struct shared_memory {
  int semid;
  int sectors[MAX_SECTORS];
  int sectors_count;
  int pooh_location;
  int found;
}shared_memory;

// Функция обработки прерывания
void intHandler(int dummy) {
    printf("SIGINT!\n");
    keepRunning = 0;
}

// Функция для уменьшения значения семафора на 1
void sem_close(int semid) {
 struct sembuf parent_buf = {
    .sem_num = 0,
    .sem_op = -1,
    .sem_flg = 0
  };
  if (semop(semid, & parent_buf, 1) < 0) {
    printf("Can\'t sub 1 from semaphor\n");
    exit(-1);
  }
}

// Функция для увеличения семафора на 1
void sem_open(int semid) {
 struct sembuf parent_buf = {
    .sem_num = 0,
    .sem_op = 1,
    .sem_flg = 0
  };
  if (semop(semid, & parent_buf, 1) < 0) {
    printf("Can\'t add 1 to semaphor\n");
    exit(-1);
  }  
}

int main(int argc, char ** argv) {
  if (argc < 2) {
    printf("usage: ./main <sectors count>");
    return -1;
  }
  signal(SIGINT, intHandler);
  int sectors_count = atoi(argv[1]);
  if (sectors_count > MAX_SECTORS) {
    printf("Incorrect sectors count! max: %d\n", MAX_SECTORS);
    return 1;
  }
  key_t key = ftok("hive.c", 0);
  shared_memory *shmptr;
  int semid;
  int shmid;

  // Создаем память и семафор
  if ((semid = semget(key, 1, 0666 | IPC_CREAT)) < 0) {
    printf("Can\'t create semaphore\n");
    return -1;
  }

  if ((shmid = shmget(key, sizeof(shared_memory), 0666 | IPC_CREAT)) < 0) {
    printf("Can\'t create shmem\n");
    return -1;
  }
  if ((shmptr = (shared_memory*) shmat(shmid, NULL, 0)) == (shared_memory *) -1) {
    printf("Cant shmat!\n");
    return -1;
  }

  semctl(semid, 0, SETVAL, 1);
  
  
  shmptr->sectors_count = sectors_count;
  shmptr->pooh_location = rand() % sectors_count;
  shmptr->found = 0;
  shmptr->semid = semid;
  for (int i = 0; i < sectors_count; i++) {
    shmptr->sectors[i] = 0;
  }

  printf("Pooh is hiding in sector #%d\n", shmptr->pooh_location);
  while (keepRunning) {
    sleep(1);
  }

  // Удаляем семафоры
  if (semctl(semid, 0, IPC_RMID, 0) < 0) {
    printf("Can\'t delete semaphore\n");
    return -1;
  }
  // Удаляем память
  shmdt(shmptr);
  shmctl(shmid, IPC_RMID, NULL);
  return 0;
}