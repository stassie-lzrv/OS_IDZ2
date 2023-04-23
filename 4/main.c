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


// Функция для Стаи пчел
void child(int semid, shared_memory* shmptr)  {
  srand(time(NULL) ^ (getpid()<<16));
  printf("I am a bee #%d!\n", getpid());
  sleep(1);
  int selected_sector = -1;
  while (!shmptr->found) {
    sem_close(semid);
    for (int i = 0; i < shmptr->sectors_count; i++) {
      if (shmptr->sectors[i] == 0) {
        selected_sector = i;
        shmptr->sectors[i] = 1;
        break;
      }
    }
    sem_open(semid);
    if (selected_sector == -1) {
       printf("[%d] No free secotrs to check\n", getpid());
       break;
    }
    printf("[%d] Checking sector #%d\n", getpid(), selected_sector);
    sleep(2 + rand() % 5);
    sem_close(semid);
    if (shmptr->pooh_location == selected_sector) {
      shmptr->sectors[selected_sector] = 2;
      printf("[%d] Found the Pooh in sector #%d\n", getpid(), selected_sector);
      shmptr->found = 1;
      sem_open(semid);
      break;
    } else {
      printf("[%d] Did not found the Pooh in sector #%d\n", getpid(), selected_sector);
    }
    sem_open(semid);
    selected_sector = -1;
    sleep(1 + rand() % 5);
  }
  exit(0);
}



int main(int argc, char ** argv) {
  if (argc < 3) {
    printf("usage: ./main <sectors count> <bee count>");
    return -1;
  }
  signal(SIGINT, intHandler);
  int sectors_count = atoi(argv[1]);
  if (sectors_count > MAX_SECTORS) {
    printf("Incorrect sectors count! max: %d\n", MAX_SECTORS);
    return 1;
  }
  int bee_count = atoi(argv[2]);

  key_t key = ftok(argv[0], 0);
  shared_memory *shmptr;
  int semid;
  int shmid;

  int* children = (int*)malloc(sizeof(int) * bee_count);

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
  for (int i = 0; i < sectors_count; i++) {
    shmptr->sectors[i] = 0;
  }

  printf("Pooh is hiding in sector #%d\n", shmptr->pooh_location);
  // Создаем процессы пчел
  for (int i = 0; i < bee_count; i++) {
    children[i] = fork();
    if (children[i] == 0) {
      child(semid, shmptr);
    }
  }

  printf("Hive ready\n");
  
  while (keepRunning) {
    sleep(1);
  }

  // Завершаем процессы детей если они еще активны
  for (int i = 0; i < bee_count; i++) {
    kill(children[i], SIGTERM);
    printf("Killing child #%d\n", children[i]);
  }

  // Очищаем память
  free(children);

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