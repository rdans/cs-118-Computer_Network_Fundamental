//
//  main.c
//  Project_1
//
//  Created by Tom Zhang on 1/25/18.
//  Copyright © 2018 CS_118. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h> //For message type convertion
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>//Network address and service translation
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>

void my_sa_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void space_replace(char *dest)
{
    char buffer[1024] = {0};
    char *insert_point = &buffer[0];
    const char *tmp = dest;
    
    while (1) {
        const char *p = strstr(tmp, "%20");
        if (p == NULL) {
            strcpy(insert_point, tmp);
            break;
        }

        memcpy(insert_point, tmp, p - tmp);
        insert_point += p - tmp;
        memcpy(insert_point, " ", 1);
        insert_point += 1;
        tmp = p + 3;
    }
    
    // write altered string back to target
    strcpy(dest, buffer);
}

void response(int sock, char *filename, size_t fileLength)
{
    char message[512];
    
    //status-line
    char *status;
    status = "HTTP/1.1 200 OK\r\n";
    
    //--------------------- General-Header ---------------------
    char *connection = "Connection: close\r\n";
    struct tm* gmt_time;
    time_t t;
    time(&t);
    gmt_time = gmtime(&t);
    char current_time[35];
    strftime(current_time, 35, "%a, %d %b %Y %T %Z", gmt_time);
    char date[45] = "Date: ";
    strcat(date, current_time);
    strcat(date, "\r\n");
    
    //response-header
    char *server = "Server: Tianyang Zhang/1.0\r\n";
    
    //--------------------- Entity-Header ---------------------
    struct tm* lmclock;
    struct stat attrib;
    stat(filename, &attrib);
    lmclock = gmtime(&(attrib.st_mtime));
    char lmtime[35];
    strftime(lmtime, 35, "%a, %d %b %Y %T %Z", lmclock);

    //last-modified
    char lastModified[50] = "Last-Modified: ";
    strcat(lastModified, lmtime);
    strcat(lastModified, "\r\n");
    
    //content-length
    char content_length[50] = "Content-Length: ";
    char len[10];
    sprintf (len, "%d", (unsigned int)fileLength);
    strcat(content_length, len);
    strcat(content_length, "\r\n");
    
    //content-type
    char* content_type = "Content-Type: text/txt\r\n";
    char *tmp = malloc(sizeof(char) * (strlen(filename) + 1));
    strcpy(tmp, filename);
    int i = 0;
    while (tmp[i]) {
        tmp[i] = tolower(tmp[i]);
        i++;
    }

    if (strstr(tmp, ".html") != NULL)
        content_type = "Content-Type: html\r\n";
    else if (strstr(tmp, ".htm") != NULL)
        content_type = "Content-Type: htm\r\n";
    else if (strstr(tmp, ".txt") != NULL)
        content_type = "Content-Type: txt\r\n";
    else if (strstr(tmp, ".jpeg") != NULL)
        content_type = "Content-Type: jpeg\r\n";
    else if (strstr(tmp, ".jpg") != NULL)
        content_type = "Content-Type: jpg\r\n";
    else if (strstr(tmp, ".gif") != NULL)
        content_type = "Content-Type: gif\r\n";
    
    strcat(message, status);
    strcat(message, connection);
    strcat(message, date);
    strcat(message, server);
    strcat(message, lastModified);
    strcat(message, content_length);
    strcat(message, content_type);
    strcat(message, "\r\n\0");
    
    // send response to client browser as header lines
    send(sock, message, strlen(message), 0);
    
    // send copy of response to console
    printf("HTTP Response:\n%s\n", message);
}

int main(int argc, const char * argv[]) {
    int sockfd = 0, newfd = 0; //socket file descriptors
    struct addrinfo hints, *servinfo, *addr_info;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    char dest[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    
    hints.ai_family = AF_UNSPEC; //Support both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    int ret = 0;
    if ((ret = getaddrinfo(NULL, "3169", &hints, &servinfo)) != 0) {
        fprintf(stderr, "%s\n", gai_strerror(ret));
        return 1;
    }
    //Binding
    addr_info = servinfo;
    int opt_val = 1;
    while (1) {
        if (addr_info == NULL)
            break;
        if ((sockfd = socket(addr_info->ai_family, addr_info->ai_socktype,
                             addr_info->ai_protocol)) == -1) {
            fprintf(stderr, "Server socket error\n");
            addr_info = addr_info->ai_next;
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_val,
                       sizeof(int)) == -1) {
            fprintf(stderr, "Sockopt setting error\n");
            exit(1);
        }
        if (bind(sockfd, addr_info->ai_addr, addr_info->ai_addrlen) == -1) {
            close(sockfd);
            fprintf(stderr, "Server binding error\n");
            addr_info = addr_info->ai_next;
            continue;
        }
        break; //Iterated all results, exit loop
    }

    if (listen(sockfd, 5) == -1) {
        fprintf(stderr, "Listen error");
        exit(1);
    }
    
    struct sigaction sig_act;
    sig_act.sa_handler = my_sa_handler; // reap all dead processes
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sig_act, NULL) == -1) {
        printf("Sigaction error");
        exit(1);
    }
    printf("Server listening...\n");
    while(1) { // main accept() loop
        sin_size = sizeof client_addr;
        newfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (newfd == -1) {
            printf("Accept failed");
            continue;
        }
        
        //Convert IPv4 and IPv6 addresses from binary to text form
        struct sockaddr* client_sockaddr = (struct sockaddr *)&client_addr;
        struct in_addr* addr_ipv4 = NULL;
        struct in6_addr* addr_ipv6 = NULL;
        if (client_sockaddr->sa_family == AF_INET) {
            addr_ipv4 = &(((struct sockaddr_in*)client_sockaddr)->sin_addr);
            inet_ntop(client_addr.ss_family, addr_ipv4, dest, sizeof dest);
        }else {
            addr_ipv6 = &(((struct sockaddr_in6*)client_sockaddr)->sin6_addr);
            inet_ntop(client_addr.ss_family, addr_ipv6, dest, sizeof dest);
        }
        
        printf("Connected with %s\n", dest);
        
        //Create child process to handle connected client
        if (!fork()) {
            //Fork() return 0 if this is child process, then close sockfd in child process
            close(sockfd);
            
            //Process request message
            char *filename;
            char buffer[512]; //Read 512 characters every time
            bzero(buffer,512);
            if (read(newfd, buffer, 511) < 0)//Reading message to buffer
                fprintf(stderr, "Socket reading error\n");
            printf("HTTP Request Message:\n%s\n", buffer);
            
            //Tokenize the received message
            const char space[2] = " ";
            filename = strtok(buffer, space);
            filename = strtok(NULL, space);
            //Delete first character '/'
            filename++;
            
            if(strlen(filename)<=0) filename = "\0";
            printf("Request file: %s\n", filename);
            
            
            //----------------- Serve Files ------------------
            char* err404 = "HTTP/1.1 404 Not Found\r\n\r\n";
            char* err404_html = "<h1>Error 404: File Not Found!</h1> <br><br>";
            if(strncmp(filename, "\0", 1) == 0)
            {
                send(newfd, err404, strlen(err404), 0);
                send(newfd, err404_html, strlen(err404_html), 0);
                printf("No file specified\n");
                close(newfd);
                exit(0);
            }
            
            space_replace(filename);
            
            FILE *fd = fopen(filename, "r");
            if (fd==NULL)
            {
                send(newfd, err404, strlen(err404), 0);
                send(newfd, err404_html, strlen(err404_html), 0);
                printf("File not found\n");
                close(newfd);
                exit(0);
            }
            
            char *content = NULL;
            if (fseek(fd, 0L, SEEK_END) == 0)
            {
                long file_size = ftell(fd);
                if (file_size == -1)
                {
                    send(newfd, err404, strlen(err404), 0);
                    send(newfd, err404_html, strlen(err404_html), 0);
                    printf("File size error\n");
                    close(newfd);
                    exit(0);
                }
                
                //allocate content buffer
                content = malloc(sizeof(char) * (file_size + 1));
                
                if (fseek(fd, 0L, SEEK_SET) != 0)
                {
                    send(newfd, err404, strlen(err404), 0);
                    send(newfd, err404_html, strlen(err404_html), 0);
                    printf("File size error\n");
                    close(newfd);
                    exit(0);
                }
                
                //read content to buffer
                size_t content_size = fread(content, sizeof(char), file_size, fd);
                
                //check read process
                if (content_size == 0)
                {
                    send(newfd, err404, strlen(err404), 0);
                    send(newfd, err404_html, strlen(err404_html), 0);
                    printf("File size error\n");
                    close(newfd);
                    exit(0);
                }
                
                //set terminal character
                content[content_size] = '\0';
                response(newfd, filename, content_size);
                send(newfd, content, content_size, 0);
                printf("File served: \"%s\"\n\n", filename);
            }
            
            // close file and free dynamically allocated file source
            fclose(fd);
            free(content);
            
            close(newfd);
            exit(0);
        }
        close(newfd);
    }
    return 0;
}
