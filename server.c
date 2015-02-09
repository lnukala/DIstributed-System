/*This code is written to be used as a server that carried out the rpcs using
 *the parameters sent by the clinet and returns the retur values to the client
 *The code is capable of handling read, write, close, open, xstat, unlink, 
 *getdirtree, getdirentries and lseek syscalls*/
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include "../include/dirtree.h"

#define MAXMSGLEN 100

/*Function declarations*/
void unlink_call(int sessfd);
void lseek_call(int sessfd);
void read_call(int sessfd);
void gdr_call(int sessfd);
void xstat_call(int sessfd);
void getdirtree_call(int sessfd);
void create_buffer(struct dirtreenode *root);
void write_call(int sessfd);
void open_call(int sessfd);
void close_call(int sessfd);

/*Global buffer used in the getdirentries syscall*/
char *buffer_pathname = NULL;


int main(int argc, char**argv) {
    char buf[MAXMSGLEN+1];
    char *serverport;
    unsigned short port;
    int sockfd, sessfd, rv;
    struct sockaddr_in srv, cli;
    socklen_t sa_size;
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) port = (unsigned short)atoi(serverport);
	else port=15440;
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);					// in case of error
	
	// setup address structure to indicate server port
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;				// IP family
	srv.sin_addr.s_addr = htonl(INADDR_ANY);// don't care IP address
	srv.sin_port = htons(port);				// server port

	// bind to our port
	rv = bind(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);
	
	// start listening for connections
	rv = listen(sockfd, 5);
	if (rv<0) err(1,0);
	
	// main server loop, handle clients one at a time, quit after 10 clients
	while(1) {	
		sa_size = sizeof(struct sockaddr_in);
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
		if(!(rv = fork())){
			if (sessfd<0) err(1,0);
			    // get messages and send replies to this client, until it goes away
			while ( (rv=recv(sessfd, buf, MAXMSGLEN, 0)) > 0) {
				buf[rv]=0;			// null terminate string to print	
				fflush(stdout);	
				 		
				if(!strcmp(buf, "open")){
                	open_call(sessfd);
		   	 	}
				else if(!strcmp(buf,"close")){
					close_call(sessfd);			
				}
				else if(!strcmp(buf, "write")){
					write_call(sessfd);
            	}
				else if(!strcmp(buf, "unlink")){
					unlink_call(sessfd);
				}
				else if(!strcmp(buf, "getdirentries")){
					gdr_call(sessfd);
				}
				else if(!strcmp(buf, "lseek")){
					lseek_call(sessfd);
				}
				else if(!strcmp(buf, "read")){
					read_call(sessfd);
				}
				else if(!strcmp(buf, "__xstat")){
					xstat_call(sessfd);
				}
				else if(!strcmp(buf, "getdirtree")){
					getdirtree_call(sessfd);
				}
			}
		// either client closed connection, or error
		if (rv<0) err(1,0);
		close(sessfd);
		exit(0);
		}
	}
	//Printing action	
	printf("server shutting down cleanly\n");
	// close socket
	close(sockfd);
	return 0;
}

void open_call(int sessfd){
	/*send an ack for open*/
	send(sessfd,"open" , strlen("open") + 1, 0);
	/*Receive the function parameters in a buffer*/
    char buffer[100];
    int number = 0;
	number = recv(sessfd,buffer, 100,0);
	buffer[number] = 0;
	int i = 0, j = 0;
	char *temp_buffer = NULL;
	char* pathname;
	int flags;
	mode_t m;
	int var_id = 1;
	while(buffer[i] != '\0'){
		if(buffer[i] == ';'){
			if(temp_buffer)
				temp_buffer[j] = 0;
			if(var_id == 1){
				if(temp_buffer){
					pathname = temp_buffer;
				}
			}
			else if(var_id == 2){
				if(temp_buffer){
					flags = atoi(temp_buffer);
				}
	      		else flags = 0;
			}
			else if(var_id == 3){
				if(temp_buffer){
					m = atoi(temp_buffer);
				}
				else m = 0;
			}	
     		var_id++;
			temp_buffer = NULL;
			j = 0;
			i++;
			continue;
		}						
			temp_buffer = realloc(temp_buffer, (i+1)*sizeof(char));
			temp_buffer[j] = buffer[i];
			i++;	
			j++;
	}
	free(temp_buffer);	
	int fd = open(pathname, flags, m);
    /*Check if the call to open resulted in an error*/
    int error_number;
 	if(fd < 0){
		error_number = errno;
	}			
	/*send the file descriptor from the server to the client*/
	char mesg[100];
	sprintf(mesg, "%d", fd + 1000);
	send(sessfd,mesg , strlen(mesg) + 1, 0);	
	if(fd < 0){
		char temp_buffer[100];
		recv(sessfd,temp_buffer, 100, 0); 
		char errno_message[10];
		sprintf(errno_message, "%d", error_number);
		send(sessfd, errno_message, strlen(errno_message) + 1, 0);
	}
}

void close_call(int sessfd){
	/*Acknowledge the receipt of close*/
	send(sessfd,"close" , strlen("close") + 1, 0);
	/*Receive the parameters of the function in a buffer*/
	char buffer[100];
    int number = 0;
    number = recv(sessfd,buffer, 100,0);
    buffer[number] = 0;
	int fd = atoi(buffer);
	fd = fd - 1000;
	int close_return = close(fd);
	int error_number;
	if(close_return < 0){
		error_number = errno;
	}
	/*Send the return value*/
	char mesg[100];
	sprintf(mesg, "%d", close_return);
    send(sessfd,mesg , strlen(mesg) + 1, 0);
	if(close_return < 0){
		char temp_buffer[100];
		recv(sessfd,temp_buffer, 100, 0);
        char errno_message[10];
        sprintf(errno_message, "%d", error_number);
        send(sessfd, errno_message, strlen(errno_message) + 1, 0);
    }
}

void write_call(int sessfd){
/*Acknowledge the receipt of close*/
    send(sessfd,"write" , strlen("write") + 1, 0);
	/*Receive the fd and count of the function in a buffer*/
	char buffer1[101];
	int count1 = recv(sessfd, buffer1, 100,0);
	buffer1[count1] = 0;
	int i = 0;
	int j = 0;
	int fd;
	size_t count;
	int var_id = 1;
	char *temp_buffer = NULL;
    while(buffer1[i] != '\0'){
       	if(buffer1[i] == ';'){
            if(var_id == 1){
                fd = atoi(temp_buffer);
                fd = fd - 1000;
            }
			if(var_id == 2){
                count = atoi(temp_buffer);
            }
            var_id++;
            temp_buffer = NULL;
            j = 0;
            i++;
            continue;
        }
        temp_buffer = realloc(temp_buffer, (i+1)*sizeof(char));
        temp_buffer[j] = buffer1[i];
        i++;
        j++;
    }
	free(temp_buffer);
	/*send an acknowledgement*/
	if(fd == sessfd){
		send(sessfd, "sessionfd", strlen("sessionfd")+1,0);
	}
	else {
		send(sessfd, "confirm", strlen("confirm")+1,0);
		/*receive the buffer*/
		char buff[1025];
		char send_buffer[101];
		char temporary_buffer[100];
		char errno_message[10];	
		int write_count = 0;			
		int count_dummy = count;
		int write_return;
		while(count_dummy > 0){
			int rv = recv(sessfd,buff, 1024, 0);
			buff[rv] = 0;
			write_return = write(fd, buff, rv);
			if(write_return < 0){
				write_count += write_return;
                int error_number = errno;
		       	sprintf(send_buffer, "%d", write_return);
              	send(sessfd, send_buffer, strlen(send_buffer)+1, 0);
				recv(sessfd,temporary_buffer, 100, 0);
                sprintf(errno_message, "%d", error_number);
               	send(sessfd, errno_message, strlen(errno_message) + 1, 0);
				break;
			}
			count_dummy -= rv;
			write_count += write_return;
		}

		if(write_return >= 0){
		sprintf(send_buffer, "%d", write_count);
		send(sessfd, send_buffer, strlen(send_buffer)+1, 0);
		}
	}
}

void unlink_call(int sessfd){
	/*acknowledge the receipt of the command*/
	send(sessfd, "unlink", strlen("unlink") + 1, 0);

	/*Receive the parameter pathname*/
	char pathname[10000];
	recv(sessfd, pathname, 10000, 0);

	/*call the function and return the return values*/
	int unlink_return;
	unlink_return = unlink(pathname);
	char transfer_buffer[100];
	sprintf(transfer_buffer, "%d", unlink_return);
	send(sessfd, transfer_buffer, strlen(transfer_buffer)+1, 0);
	if(unlink_return < 0){
		recv(sessfd, transfer_buffer, 100, 0);
		char errno_stirng[11];
		sprintf(errno_stirng, "%d", errno);
		printf("error number sent = %d\n",errno);
		send(sessfd, errno_stirng, strlen(errno_stirng) + 1, 0);
	}
}

void xstat_call(int sessfd){
	/*acknowledge the receipt of the command*/
	send(sessfd, "xstat", strlen("xstat")+1, 0);

	/*Receive the parameters*/
	char parameters[10000];
	int k = recv(sessfd, parameters, 9999,0);
	parameters[k] = 0;

	/*Segregate the function parameters*/
	int ver;
	char *path;
	struct stat* stat_buf = malloc(sizeof(struct stat));
    int i = 0, j = 0;
    int var_id = 0;
    char *temp_buffer = NULL;
    while(parameters[i] != 0 || var_id < 2){
        if(parameters[i] == ';'){
            if(var_id == 0){
                ver = atoi(temp_buffer);
            }
            else if(var_id == 1){
                path = malloc(sizeof(temp_buffer));
                memcpy(path, temp_buffer, j); 
            }
			var_id ++;
            temp_buffer = NULL;
            i++;
            j = 0;
            continue;
        }
        temp_buffer = realloc(temp_buffer, (j+1)*sizeof(char));
        temp_buffer[j] = parameters[i];
        i++;
        j++;
    }
    send(sessfd, "ack", strlen("ack") + 1, 0);
    char *stat_buff = malloc(sizeof(struct stat));
	recv(sessfd, stat_buff, sizeof(struct stat), 0);
	memcpy(stat_buf, stat_buff, sizeof(struct stat));

	send(sessfd, "ack", strlen("ack")+1, 0);

	/*call the function*/
	ssize_t xstat_return = __xstat(ver, path, stat_buf);
	char *xstat_buffer = malloc(100);
	sprintf(xstat_buffer, "%lu", xstat_return);
	send(sessfd, xstat_buffer, strlen(xstat_buffer)+1,0);

	/*In case of an error send the errno*/
	char send_buf[100];
	if(xstat_return < 0){
        recv(sessfd, send_buf, 100, 0);
        char errno_stirng[11];
        sprintf(errno_stirng, "%d", errno);
        printf("error number sent = %d\n",errno);
        send(sessfd, errno_stirng, strlen(errno_stirng), 0);
    }

	/*send the stat buf*/
	memset(send_buf, 0 , sizeof(send_buf));
	int l = recv(sessfd, send_buf, strlen("ack") + 1, 0);
	send_buf[l] = 0;
	char *struct_buf = malloc(sizeof(stat_buf)+1);
	memcpy(struct_buf, stat_buf, sizeof(struct stat));
	struct_buf[sizeof(stat_buf)] = 0;
	send(sessfd, struct_buf, sizeof(struct stat), 0);
}

void gdr_call(int sessfd){
	/*acknowledge the receipt of the command*/
	send(sessfd, "getdirentries", strlen("getdirentries") + 1, 0);

	/*receive the function parameters*/
	char parameters[1000];
	int k = recv(sessfd, parameters, 999, 0);
	parameters[k] = 0;

	/*segregate the function parameters based on delimiter*/
	int fd;
	off_t *basep;
	size_t nbytes;
	int i = 0, j = 0;
	int var_id = 0;
	char *temp_buffer = NULL;
	while(parameters[i] != 0){
		if(parameters[i] == ';'){
			if(var_id == 0){
				fd = atoi(temp_buffer);
				fd = fd - 1000;
			}
			else if(var_id == 1){
				char *temp1;
				nbytes = strtol(temp_buffer, &temp1, 0);
			}
			else if(var_id == 2){
				basep = malloc(sizeof(atoi(temp_buffer)));
				*basep = atoi(temp_buffer);
			}
			var_id ++;
			temp_buffer = NULL;
			i++;
			j = 0;
			continue;
		}
		temp_buffer = realloc(temp_buffer, (j+1)*sizeof(char));
		temp_buffer[j] = parameters[i];
		i++;
		j++;
	}
	/*Call the function*/
	char *buf;
	buf = malloc((sizeof(char)) * nbytes);
	ssize_t gdr_return;
	gdr_return = getdirentries(fd, buf, nbytes, basep);
	char send_buf[100];
	sprintf(send_buf, "%lu", gdr_return);
	send(sessfd, send_buf, strlen(send_buf)+1,0);
	if(gdr_return < 0){
		recv(sessfd, send_buf, 100, 0);
		char errno_stirng[11];
		sprintf(errno_stirng, "%d", errno);
		printf("error number sent = %d\n",errno);
		send(sessfd, errno_stirng, strlen(errno_stirng) + 1, 0);
	}
	else {
	//	printf("sent buf = %s",buf);
		send(sessfd, buf, nbytes, 0);
	}
}	

void lseek_call(int sessfd){
	/*send an acknowledgement*/
	send(sessfd, "lseek", strlen("lseek")+1, 0);

	/*receive the function parameters*/
	char parameters[200];
	int k = recv(sessfd, parameters, 200, 0);
	parameters[k] = 0;

	/*identify the function parameters*/
	int i = 0, j = 0;
	char* temp_buffer = NULL;
	int fd, whence;
	int offset;
	int var_id = 0;
	while(parameters[i] != 0){
		if(parameters[i] == ';'){
			if(var_id == 0){
				fd = atoi(temp_buffer);
				fd = fd - 1000;
				//printf("fd = %d\n",fd);
			}
			else if(var_id == 1){
				offset = atoi(temp_buffer);
				//printf("offset = %d\n",offset);
			}
			else if(var_id == 2){
				whence = atoi(temp_buffer);
				//printf("whence = %d",whence);
			}
			var_id ++;
			temp_buffer = NULL;
			i++;
			j = 0;
			continue;
		}
		temp_buffer = realloc(temp_buffer, (j+1)*sizeof(char));
		temp_buffer[j] = parameters[i];
		i++;
		j++;
	}

	/*call the function*/
	off_t lseek_return;
	lseek_return = lseek(fd, offset, whence);
	char send_buf[100];
	sprintf(send_buf, "%lu", lseek_return);
	send(sessfd, send_buf, 100, 0);
	if(lseek_return < 0){
		recv(sessfd, send_buf, 100, 0);
		char errno_stirng[11];
		sprintf(errno_stirng, "%d", errno);
		printf("error number sent = %d\n",errno);
		send(sessfd, errno_stirng, strlen(errno_stirng)  + 1, 0);	
	}
}

void read_call(int sessfd){
	/*acknowledge the call*/
	send(sessfd, "read", strlen("read")+1, 0);

	/*receive the parameters for the call*/
	char parameters[200];
	int k = recv(sessfd, parameters, 200, 0);
	parameters[k] = 0;

	/*identify the function parameters*/
	int i = 0, j = 0;
	char* temp_buffer = NULL;
	int fd;
	size_t count;
	int var_id = 0;
	while(parameters[i] != 0){
		if(parameters[i] == ';'){
			if(var_id == 0){
				fd = atoi(temp_buffer);
				fd = fd - 1000;
			}
			else if(var_id == 1){
				char *temp1;
				count = strtol(temp_buffer, &temp1, 0);
			}
			var_id ++;
			temp_buffer = NULL;
			i++;
			j = 0;
			continue;
		}
		temp_buffer = realloc(temp_buffer, (j+1)*sizeof(char));
		temp_buffer[j] = parameters[i];
		i++;
		j++;
	}
	
	/*Make the call and set the return values*/
	char *buf;
	buf = malloc(count*sizeof(char));
	ssize_t read_return = read(fd, buf, count);
	char send_buf[100];
	sprintf(send_buf, "%ld", read_return);
	send(sessfd, send_buf, strlen(send_buf)+1, 0);
	if(read_return < 0){
		recv(sessfd, send_buf, strlen("synchronize_send")+1, 0);
		char errno_stirng[11];
		sprintf(errno_stirng, "%d", errno);
		printf("error number sent = %d\n",errno);
		send(sessfd, errno_stirng, strlen(errno_stirng) + 1, 0);

	}
	else {
		char temp_string[100];
		int recv_count = recv(sessfd, temp_string, strlen("Synchronize_send")+1, 0);
		temp_string[recv_count] = 0;
		send(sessfd, buf, count, 0);
	}	
}

void getdirtree_call(int sessfd){
	/*acknowledge the call*/
	send(sessfd, "getdirtree", strlen("getdirtree")+1, 0);

	/*receive the parameters for the call*/
	char pathname[1000];
	int k = recv(sessfd, pathname, 2000, 0);
	pathname[k] = 0;

	/*call getdirtree*/
	struct dirtreenode* root = getdirtree(pathname);
	buffer_pathname = malloc(sizeof(char));
	buffer_pathname[0] = '\0';
	create_buffer(root);

	/*send the buffer to be created*/
	send(sessfd, buffer_pathname,strlen(buffer_pathname), 0);
}

void create_buffer(struct dirtreenode *root){
	static int size_count = 0; 
	int num_len = (root->num_subdirs == 0 ? 1 : (int)(log10(root->num_subdirs)+1));
	size_count += strlen(root->name) + 3 + num_len;
	char *tempbuf = malloc(strlen(root -> name) + 3 + num_len);
	buffer_pathname = realloc(buffer_pathname,size_count);
	sprintf(tempbuf, "%s\n%d\n",root->name, root->num_subdirs);
	strcat(buffer_pathname,tempbuf);
	if(root -> num_subdirs > 0){
		int i;
		for(i = 0; i < root->num_subdirs; i++){
			create_buffer(root->subdirs[i]);
		}
	}
	return;		
}

