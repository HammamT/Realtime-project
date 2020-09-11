/*
 * PATIENT FILE
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
#include <sys/wait.h>

union semun {
	int val;
	struct semid_ds *buf;
	ushort *array; 
};

struct sembuf acquire = {0, -1, SEM_UNDO}, 
              release = {0,  1, SEM_UNDO};

struct MEMORY {
	int type;
	int id;
	int ex;
};

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
      		perror("shmptr -- Patient -- attach");
      		exit(1);
    	}
	return shmptr;
}

 /* 
  * using rand built in function is easier than reading from file
 */
int calc_severity() { 
	int sum = 0;
	srand((unsigned) getpid());
	int fever = rand()% 9;
	int cough = rand()% 9;
	int shortnes = rand()% 9;
	int blood_hyper = rand()% 9;
	int heart = rand()% 9;
	int cancer = rand()% 9;
	int howlogill = (rand()% 10)+ 1;
	if (fever > 4) sum++;
	if (cough > 4) sum++;
	if (shortnes > 4) sum++;
	if (blood_hyper > 4) sum++;
	if (heart > 4) sum+= 2;
	if (cancer > 4) sum+= 3;
	if (howlogill > 3) sum++;
	return sum;
}

void sighup();

int semid, idx, msqid, age, sa, idparent;
struct  MEMORY *memory; // pointer to the memory
QMESSAGE msg, rmsg;

int main(int argc, char *argv[]) {

  	srand((unsigned) getpid());
	age = (rand()%100)+1;
	sa = calc_severity();
	idx = atoi(argv[1]);
	idparent = atoi(argv[2]);
	printf("PATIENTT pid = %d, PATIENTT idx = %d\n", getpid(), idx);
	
	/*
	 * Access to sheard memory
	*/
	
	int shmid;
	if ( (shmid = shmget((int) 6, 0, 0)) == -1 ) {	
		perror("shmid -- Patient -- creation");		
		exit(3);
	} 
	memory = ((struct MEMORY *) sh_get_p(shmid));

	/*
	 * Access the semaphore set
	*/
	
	if ( (semid = semget((int) 6, 1, 0)) == -1 ) {
		perror("semget -- Patient -- access");
		exit(3);
	}

	acquire.sem_num = 0;
	if ( semop(semid, &acquire, 1) == -1 ) {
		perror("semop -- Patient -- acquire");
		exit(4);
	}

	// Writing the pid into shared memory
	(memory[idx]).id= getpid();
	(memory[idx]).type= 2;

	release.sem_num = 0;
	if ( semop(semid, &release, 1) == -1 ) {
		perror("semop -- Patient -- release");
		exit(5);
	}	

	while (1) {
		sleep(2);
		if ( signal(SIGHUP, sighup) == SIG_ERR ) {
			perror("singal -- Patient");
			exit(2);
		}
		sa++;
	}
 	return 0;
}

void sighup() {

	acquire.sem_num = 0;
	if ( semop(semid, &acquire, 1) == -1 ) {
		perror("semop -- Patient -- acquire");
		exit(4);
	}

	// Reading the massge id to send to the Doctor.
	msqid = (memory[idx]).ex;
	
	release.sem_num = 0;
	if ( semop(semid, &release, 1) == -1 ) {
		perror("semop -- Patient -- release");
	}

	// sending msg to queue
	msg.msg_to = 1;
	msg.msg_fm = getpid();
	msg.age = age;
	msg.sevirity = sa;
	int n = sizeof(long)* 3;
	strcpy(msg.buffer, "I am sick");
	n += strlen(msg.buffer)* sizeof(char) + 2;
	printf("MSG SEND WITH SIZE = %d, ON ID = %d\n", n, msqid);
	if ( msgsnd(msqid, &msg, n, 0) == -1 ) {
		perror("Patient: msgsend");
		exit(3);
	}
	sleep(40);
	//n = 0;
	//puts("YES");
	//if (age > 60 && sa > 1) {
	//	kill(idparent, SIGINT);	
	//	exit(1);
	//}
	//if (sa > 1) {
	//	kill(idparent, SIGINT);	
	//	exit(1);	
	//}
	//sleep(10);
	//printf("FROM THE PATIENTT iqid = %d, %d\n", msqid, idparent);
	if (rmsg.buffer[0] == 's') {
		printf("PATIENT NUMBER %d, RECOVERS\n", idx);
		shmdt(memory);
		exit(1);
	} else {
		printf("PATIENT NUMBER %d , DIES\n", idx);
		shmdt(memory);
		kill(idparent, SIGINT);
		exit(0);
	}
}
