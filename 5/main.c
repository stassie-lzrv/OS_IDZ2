#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>


#define MAX_SECTORS 32

static volatile int keepRunning = 1;

// Структура для общей памяти
// sectors: 0 - еще не проверили, 1 - проверяют, 2 - нашли Винни Пуха
typedef struct shared_memory {
  sem_t mutex;
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

// Функция для Стаи пчел
void child(shared_memory* shmptr)  {
  srand(time(NULL) ^ (getpid()<<16));
  printf("I am a bee #%d!\n", getpid());
  sleep(1);
  int selected_sector = -1;
  while (!shmptr->found) {
    sem_wait(&shmptr->mutex);
    for (int i = 0; i < shmptr->sectors_count; i++) {
      if (shmptr->sectors[i] == 0) {
        selected_sector = i;
        shmptr->sectors[i] = 1;
        break;
      }
    }
    sem_post(&shmptr->mutex);
    if (selected_sector == -1) {
       printf("[%d] No free secotrs to check\n", getpid());
       break;
    }
    printf("[%d] Checking sector #%d\n", getpid(), selected_sector);
    sleep(2 + rand() % 5);
    sem_wait(&shmptr->mutex);
    if (shmptr->pooh_location == selected_sector) {
      shmptr->sectors[selected_sector] = 2;
      printf("[%d] Found the Pooh in sector #%d\n", getpid(), selected_sector);
      shmptr->found = 1;
      sem_post(&shmptr->mutex);
      break;
    } else {
      printf("[%d] Did not found the Pooh in sector #%d\n", getpid(), selected_sector);
    }
    sem_post(&shmptr->mutex);
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

  int* children = (int*) malloc(sizeof(int) * bee_count);
  char memn[] = "shared-memory"; //  имя объекта
  int mem_size = sizeof(shared_memory);
  int shm;
  sem_t mutex;

  if ((shm = shm_open(memn, O_CREAT | O_RDWR, 0666)) == -1) {
      printf("Object is already open\n");
      perror("shm_open");
      return 1;
  } else {
      printf("Object is open: name = %s, id = 0x%x\n", memn, shm);
  }
  if (ftruncate(shm, mem_size) == -1) {
      printf("Memory sizing error\n");
      perror("ftruncate");
      return 1;
  } else {
      printf("Memory size set and = %d\n", mem_size);
  }

  //получить доступ к памяти
  void* addr = mmap(0, mem_size, PROT_WRITE, MAP_SHARED, shm, 0);
  if (addr == (int * ) - 1) {
      printf("Error getting pointer to shared memory\n");
      return 1;
  }

  shared_memory* shmptr = addr;
  sem_init(&shmptr->mutex, 1, 1);
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
      child(shmptr);
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
  sem_destroy(&shmptr->mutex);
  close(shm);
  // удалить выделенную память
  if(shm_unlink(memn) == -1) {
    printf("Shared memory is absent\n");
    perror("shm_unlink");
    return 1;
  }
  return 0;
}