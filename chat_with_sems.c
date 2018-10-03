//Chat between two users of the same computer with use of shared memory and semaphores

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/sem.h>
#include <semaphore.h>

#define KEY 1234
#define ONLINE 1
#define OFFLINE 0
#define FULL 1
#define EMPTY 0
#define NAME_SIZE 20
#define MAX_SIZE 100

struct chat {
	char msg[MAX_SIZE];
	int status;
	pid_t pid;
};

int main (int argc, char *argv[]) {
	int shmid, fd, n, k, semid, i/*this user*/, j/*the other user*/
	key_t key;
	int a, b, c, d, p1, p2;
	char name1[NAME_SIZE], name2[NAME_SIZE], names[2 * NAME_SIZE + 2];
	struct chat *user;
	pid_t reading;
	struct sembuf sembuf[4];
	
	key = ftok("./", KEY);
	if (key == -1) {
		perror("ftok");
		return 1;
	}
	
	shmid = shmget(key, 2 * sizeof(struct chat), IPC_CREAT|S_IRWXU);
	if (shmid == -1) {
		perror("shmget");
		return 1;
	}
	
	user = (struct chat *) shmat (shmid, NULL, 0);
	if (user == (struct chat *) -1) {
		perror ("shmat");
		return 1;
	}
	
	semid = semget (key, 4, IPC_CREAT|S_IRWXU);
	if (semid == -1) {
		perror("semget");
		return 1;
	}
	//semaphores initialization
	for (k = 0 ; k < 4 ; k++) {
		sembuf[k].sem_num = k;
		sembuf[k].sem_flg = SEM_UNDO;
		if (k % 2 == 0) {
			if(semctl (semid, k, SETVAL, 1) == -1) {
				perror ("semctl");
			}
		}
		else {
			if(semctl (semid, k, SETVAL, 0) == -1) {
				perror ("semctl");
			}
		}
	}
	
	fd = open("info", O_CREAT|O_RDWR, S_IRWXU);
	if (fd == -1) {
		perror("open");
		return 1;
	}
	
	n = read(fd, names, 2 * NAME_SIZE + 2);
	if (n == -1) {
		perror ("read1");
		if (close(fd) == -1) {
			perror("close");
		}
		return 1;
	}
	else {
		if (n == 0) {
			for (k=0 ; k<2 ; k++) {
				user[k].status = OFFLINE;
			}
			
			i = 0;
			j = 1;
			
			strcpy(name1, argv[1]);
			strcpy(name2, argv[3]);
			strcpy(names,name1);
			strcat(names, " ");
			strcat(names, name2);
			
			if (write(fd, names, strlen(names)) == -1) {
				perror ("write");
				if (close(fd) == -1) {
					perror("close");
				}
				return 1;
			}
		}
		else {
			if (user[0].status == ONLINE) {
				sscanf(names, " %s %s", name2, name1);
				
				i=1;
				j=0;
			}
			else {
				if (user[1].status == ONLINE) {
					sscanf(names, " %s %s", name1, name2);
					
					i=0;
					j=1;
				}
				else {
					if (close(fd) == -1) {
						perror("close");
					}
					return 1;
				}
			}
			
			if ((p1 = strcmp(name1, argv[1])) != 0 || (p2 = strcmp(name2, argv[3])) != 0) {
				printf("Other user not found\np1 = %d, p2 = %d\n", p1, p2);
				if (kill(user[j].pid, SIGTERM) == -1) {
					perror ("kill");
				}
				if (close(fd) == -1) {
					perror("close");
				}
				return 1;
			}
		}
	}
	
	if (i == 0) {
		a = 0;
		b = 1;
		c = 2;
		d = 3;
	}
	else {
		a = 2;
		b = 3;
		c = 0;
		d = 1;
	}
	
	user[i].pid = getpid();
	user[i].status = ONLINE;
	
	reading = fork();
	if (reading == -1) {
		user[i].status = OFFLINE;
		perror ("fork");
		if (close(fd) == -1) {
			perror("close");
		}
		return 1;
	}
	else {
		if (reading == 0) { //reading process
			while (1) {
				//block this process
				sembuf[d].sem_op = -1;
				if (semop (semid, &sembuf[d], 1) == -1) {
					user[i].status = OFFLINE;
					perror("semop 3");
					if (kill(user[i].pid, SIGTERM) == -1) {
						perror("kill");
					}
					return 1;
				}
				
				printf("%s\n", user[j].msg);
				
				//unblock other user's writing process
				sembuf[c].sem_op = 1;
				if (semop (semid, &sembuf[c], 1) == -1) {
					user[i].status = OFFLINE;
					perror("semop 4");
					if (kill(user[i].pid, SIGTERM) == -1) {
						perror("kill");
					}
					return 1;
				}
			}
		}
		else { //writing process
			while (1) {
				n = read(STDIN_FILENO, user[i].msg, MAX_SIZE);
				if (n == -1) {
					user[i].status = OFFLINE;
					perror("read");
					if (kill(reading, SIGTERM) == -1) {
						perror("kill");
					}
					return 1;
				}
				
				user[i].msg[n - 1] = '\0';
				
				//unblock other user's writing process
				sembuf[b].sem_op = 1;
				if (semop (semid, &sembuf[b], 1) == -1) {
					user[i].status = OFFLINE;
					perror("semop 1");
					if (kill(reading, SIGTERM) == -1) {
						perror("kill");
					}
					return 1;
				}
				
				if (strcmp (user[i].msg, "quit") == 0) {
					user[i].status = OFFLINE;
					if (kill(reading, SIGTERM) == -1) {
						perror ("kill");
					}
					break;
					//the writing process kills reading process and ends the loop when 'quit' is written
				}
				
				//block this process
				sembuf[a].sem_op = -1;
				if (semop (semid, &sembuf[a], 1) == -1) {
					user[i].status = OFFLINE;
					perror("semop 2");
					if (kill(reading, SIGTERM) == -1) {
						perror("kill");
					}
					return 1;
				}
			}
		}
	}
	
	if (user[j].status == OFFLINE) {
		if (semctl(semid, 0, IPC_RMID) == -1) {
			perror("semctl");
			return 1;
		}
		if (shmctl(shmid, 0, IPC_RMID) == -1) {
			perror("shmctl");
			return 1;
		}
		if (unlink("info") == -1) {
			perror("unlink");
			return 1;
		}
	}
	
	return 0;
}