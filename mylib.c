#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <err.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "../include/dirtree.h"

// The following lines declare function pointers with the same prototype as the original functions  
int (*orig_open)(const char *pathname, int flags, ...); 		//prototype for open
ssize_t (*orig_read)(int fd, void *buf, size_t count); 			//prototype for read
ssize_t (*orig_write)(int fd, void *buf, size_t count); 		//prototype for write
off_t (*orig_lseek)(int fd, off_t offset, int whence); 			//prototype for lseek
int (*orig_stat)(int ver ,const char *path, struct stat *buf); 	//prototype for stat
int (*orig_close)(int fd); 										//prototype for close
ssize_t (*orig_getdirentries)(int fd, char *buf, 
							 size_t nbytes , off_t *basep); 	//prototype for getdirentries
int (*orig_unlink)(const char *pathname); 						//prototype for unlink
void (*orig_freedirtree)( struct dirtreenode* dt); 				//prototype for freedirentries
struct dirtreenode* (*orig_getdirtree)( const char *path ); 	//prototype for dirtreeentries
/*End of function prototypes*/

int sockfd; //The socket file descriptor used to communicate with server

void construct_tree(struct dirtreenode** root); //Function used to construct directory tree
char *buffer = NULL; //buffer for storing directory data

#define MAX_BUF 100
#define rpc_fd_offset 1000

/*
 *Function : open
 *Role: Send "open" and marshalled parameters to the server
 *and receive the return values.
 *Invoked from: Call
 */
int open(const char *pathname, int flags, ...){
    //printf("Open");
	mode_t m=0;
	char str[100] = "open";
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}
	send(sockfd, str, strlen(str) + 1, 0);
	
	/*Receive an ack form the server after identifying the function*/
	char buff[101];
	int k = recv(sockfd, buff, 100, 0);
	buff[k] = '\0';
	
	/*Passing the function parameters into a string separated by an identifier*/
	char *transfer_buffer = NULL; //used for encapsulatng the arguments in a string
	
	/*Passing the pathname into the transfer buffer*/
	int i = 0;
	while(pathname[i] != '\0'){
		transfer_buffer = realloc(transfer_buffer, (i+1)*sizeof(char));
		transfer_buffer[i] = pathname[i];
		i++;
	}
	transfer_buffer = realloc(transfer_buffer, (i+1)*sizeof(char));
	transfer_buffer[i] = ';';
	i++;	
	
	/*Passing the flag variable into the transfer buffer*/
	char *temp_string = malloc(100*sizeof(char));
	sprintf(temp_string, "%d", flags);
	int j = 0;
	while(temp_string[j] != '\0'){
        transfer_buffer = realloc(transfer_buffer, (i+1)*sizeof(char));
        transfer_buffer[i] = temp_string[j];
        i++;
		j++;
    }
	free(temp_string);
    transfer_buffer = realloc(transfer_buffer, (i+1)*sizeof(char));
    transfer_buffer[i] = ';';
    i++;
	
	/*Passing the mode_t into the stirng*/
	if(flags & O_CREAT){
		char *temp_string = malloc(100*sizeof(char));
        	sprintf(temp_string, "%d", m);
		j = 0;
		while(temp_string[j] != '\0'){
            transfer_buffer = realloc(transfer_buffer, (i+1)*sizeof(char));
            transfer_buffer[i] = temp_string[j];
            i++;
			j++;
       	}
        free(temp_string);
        transfer_buffer = realloc(transfer_buffer, (i+1)*sizeof(char));
        transfer_buffer[i] = ';';
        i++;
	}
	else {
		transfer_buffer = realloc(transfer_buffer, (i+1)*sizeof(char));
        transfer_buffer[i] = ';';
        i++;
	}

	/*Transfer the buffer containing the parameters to the server*/
	transfer_buffer = realloc(transfer_buffer, (i+1)*sizeof(char));
	transfer_buffer[i] = '\0';
	send(sockfd, transfer_buffer, strlen(transfer_buffer)+1, 0);

	/*Wait for fd from the server*/
    int fd;
    char buf[101];
    int size = recv(sockfd, buf, 100, 0);
    buf[size] = '\0';
	fd = atoi(buf);
	if(fd < 1000){
		send(sockfd, "Synchronize_send", strlen("Synchronize_send")+1, 0);
		char recv_buffer[11];
		recv(sockfd, recv_buffer, 10, 0);
		int error = atoi(recv_buffer);
		errno = error;
		return fd - rpc_fd_offset;
	}
	return fd;
}

/*
 * Function : Read
 * Role: Send "read" to server and wait for ack when invoked
 * 	 Pass through to original function
 * Invoked from: Call
 */	
ssize_t read(int fd, void *buf, size_t count){
    if(fd < 0){
        errno = 9;
        return -1;
    }
    if(fd > 1000){
        char str[MAX_BUF] = "read";
        send(sockfd, str, strlen(str) + 1, 0);

        /*receive an acknowedgemnt from the server*/
        char ackn_buffer[101];
        int k = recv(sockfd, ackn_buffer, 100, 0);
        ackn_buffer[k] = '\0';

        /*send buffer containing parameters*/
        char *transfer_buffer = NULL;
        transfer_buffer = malloc(100);
        sprintf(transfer_buffer, "%d;%lu;",fd,count);
        send(sockfd, transfer_buffer, strlen(transfer_buffer)+1, 0);

        /*receive the read return*/
        char read_buffer[100];
        int m = recv(sockfd, read_buffer, 99, 0);
        read_buffer[m] = 0;
        char *temp;
        ssize_t read_return = strtol(read_buffer, &temp, 0);

        /*in case of an error receive and set errno*/
        if(read_return < 0){
            send(sockfd, "Synchronize_send", strlen("Synchronize_send")+1, 0);
            char recv_buffer[11];
            int recv_count = recv(sockfd, recv_buffer, 10, 0);
            recv_buffer[recv_count] = 0;
            int error = atoi(recv_buffer);
            errno = error;
        }

        else {
            int count_dummy = count;
            send(sockfd, "ack", strlen("ack")+1, 0);
            while(count_dummy > 0){
            char reader_buffer[1024];
            int rv = recv(sockfd, reader_buffer, 1024, 0);
            memcpy(buf, reader_buffer, rv);
            buf = (char*)buf + rv;
            count_dummy -= rv;
            }
        }
        return read_return;
    }
    else if(fd != sockfd) {
        return orig_read(fd, buf, count);
    }

    else {
        errno = 9;
        return -1;
    }   
}

/*
 * Function : __xstat
 * Role: Send "__xstat" to server and wait for ack when invoked
 *       Pass through to original function
 * Invoked from: Call
 */
int __xstat(int ver,const char *path, struct stat *buf){
    //printf("xstat");
    char str[MAX_BUF] = "__xstat";
    send(sockfd, str, strlen(str) + 1, 0);

    /*receive an ack from the server*/
    char ackn_buffer[101];
    int k = recv(sockfd, ackn_buffer, 100, 0);
    ackn_buffer[k] = '\0';

    /*send buffer containing the function parameters*/
    char *transfer_buffer = NULL;
    transfer_buffer = malloc(strlen(path) + 1 + sizeof(struct stat) + (50)*sizeof(char));
    sprintf(transfer_buffer, "%d;%s;",ver,path);
    char *struct_buffer = malloc(sizeof(struct stat));
    memcpy(struct_buffer, buf, sizeof(struct stat));
    send(sockfd, transfer_buffer, strlen(transfer_buffer) + 1, 0);
    char temp[10];
    int temp_count = recv(sockfd, temp, strlen("ack")+1,0);
    temp[temp_count] = 0;
    send(sockfd, struct_buffer, strlen(struct_buffer)+1, 0);
    free(struct_buffer);

    /*receive an ack*/
    k = recv(sockfd, ackn_buffer, 100, 0);
    ackn_buffer[k] = 0;

    /*receive an return value*/
    char xstat_return[100];
    k = recv(sockfd, xstat_return, 100, 0);
    xstat_return[k] = 0;
    int xstat = atoi(xstat_return);

    /*In case of error, receive and set errno*/
    if(xstat < 0){
        send(sockfd, "ack", strlen("ack")+1, 0);
        char recv_buffer[11];
        int recv_count = recv(sockfd, recv_buffer, 10, 0);
        recv_buffer[recv_count] = 0;
        int error = atoi(recv_buffer);
        errno = error;
    }

    /*receive the struct*/
    send(sockfd, "ack", strlen("ack")+1, 0);
    char* struct_buf = malloc(sizeof(struct stat)+1);
    int lo = recv(sockfd, struct_buf, sizeof(struct stat), 0);
    struct_buf[lo] = 0;
    memcpy(buf, struct_buf, sizeof(struct stat));
    free(struct_buf);

    return xstat;
}

/*
 *  Function : write
 *  Role: Send "write" to server and wait for ack when invoked
 *        Pass through to original function
 *  Invoked from: Call
*/
int write(int fd, void *buf, size_t count){
    if(fd < 0){
        errno = 9;
        return -1;
    }

    if(fd > 1000){
        //printf("fd count = %d\n",fd_count);
        char str[MAX_BUF] = "write";
        send(sockfd, str, strlen(str) + 1, 0);
    
        /*Receive an ack form the server after identifying the function*/
        char ackn_buffer[101];
        int k = recv(sockfd, ackn_buffer, 100, 0);
        ackn_buffer[k] = '\0';
    
        /*send buffer containing the file descriptor and the count*/
        char buffer[100];
        sprintf(buffer, "%d"";""%lu"";", fd, count);
        send(sockfd, buffer, strlen(buffer)+1, 0);
    
        /*receive confirmation*/
        char ack_buffer[101];
        int receive_count = recv(sockfd, ack_buffer, 100, 0);
        ack_buffer[receive_count] = '\0';
        if(!strcmp("sessionfd",ack_buffer)){
            return orig_write(fd, buf, count);
        }

        /*send the buffer*/
        char *send_buffer;
        send_buffer = malloc(count + 1);
        memcpy(send_buffer, buf, count);
        send_buffer[count+1] = 0;
        send(sockfd, send_buffer, count, 0);    

        /*receive the return from the write*/   
        int write_return = 0;
        char buff[101];
        (k = recv(sockfd, buff, 100, 0));
        buff[k] = '\0';
        write_return = atoi(buff);
        if(write_return < 0){
            send(sockfd, "Synchronize_send", strlen("Synchronize_send")+1, 0);
            char recv_buffer[11];
            int recv_count = recv(sockfd, recv_buffer, 10, 0);
            recv_buffer[recv_count] = 0;
            int error = atoi(recv_buffer);
            errno = error;
        }
        return write_return;    
    }

    else if (fd != sockfd){
    //    printf("Not Found fd = %d",fd);
        int n = orig_write(fd, buf, count);
        return n;
    }	

    else {
        errno = 9;
        return -1;
    }   
}

/*
 *  Function : lseek
 *  Role: Send "lseek" to server and wait for ack when invoked
 *        Pass through to original function
 *  Invoked from: Call
 */
off_t lseek (int fd, off_t offset, int whence){
    //printf("lseek");
    if(fd < 0){
        errno = 9;
        return -1;
    }
    if(fd > 1000){
        char str[MAX_BUF] = "lseek";
        send(sockfd, str, strlen(str) + 1, 0);
    
        /*Receive an acknowledgment from the server recognizing the call*/
        char buff[101];
        int k = recv(sockfd, buff, 100, 0);
        buff[k] = '\0';

        /*Pass the parameters to a string*/
        char *transfer_buffer = malloc(200 * sizeof(char));
        sprintf(transfer_buffer, "%d;%d;%d;", fd,(int)offset,whence);
        send(sockfd, transfer_buffer, strlen(transfer_buffer) + 1, 0);

        /*receive the return value*/
        int size = 0;
        char buf[101];
        size = recv(sockfd, buf, 100, 0);
        buf[size] = '\0';
        int lseek_return = atoi(buf);

        /*in case of error, receive and set errno*/
        if(lseek_return < 0){
            send(sockfd, "Synchronize_send", strlen("Synchronize_send")+1, 0);
            char recv_buffer[11];
            int recv_count = recv(sockfd, recv_buffer, 10, 0);
            recv_buffer[recv_count] = 0;
            int error = atoi(recv_buffer);
            errno = error;
        }
        return lseek_return;
    }

    else if(fd != sockfd) {
        return orig_lseek(fd, offset, whence);
    }
	
    else {
        errno = 9;
        return -1;
    }   
}

/*
 *Function : close
 *Role: Send "close" to server and wait for ack when invoked
 *	Pass through to original function
 *Invoked from: Call
*/
int close(int fd){
    // printf("close\n");
    //printf("fd = %d\n",fd);
    if(fd < 0){
        errno = 9;
        return -1;
    }
    if(fd > 1000){
        //printf("fd opened by server\n");
        char str[MAX_BUF] = "close";
        send(sockfd, str, strlen(str) + 1, 0);
    
        /*Receive an acknowledgment from the server recognizing the call*/
        char buff[101];
        int k = recv(sockfd, buff, 100, 0);
        buff[k] = '\0';

        /*Pass the parameter into a string to be sent to the server*/
        char *transfer_buffer = malloc(100*sizeof(char));
        sprintf(transfer_buffer, "%d", fd);

        /*Transfer the buffer containing the parameters to the server*/
        send(sockfd, transfer_buffer, strlen(transfer_buffer)+1, 0);

        /*Wait for an ack from the server*/
        int size = 0;
        char buf[101];
        (size = recv(sockfd, buf, 100, 0));
        buf[size] = '\0';
        int close_return = atoi(buf);
        //printf("close return = %d\n",close_return);
        if(close_return < 0){
            send(sockfd, "Synchronize_send", strlen("Synchronize_send")+1, 0);
            char recv_buffer[11];
            int recv_count = recv(sockfd, recv_buffer, 10, 0);
            recv_buffer[recv_count] = 0;
            int error = atoi(recv_buffer);
            //printf("close error number = %d\n",error);
            errno = error;
        }
        //remove_fd(fd);
        return close_return;
    }
    else if(fd != sockfd) {
        return orig_close(fd);
    }	

    else {
        errno = 9;
        return -1;
    }   
}

/*
 * Function : close
 * Role: Send "close" to server and wait for ack when invoked
 * 	Pass through to original function
 * Invoked from: Call
 */
int unlink(const char *pathname){
    //printf("Unlink");
    char str[MAX_BUF] = "unlink";
    send(sockfd, str, strlen(str) + 1, 0);

    /*receive an acknowledgement from the server*/
    char temp_buffer[100];
    int k = recv(sockfd, temp_buffer, 99, 0);
    temp_buffer[k] = 0;

    /*send the parameters packed*/
    send(sockfd, pathname, strlen(pathname) + 1, 0);

    /*receive the return value*/
    int size = 0;
    char buf[101];
    size = recv(sockfd, buf, 100, 0);
    buf[size] = '\0';
    int unlink_return = atoi(buf);
    if(unlink_return < 0){
        send(sockfd, "Synchronize_send", strlen("Synchronize_send")+1, 0);
        char recv_buffer[11];
        int recv_count = recv(sockfd, recv_buffer, 10, 0);
        recv_buffer[recv_count] = 0;
        int error = atoi(recv_buffer);
        errno = error;
    }   
    return unlink_return;
}

/*
 *  Function : getdirentries
 *  Role: Send "close" to server and wait for ack when invoked
 *  	Pass through to original function
 *  Invoked from: Call
 */
ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep){
    //printf("gdr");
    if(fd < 0){
        errno = 9;
        return -1;
    }
    if(fd > 1000){
        //  printf("in getfirentries");
        char str[MAX_BUF] = "getdirentries";
        send(sockfd, str, strlen(str) + 1, 0);

        /*receive an ack form the server*/
        char temp_buffer[100];
        int k = recv(sockfd,temp_buffer, 99,0);
        temp_buffer[k] = 0;

        /*send the parameters packed in a string*/
        char transfer_buffer[300];
        sprintf(transfer_buffer, "%d;%lu;%d;",fd,nbytes,(int)*basep);
        send(sockfd, transfer_buffer, strlen(transfer_buffer)+1, 0);

        /*receive the return value*/
        int size = 0;
        char buffer[101];
        size = recv(sockfd, buffer, 100, 0);
        buffer[size] = '\0';
        int gdr_return = atoi(buffer);
    
        /*in case of an error receive and set errno*/
        if(gdr_return < 0){
            send(sockfd, "Synchronize_send", strlen("Synchronize_send")+1, 0);
            char recv_buffer[11];
            int recv_count = recv(sockfd, recv_buffer, 10, 0);
            recv_buffer[recv_count] = 0;
            int error = atoi(recv_buffer);
            errno = error;
        }   
        else {
            int count = 0;
            while(count < nbytes){
                int rv = recv(sockfd, buf, nbytes, 0);
                count += rv;
            }
        } 
        return gdr_return;
    }

    else if (fd != sockfd){
        return orig_getdirentries(fd, buf, nbytes, basep);
    }  

    else {
        errno = 9;
        return -1;
    }   
}

/*
 * Function : getdirtree
 * Role: Send getdirtree and marshalled parameters to server 
 *and wait for ack when invoked and receive return values
 * 	Pass through to original function
 * Invoked from: Call
 */

struct dirtreenode* getdirtree( const char *path ){
    /*send synchronization ack*/
    char str[MAX_BUF] = "getdirtree";
    send(sockfd, str, strlen(str) + 1, 0);

    /*receive ack*/
    recv(sockfd, str, strlen("getdirtree")+1, 0);

    /*send the eparameter*/
    send(sockfd, path, strlen(path)+1, 0);

    /*receive the return value*/
    buffer = malloc(10000*sizeof(char));
    int k = recv(sockfd, buffer, 9999, 0);
    buffer[k] = 0;
    //printf("buffer = %s",buffer);

    /*reconstruct the tree*/
    struct dirtreenode* root = NULL;
    construct_tree(&root);
    return root;
}

/*function to create directory tree on client*/
void construct_tree(struct dirtreenode** root){
    char* name = NULL;
    if(*root == NULL) {
	name = strtok(buffer, "\n");
    }
    else {
	name = strtok(NULL, "\n");
    }

    char *num = strtok(NULL, "\n");

    if(*root == NULL){
	    *root = (struct dirtreenode*)malloc(sizeof(struct dirtreenode));
    }
    (*root) -> num_subdirs = atoi(num);
    (*root) -> name = name;
    int i;
    if(num){
        (*root) -> subdirs = malloc((*root)->num_subdirs*sizeof(struct dirtreenode));
        for(i = 0; i < (*root)->num_subdirs; i++){
            (*root) -> subdirs[i] = (struct dirtreenode*)malloc(sizeof(struct dirtreenode));
            construct_tree(&((*root)->subdirs[i]));
        }
    }
    else {
	(*root)->subdirs = NULL;
     }
    return;   
}

/*Function to delete the directory tree on the client*/ 
void free_tree(struct dirtreenode* root){
	int i;
	if(root -> subdirs){
		for(i = 0; i < root->num_subdirs; i++){
			free_tree(root->subdirs[i]);
		}
	}
	else {
		free(root);
	}
}

/*
 * Function : freedirtree
 * Role: free the direcrtory data structure created on the client
 * Invoked from: Call
 */
void freedirtree( struct dirtreenode* dt ){
        free_tree(dt);
}

// This function is automatically called when program is started
void _init(void) {
	char *serverip;
    char *serverport;
    unsigned short port;
    int rv;
    struct sockaddr_in srv;
	
	// Get environment variable indicating the ip address of the server
	serverip = getenv("server15440");
        if (!serverip){
                serverip = "127.0.0.1";
        }
	
	// Get environment variable indicating the port of the server 
	serverport = getenv("serverport15440");
        if (!serverport) {
                serverport = "15440";
	}
	port = (unsigned short)atoi(serverport);

	//Establish a socket connection
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd<0) err(1, 0);
	
	//Set up structure to point to the server communicated with
	memset(&srv, 0, sizeof(srv));
	srv.sin_family = AF_INET;
	srv.sin_addr.s_addr = inet_addr(serverip);
	srv.sin_port = htons(port);
	
	//Connect to the server
	rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
        if (rv<0) err(1,0);
		
	// set function pointer orig_open to point to the original open function
	orig_open = dlsym(RTLD_NEXT, "open");

	// set function pointer orig_read to point to the original read function
	orig_read = dlsym(RTLD_NEXT, "read");

	// set function pointer orig_write to point to the original write function
	orig_write = dlsym(RTLD_NEXT, "write");

	// set function pointer origlseek to point to the original lseek function
	orig_lseek = dlsym(RTLD_NEXT, "lseek");

	// set function pointer orig_stat to point to the original stat function
	orig_stat = dlsym(RTLD_NEXT, "__xstat");

	// set function pointer orig_close to point to the original close function
	orig_close = dlsym(RTLD_NEXT, "close");
	
	// set function pointer orig_close to point to the original unlink function
	orig_unlink = dlsym(RTLD_NEXT, "unlink");
	
	// set function pointer orig_close to point to the original getdirentries function
	orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");

	// set function pointer orig_close to point to the original getdirtree function
    orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");

	// set function pointer orig_close to point to the original freedirtree function
	orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
}

/*fini called after the library is for the last time*/
void _fini(void){
	orig_close(sockfd);
}
