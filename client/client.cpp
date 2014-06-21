//
//  Client.cpp
//  Client for title-case
//
//  Created by Zhang Honghao on 6/17/14.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <iostream>
#include <fcntl.h>

struct timeval should_fire_time = timeval();
pthread_mutex_t mutex;

static int sock;

void* send_request(void *t) {
    char *message = (char *)t;
    // Schedule task with 2 seconds' delay
    // Get current time
    struct timeval now, result;
    gettimeofday(&now, NULL);
    timersub(&now, &should_fire_time, &result);
    
    // If the current time is after should_fire_time, fire imediately
    if (result.tv_sec > 0 || (result.tv_sec == 0 && result.tv_usec > 0)) {
        // Set the should_fire_time to be next 2 seconds
        pthread_mutex_lock(&mutex);
        should_fire_time.tv_sec = now.tv_sec + 2;
        should_fire_time.tv_usec = now.tv_usec;
        pthread_mutex_unlock(&mutex);
    }
    // Current time is before should_fire_time, sleep offset time and then fire
    else {
        timersub(&should_fire_time, &now, &result);
        pthread_mutex_lock(&mutex);
        should_fire_time.tv_sec += 2;
        pthread_mutex_unlock(&mutex);
        usleep((int)result.tv_sec * 1000 * 1000 + result.tv_usec);
    }
    
    // Prepare for the string length
    uint32_t string_length = (uint32_t)(strlen(message) + 1);
    uint32_t network_byte_order = htonl(string_length);
    
//    printf("%u:%s\n", string_length, message);
    
    long response_result;
    // Send string length
    response_result = send(sock, &network_byte_order, sizeof(uint32_t), 0);
    if (response_result < 0) {
        perror("Send length failed");
    }
//    else {printf("%lu:", response_result);}
    // Send string
    response_result = send(sock, message, string_length, 0);
    if (response_result< 0) {
        perror("Send string failed");
    }
//    else {printf("%ld\n", response_result);}
    
    ssize_t receive_size;
    // Receive string length
    receive_size = recv(sock, &network_byte_order, sizeof(uint32_t), 0);
    // Receive size must be same as sizeof(uint32_t) = 4
    if (receive_size != sizeof(uint32_t)) {
        perror("string length error");
    }
    
    char *response= (char *)malloc(sizeof(char) * string_length);
    receive_size = recv(sock, response, string_length, 0);
    if (receive_size == string_length) {
        printf("Server: %s\n", response);
    } else {
        perror("string length error\n");
    }
    free(response);
    free(message);
    pthread_exit((void*) 0);
}

int main(int argc , char *argv[])
{
//    printf("SERVER_ADDRESS: %s\n", getenv("SERVER_ADDRESS"));
//    printf("SERVER_PORT: %s\n", getenv("SERVER_PORT"));
    
    struct sockaddr_in server;
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create socket\n");
    }
//    setnonblocking(sock);
//    printf("Socket created\n");
    
    // Set address
    server.sin_addr.s_addr = inet_addr(getenv("SERVER_ADDRESS"));
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(getenv("SERVER_PORT")));
    
//    server.sin_addr.s_addr = inet_addr("127.0.0.1");
//    server.sin_family = AF_INET;
//    server.sin_port = htons( 15000 );
    
    // Connect to remote server
    if (connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1) {
        perror("connect failed. Error");
        exit(-1);
    }
//    printf("Connected\n");
    
    pthread_mutex_init(&mutex, NULL);
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
    
    // Keep communicating with server
    std::string message;
    while(getline (std::cin, message)) {
        char *message_cstr = new char[message.length() + 1]; // free this in the thread
        strcpy(message_cstr, message.c_str());

        pthread_t new_send_thread;
        // Schedule a schedule_task
        int create_thread_failed = pthread_create(&new_send_thread, &thread_attr, send_request, (void *)message_cstr);
        if (create_thread_failed) {
            printf("Schedule task failed: %d", create_thread_failed);
            exit(-1);
        }
        pthread_join(new_send_thread, NULL);
    }
    
    pthread_mutex_destroy(&mutex);
    close(sock);
    return 0;
}

