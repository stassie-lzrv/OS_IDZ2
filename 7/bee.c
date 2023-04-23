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
  signal(SIGINT, intHandler);
  key_t key = ftok("hive.c", 0);
  shared_memory *shmptr;
  int shmid;

  if ((shmid = shmget(key, sizeof(shared_memory), 0666 | IPC_CREAT)) < 0) {
    printf("Can\'t create shmem\n");
    return -1;
  }
  if ((shmptr = (shared_memory*) shmat(shmid, NULL, 0)) == (shared_memory *) -1) {
    printf("Cant shmat!\n");
    return -1;
  }
  
  printf("I am a bee #%d!\n", getpid());
  sleep(1);
  int selected_sector = -1;
  while (!shmptr->found && keepRunning) {
    sem_close(shmptr->semid);
    for (int i = 0; i < shmptr->sectors_count; i++) {
      if (shmptr->sectors[i] == 0) {
        selected_sector = i;
        shmptr->sectors[i] = 1;
        break;
      }
    }
    sem_open(shmptr->semid);
    if (selected_sector == -1) {
       printf("[%d] No free sectors to check\n", getpid());
       break;
    }
    printf("[%d] Checking sector #%d\n", getpid(), selected_sector);
    sleep(2 + rand() % 5);
    sem_close(shmptr->semid);
    if (shmptr->pooh_location == selected_sector) {
      shmptr->sectors[selected_sector] = 2;
      printf("[%d] Found the Pooh in sector #%d\n", getpid(), selected_sector);
      shmptr->found = 1;
      sem_open(shmptr->semid);
      break;
    } else {
      printf("[%d] Did not found the Pooh in sector #%d\n", getpid(), selected_sector);
    }
    sem_open(shmptr->semid);
    selected_sector = -1;
    sleep(1 + rand() % 5);
  }  
  return 0;
}