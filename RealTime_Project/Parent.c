/*
 * PARENT FILE
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

#define SLOT_LEN 3
#define N_SLOTS 100001

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


int sh_get_key(int key, int size) {
	int shmid;
	if ( (shmid = shmget((int) key, size, IPC_CREAT | 0600)) == -1 ) {	
		perror("shmid -- parent -- creation");		
		exit(3);
	} else return shmid;
}

char* sh_get_p(int shmid) {
	char* shmptr;
	if ( (shmptr = (char *) shmat(shmid, NULL, 0)) == (char *) -1 ) {
      		perror("shmptr -- parent -- attach");
      		exit(1);
    	}
	return shmptr;
}

int get_sem_id(int key, int size) {
	int semid;
	if ( (semid = semget((int) key, size, IPC_CREAT | 0666)) == -1 ) {
		perror("semget -- parent -- creation");
		exit(4);	
	}
	return semid; 
}

char lines[4][60];
int NUMBER_OF_DOCTORS;
int PATIENT_NEW_FORK;
int DOCTOR_SLEEP_TIME;
int DECEASED_PATIENT_THRESHOLD = 0;

void read_file() {

	FILE *fp = fopen("input.txt", "r");
	if (fp == NULL) {
		perror("error -- reading file");
		exit(1);
	}
	int i = 0;
	while (fgets(lines[i++], 60, fp) != NULL){}

	char *token = strtok(lines[0], " ");
	token = strtok(NULL, " ");
	NUMBER_OF_DOCTORS = atoi(token);

	token = strtok(lines[1], " ");
	token = strtok(NULL, " ");
	PATIENT_NEW_FORK = atoi(token);

	token = strtok(lines[2], " ");
	token = strtok(NULL, " ");
	DOCTOR_SLEEP_TIME = atoi(token);

	token = strtok(lines[3], " ");
	token = strtok(NULL, " ");
	DECEASED_PATIENT_THRESHOLD = atoi(token);
}

int thr = 2, NumOfD, parentid, sumDP, shmid, semid;
char *shmptr;
int pidarr[100001];
void sighup();

int main(int argc, int *argv[]) {

	read_file();

	static struct MEMORY memory[10001]; // define the structer of shared memory
	union semun arg; // define the semephore
	int pid = 6; // num of doctor and random gen should from file 
	char str[5], str2[5], str3[5]; 
	static ushort start_val[1] = {1}; // for sem
	parentid = getpid();

	/*
	 * Create, attach and initialize the memory segment
	*/	
	shmid = sh_get_key(pid, sizeof(memory)); // creation the shm memory
	shmptr = sh_get_p(shmid);
 	memcpy(shmptr, (char *) &memory, sizeof(memory));

	/*
	 * Create and initialize the semaphore
	*/
	semid = get_sem_id(pid, 1);
	arg.array = start_val;
	if ( semctl(semid, 0, SETALL, arg) == -1 ) {
		perror("semctl -- parent -- initialization");
		exit(3);
	}

	/*
	 * Create Doctors
	*/
	for (int i = 0; i < NUMBER_OF_DOCTORS; i++) {
		sleep(1);
		if ((pid = fork()) == -1) {
			perror("forking -- error");
			exit(1);
		} else if (pid == 0) {
			sprintf(str, "%d", (i+1));
			sprintf(str2, "%d", NUMBER_OF_DOCTORS);
			sprintf(str3, "%d", DOCTOR_SLEEP_TIME);
			execl("./Doctor", "./Doctor", str, str2, str3, NULL);	
			exit(2);
		}
		pidarr[i] = pid;
	}
	sleep(5);

	/*
	 * Create Patient
	*/
	int Nump = 0;
	for (int i = 1; i < 11; i++) {
		
			Nump++;
			if ((pid = fork()) == -1) {
				perror("forking -- error");
				exit(3);			
			}else if (pid == 0) {
				sprintf(str, "%d", (Nump + NUMBER_OF_DOCTORS));
				char idp[10];
				sprintf(idp, "%d", parentid);
				execl("./Patient", "./Patient",str,idp, (char *) 0);	
				exit(4);
			}	
			pidarr[Nump + NUMBER_OF_DOCTORS-1] = pid;
			sleep(PATIENT_NEW_FORK);	
	}

	sumDP = Nump + NUMBER_OF_DOCTORS;
	while (1) {
		if ( signal(SIGINT, sighup) == SIG_ERR ) {
			perror("singal -- Patient");
			exit(2);
		}
	}


	return 0;
}

void sighup() {
	printf("NEW PATIENT WAS KILLED\n");
	NumOfD++;
	if (NumOfD == DECEASED_PATIENT_THRESHOLD) {
		for (int i = 0; i < sumDP; i++) kill(pidarr[i], SIGKILL);
		shmdt(shmptr);
		shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0);
		semctl(semid, 0, IPC_RMID, 0);
		exit(1);
	}
}

