#include "l4-common.h"
#include <pthread.h>
#include <stdbool.h>

#define BACKLOG_SIZE 10
#define MAX_CLIENT_COUNT 4
#define MAX_EVENTS 10

#define NAME_OFFSET 0
#define NAME_SIZE 64
#define MESSAGE_OFFSET NAME_SIZE
#define MESSAGE_SIZE 448
#define BUFF_SIZE (NAME_SIZE + MESSAGE_SIZE)

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) { do_work = 0; (void)sig;}

void usage(char *program_name)
{
    fprintf(stderr, "USAGE: %s port key\n", program_name);
    exit(EXIT_FAILURE);
}

/* IDEA - CREATE A THREAD THAT WILL LISTEN FOR UPCOMING CONNECTIONS */

// CREATE A STRUCT TO PASS TO THAT THREAD
typedef struct clients_t
{
    int clients_fd[MAX_CLIENT_COUNT];
    int client_count;
    int tcp_listen_socket;
    char *key;
} clients_t;

void *listen_for_clients(void *args_t)
{
    /* READING THREAD ARGUMENTS */
    clients_t *clients_args = (clients_t *)args_t;
    while(do_work)
    {
        int new_client_fd = add_new_client(clients_args->tcp_listen_socket);
        if(new_client_fd < 0)
        {
            ERR("cannot add new client");
        }
        if(clients_args->client_count >= 4)
        {
            printf("cannot connect new client...\n");
            close(new_client_fd);
            continue;
        }
        /* AUTHORIZATION */
        char client_message[BUFF_SIZE];
        if(recv(new_client_fd, client_message, sizeof(client_message), 0) < 0)
        {
            ERR("recv");
        }
        if(strcmp(clients_args->key, client_message + MESSAGE_OFFSET) == 0)
        {
            if(send(new_client_fd, client_message, sizeof(client_message), 0) < 0)
            {
                ERR("send");
            }
            printf("Successfully authorized client %s!\n", client_message);
        }
        else
        {
            printf("Cannnot authorize client %s!\n", client_message);
            close(new_client_fd);
        }
        /* END OF AUTHORIZATION */
        clients_args->clients_fd[clients_args->client_count] = new_client_fd;
        clients_args->client_count++;
    }
    return NULL;
}

void do_server(int tcp_listen_socket, clients_t *clients)
{
    /* SETTING UP EPOLL */
    int epoll_descriptor;
    if((epoll_descriptor = epoll_create1(0)) < 0)
        ERR("epoll_create1:");
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = tcp_listen_socket;
    if(epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, tcp_listen_socket, &event))
    {
        perror("epoll_ctl: listen sock");
        exit(EXIT_FAILURE);
    }

    int nfds; /* number of file descriptors */
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    char client_message[BUFF_SIZE];

    int clients_fds[MAX_CLIENT_COUNT];
    int clients_count = 0;
    bool is_free[MAX_CLIENT_COUNT];
    for(int i = 0; i < MAX_CLIENT_COUNT; i++)
        is_free[i] = true;
    (void)is_free;

    while(do_work)
    {
        if((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, &oldmask)) > 0)
        {
            for(int n = 0; n < nfds; n++)
            {
                if(events[n].data.fd == tcp_listen_socket) /* PROBA POLACZENIA DO SERWERA */
                {
                    int client_socket = add_new_client(events[n].data.fd);
                    /* CHECKING IF THERE IS STILL PLACE FOR NEW CLIENT */
                    if(clients_count == 4)
                    {
                        fprintf(stderr, "Cannot connect new client...\n");
                        close(client_socket);
                        continue;
                    }
                    /* AUTHORIZATION */
                    if(recv(client_socket, client_message, sizeof(client_message), 0) < 0)
                    {
                        ERR("recv");
                    }
                    if(strcmp(clients->key, client_message + MESSAGE_OFFSET) == 0)
                    {
                        if(send(client_socket, client_message, sizeof(client_message), 0) < 0)
                        {
                            ERR("send");
                        }
                        printf("Successfully authorized client %s!\n", client_message);


                        /* ADDIND CLIENT TO EPOLL */
                        clients_fds[clients_count++] = client_socket;

                        event.events = EPOLLIN | EPOLLRDHUP;
                        event.data.fd = client_socket;
                        if(epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_socket, &event) == -1)
                            ERR("epoll_ctl");
                    }
                    else
                    {
                        printf("Cannnot authorize client %s!\n", client_message);
                        close(client_socket);
                    }
                    /* END OF AUTHORIZATION */
                }
                else /* WIADOMOSC OD KLIENTA */
                {
                    int bytes_received = recv(events[n].data.fd, client_message, BUFF_SIZE, 0);
                    if(bytes_received == 0)
                    {
                        fprintf(stderr, "Client disconnected\n");
                        close(events[n].data.fd);
                        epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, events[n].data.fd, NULL);
                        clients_count--;
                    }
                    else if(bytes_received == -1)
                    {
                        if(errno == ECONNRESET || errno == EPIPE)
                        {
                            fprintf(stderr, "Client connection reset...\n");
                            close(events[n].data.fd);
                            epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, events[n].data.fd, NULL);
                        }
                        else
                            ERR("recv");
                    }
                    else
                    {
                        for(int i = 0; i < clients_count; i++)
                        {
                            if(events[n].data.fd != clients_fds[i])
                            {
                                if(send(clients_fds[i], client_message, sizeof(client_message), 0) < 0)
                                {
                                    ERR("send");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    for(int i = 0; i < clients_count; i++)
        close(clients_fds[i]);
}

int main(int argc, char **argv)
{
    char *program_name = argv[0];
    if (argc != 3) {
        usage(program_name);
    }

    uint16_t port = atoi(argv[1]);
    if (port == 0){
        usage(argv[0]);
    }

    char *key = argv[2];

    int tcp_listen_socket;
    int new_flags;
    if(sethandler(SIG_IGN, SIGPIPE))
        ERR("Setting SIGPIPE handler:");
    if(sethandler(sigint_handler, SIGINT))
        ERR("Setting SIGINT handler:");

    tcp_listen_socket = bind_tcp_socket(port, BACKLOG_SIZE);
    new_flags = fcntl(tcp_listen_socket, F_GETFL);
    fcntl(tcp_listen_socket, F_SETFL, new_flags);

    // Listen for clients:
    if(listen(tcp_listen_socket, 1) < 0)
    {
        printf("Error while listening\n");
        return EXIT_FAILURE;
    }
    printf("\nListening for incoming connections...\n");

    /* ETAP 2 */
    // /* NEEDED CLIENT CREDENTIALS */
    // int client_sock;
    // // Accept an incoming connection:
    // client_sock = add_new_client(tcp_listen_socket);
    // if(client_sock < 0)
    // {
    //     ERR("cannot add new client");
    // }
    // (void)key;
    //
    // printf("Client %d connected!\n", client_sock);
    //
    // /* AUTHORIZATION OF THE CLIENT */
    // char client_message[BUFF_SIZE];
    // if(recv(client_sock, client_message, sizeof(client_message), 0) < 0)
    // {
    //     ERR("recv");
    // }
    // if(strcmp(key, client_message + MESSAGE_OFFSET) == 0)
    // {
    //     if(send(client_sock, client_message, sizeof(client_message), 0) < 0)
    //     {
    //         ERR("send");
    //     }
    //     printf("Successfully authorized client %s!\n", client_message);
    // }
    // else
    //     return EXIT_FAILURE;
    // /* END OF AUTHORIZATION */
    //
    // /* READING 5 MESSAGES FROM CLIENT */
    // for(int i = 0; i < 5; i++)
    // {
    //     if(recv(client_sock, client_message, sizeof(client_message), 0) < 0)
    //     {
    //         ERR("recv");
    //     }
    //     printf("Message from client %s: %s\n", client_message, client_message + MESSAGE_OFFSET);
    // }
    /* END OF READING 5 MESSAGES */
    /* CLOSING CONNECTION */
    //close(client_sock);
    /* END OF ETAP 2 */

    /* ETAP 3 */
    // CREATING THREAD FOR LISTENING FOR NEW CLIENTS
    //pthread_t tid;
    clients_t clients;
    clients.client_count = 0;
    clients.tcp_listen_socket = tcp_listen_socket;
    clients.key = key;
    // if(pthread_create(&tid, NULL, listen_for_clients, &clients) != 0)
    //     ERR("pthread_create");

    do_server(tcp_listen_socket, &clients);

    // if(pthread_join(tid, NULL) != 0)
    //     ERR("pthread_join");
    return EXIT_SUCCESS;
}

/* CITATION MARKS TO COPY: "" */