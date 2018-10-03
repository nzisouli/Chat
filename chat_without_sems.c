//Chat between two users at the same computer with use of shared memory

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
	int fullness;
	pid_t pid;
};

int main (int argc, char *argv[]) {
	int shmid, fd, n, k, i/*this user*/, j/*the other user*/, p1, p2;
	key_t key;
	char name1[NAME_SIZE]/*this user*/, name2[NAME_SIZE]/*the other user*/, names[2 * NAME_SIZE + 2];
	struct chat *user;
	pid_t reading;
	
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
				user[k].fullness = EMPTY;
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
	
	user[i].pid = getpid();
	user[i].status = ONLINE;
	
	reading = fork();
	if (reading == -1) {
		perror ("fork");
		if (close(fd) == -1) {
			perror("close");
		}
		return 1;
	}
	else {
		if (reading == 0) { //reading process
			while (1) {
				if (user[j].fullness == FULL) {
					printf("%s\n", user[j].msg);
					user[j].fullness = EMPTY;
				}
				sleep(1);
			}
		}
		else { //writing process
			while (1) {
				if (user[i].fullness == EMPTY) {
					n = read(STDIN_FILENO, user[i].msg, MAX_SIZE);
					if (n != -1) {
						perror("read");
						return 1;
					}
					user[i].msg[n - 1] = '\0'; //n-1 because of '\n'
					user[i].fullness = FULL;
					if (strcmp (user[i].msg, "quit") == 0) {
						user[i].status = OFFLINE;
						if (kill(reading, SIGTERM) == -1) {
							perror ("kill");
						}
						break;
						//the writing process kills the reading process and ends the loop when 'quit' is written
					}
				}
				sleep(1);
			}
		}
	}
	
	user[i].status = OFFLINE;
	
	if (user[j].status == OFFLINE) {
		if (shmctl(shmid, IPC_RMID, NULL) == -1) {
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