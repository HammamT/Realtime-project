/*
 * DOCOTR FILE
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <wait.h>
#include <signal.h>

struct MEMORY {
	int type;
	int id;
	int ex;
};

union semun {
	int val;
	struct semid_ds *buf;
	ushort *array; 
};

struct sembuf acquire = {0, -1, SEM_UNDO}, 
              release = {0,  1, SEM_UNDO};

typedef struct {
  long msg_to;		/* Placed in the queue for */
  long msg_fm;		/* Placed in the queue by  */
  long age;
  long sevirity;
  char buffer[125];
} QMESSAGE;


char* sh_get_p(int shmid) {
	char* shmptr;
	if ( (shmptr = (char *) shmat(shmid, NULL, 0)) == (char *) -1 ) {
      		perror("shmptr -- Doctor -- attach");
      		exit(1);
    	}
	return shmptr;
}

int main(int argc, char *argv[]) {

	int Dnum = atoi(argv[1]);
	int nmD = atoi(argv[2]);
	int sleep_time = atoi(argv[3]);
  	printf("HI FROM DOCTOR WITH pid = %d, WITH NUMBER = %d\n", getpid(), Dnum);

	/*
	 * shared memory access
	*/

	struct MEMORY *memory, *p; // pointer to the memory
	int shmid; //shared memory id
	if ( (shmid = shmget((int) 6, 0, 0)) == -1 ) {	
		perror("shmid -- Doctor -- creation");		
		exit(3);
	} 
	memory = ((struct MEMORY *) sh_get_p(shmid));

	/*
	 * Access the semaphore set
	*/
	int semid;
	if ( (semid = semget((int) 6, 1, 0)) == -1 ) {
		perror("semget -- Doctor -- access");
		exit(3);
	}

	acquire.sem_num = 0;
	if ( semop(semid, &acquire, 1) == -1 ) {
		perror("semop -- Doctor -- acquire");
		exit(4);
	}

	/* 
	 * create massege queue and write the id on the shared memory
	*/

	int mid;
	if ( (mid = msgget(getpid(),IPC_CREAT | 0660)) == -1 ){
		perror("queue");
		exit(1);
	}
	(memory[Dnum]).id = mid;
	(memory[Dnum]).type = 1;

	release.sem_num = 0;
	if ( semop(semid, &release, 1) == -1 ) {
		perror("semop -- Doctor -- release");
		exit(5);
	}
	
	QMESSAGE msg, msg2;
	while (1) {
		printf("DOCTOR NUMBER = %d\n", Dnum);
		acquire.sem_num = 0;
		if ( semop(semid, &acquire, 1) == -1 ) {
			perror("semop -- Doctor -- acquire");
			exit(4);
		}

		/* 
		 * read from the shared memory
		*/
		int flag = 0, idpa = 0;
		p = &memory[nmD+ 1];
		while (p->type == 2) {
				if (p->ex == 0) {
					printf("FOUND PATEINT, id = %d\n", p->id);
					p->ex = mid;
					idpa = p->id;
					flag = 1;
					break;			
				}	
			p++;
		}
		release.sem_num = 0;
		if ( semop(semid, &release, 1) == -1 ) {
			perror("semop -- Doctor -- release");
			exit(5);
		}
		if (flag) {
			int d = kill(idpa, SIGHUP);
			sleep(5);
			//Have Patient.
			int n = 0;
			if ((n = msgrcv(mid, &msg, 130, 1, 0)) == -1 ) {
				perror("DOCTOR: msgrcv");
				exit(2);
		    }
			
			printf("FROM THE DOCTOR WITH NUMBER = %d THE MSG IS = %s, THE AGE = %ld, SEVIRITY = %ld\n", Dnum, msg.buffer, msg.age, msg.sevirity);
			sleep(20);

			if ((msg.age > 60 && msg.sevirity >= 5) || msg.sevirity >= 10) {
				msg2.msg_to = idpa;
				msg2.msg_fm = 1;
				n = sizeof(long) * 3;
				strcpy(msg2.buffer, "");
				n += strlen(msg2.buffer) * sizeof(char) + 2;
				if (msgsnd(mid, &msg2, n, 0) == -1) {
					perror("Doctor: msgsnd");
				}
			} else {
				msg2.msg_to = idpa;
				msg2.msg_fm = 1;
				n = sizeof(long) * 3;
				strcpy(msg2.buffer, "show-up");
				n += strlen(msg2.buffer) * sizeof(char) + 2;
				//printf("FROM DOCTOR %d: MSG SEND WITH SIZE = %d, %d\n", Dnum, n, mid);
				if ( msgsnd(mid, &msg2, n, 0) == -1 ) {
					perror("Patient: msgsend");
					exit(3);
				}
			}			
			
		} else {
			sleep(sleep_time);
			puts("NO PATEINT FOUND\n");
		}
	}
 	return 0;
}
