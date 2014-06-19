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
#include <vector>

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
    // Send string length
    send(sock, &network_byte_order, sizeof(uint32_t), 0);
    // Send string
    send(sock, message, string_length, 0);
    
    ssize_t receive_size;
    // Receive string length
    receive_size = recv(sock, &network_byte_order, sizeof(uint32_t), 0);
    // Receive size must be same as sizeof(uint32_t) = 4
    if (receive_size != sizeof(uint32_t)) {
        printf("string length error\n");
    }
    
    char *server_reply = (char *)malloc(sizeof(char) * string_length);
    receive_size = recv(sock, server_reply, string_length, 0);
    if (receive_size == string_length) {
        printf("Server: %s\n", server_reply);
    } else {
        printf("string length error\n");
    }
    free(server_reply);
    free(message);
    pthread_exit((void*) 0);
}

int main(int argc , char *argv[])
{
    struct sockaddr_in server;
    std::vector<pthread_t> thread_vc;
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Could not create socket\n");
    }
    printf("Socket created\n");
    
    // Set address
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    
    // Connect to remote server
    if (connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1) {
        perror("connect failed. Error");
        exit(-1);
    }
    printf("Connected\n");
    
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

