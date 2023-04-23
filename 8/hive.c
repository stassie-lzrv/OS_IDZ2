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
  char memn[] = "shared-memory"; //  имя объекта
  char sem_name[] = "sem-mutex";
  int mem_size = sizeof(shared_memory);
  int shm;
  sem_t* mutex;

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
  if ((mutex = sem_open(sem_name, O_CREAT, 0666, 1)) < 0) {
    printf("Error creating semaphore!\n");
      return 1;
  }
  shmptr->mutex = *mutex;
  shmptr->sectors_count = sectors_count;
  shmptr->pooh_location = rand() % sectors_count;
  shmptr->found = 0;
  for (int i = 0; i < sectors_count; i++) {
    shmptr->sectors[i] = 0;
  }

  printf("Pooh is hiding in sector #%d\n", shmptr->pooh_location);
  
  while (keepRunning) {
    sleep(1);
  }

  if (sem_unlink(sem_name) == -1) {
    perror ("sem_unlink"); exit (1);
  }
  close(shm);
  // удалить выделенную память
  if(shm_unlink(memn) == -1) {
    printf("Shared memory is absent\n");
    perror("shm_unlink");
    return 1;
  }
  return 0;
}