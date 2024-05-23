#include "l4-common.h"

#include <stdbool.h>

#define BACKLOG_SIZE 10
#define MAX_CLIENT_COUNT 4
#define MAX_EVENTS 10

#define NAME_OFFSET 0
#define NAME_SIZE 64
#define MESSAGE_OFFSET NAME_SIZE
#define MESSAGE_SIZE 448
// #define BUFF_SIZE (NAME_SIZE + MESSAGE_SIZE)
#define BUFF_SIZE 5
#define CITIES_NUMBER 20

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig)
{
    do_work = 0;
    (void)sig;
}

/* ETAP 2 I RESZTA */
void do_server(int tcp_listen_socket)
{
    /* SETTING UP EPOLL */
    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0)
        ERR("epoll_create1:");
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = tcp_listen_socket;
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, tcp_listen_socket, &event))
    {
        perror("epoll_ctl: listen sock");
        exit(EXIT_FAILURE);
    }

    int nfds; /* number of file descriptors */
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    /* BUFFER FOR CLIENT MESSAGES */
    char client_message[BUFF_SIZE];

    int clients_fds[MAX_CLIENT_COUNT];
    int clients_count = 0;
    bool is_free[MAX_CLIENT_COUNT];
    for (int i = 0; i < MAX_CLIENT_COUNT; i++)
        is_free[i] = true;
    (void)is_free;

    // TODO: IMPLEMENT TABLE WITH OWNERSHIPS AND THE WHOLE LOGIC
    char city_ownership[CITIES_NUMBER];
    /* AT THE BEGGINNING ALL THE CITIES ARE GREAK */
    for (int i = 0; i < CITIES_NUMBER; i++)
        city_ownership[i] = 'g';

    while (do_work)
    {
        if ((nfds = epoll_pwait(epoll_descriptor, events, MAX_EVENTS, -1, &oldmask)) > 0)
        {
            for (int n = 0; n < nfds; n++)
            {
                if (events[n].data.fd == tcp_listen_socket) /* PROBA POLACZENIA DO SERWERA */
                {
                    int client_socket = add_new_client(events[n].data.fd);
                    /* CHECKING IF THERE IS STILL PLACE FOR NEW CLIENT */
                    if (clients_count == 4)
                    {
                        fprintf(stderr, "Cannot connect new client...\n");
                        close(client_socket);
                        continue;
                    }

                    /* LOOKING FOR FREE SPOT IN CLIENTS_SOCKET ARRAY */
                    int free_spot = -1;
                    for (int i = 0; i < MAX_CLIENT_COUNT; i++)
                    {
                        if (is_free[i] == true)
                        {
                            free_spot = i;
                            break;
                        }
                    }

                    /* ADDING CLIENT FD TO FREE SPOT */
                    is_free[free_spot] = false;
                    clients_fds[free_spot] = client_socket;
                    clients_count++;

                    /* ADDIND CLIENT TO EPOLL */
                    event.events = EPOLLIN | EPOLLRDHUP;
                    event.data.fd = client_socket;
                    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_socket, &event) == -1)
                        ERR("epoll_ctl");

                    fprintf(stderr, "New client connected of fd: %d\n", client_socket);
                }
                else /* WIADOMOSC OD KLIENTA */
                {
                    int bytes_received = recv(events[n].data.fd, client_message, BUFF_SIZE, 0);
                    if (bytes_received == 0)
                    {
                        /* LOOKING FOR INDEX IN CLIENTS_FDS ARRAY */
                        int client_index = -1;
                        for (int i = 0; i < MAX_CLIENT_COUNT; i++)
                            if (clients_fds[i] == events[n].data.fd)
                            {
                                client_index = i;
                                break;
                            }
                        is_free[client_index] = true;
                        clients_count--;

                        /* CLOSING CONNECTION AND DELETING CLIENT FROM EPOLL */
                        close(events[n].data.fd);
                        epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, events[n].data.fd, NULL);

                        fprintf(stderr, "Client disconnected\n");
                    }
                    else if (bytes_received == -1)
                    {
                        if (errno == ECONNRESET || errno == EPIPE)
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
                        fprintf(stderr, "Message from client: %s\n", client_message);

                        /* READING NECESSARY DATA FROM MESSAGE */
                        char owner = client_message[0];
                        char number_one = client_message[1];
                        char number_two = client_message[2];
                        char whole_number[3] = {number_one, number_two, '\0'};
                        int int_number = atoi(whole_number);

                        if ((owner != 'g' && owner != 'p') || (int_number < 1 || int_number > 20))
                        {
                            fprintf(stderr, "Invalid data from client, disconnecting...\n");

                            /* DISCONNECTING CLIENT */
                            // LOOKING FOR INDEX IN CLIENTS_FDS ARRAY
                            int client_index = -1;
                            for (int i = 0; i < MAX_CLIENT_COUNT; i++)
                                if (clients_fds[i] == events[n].data.fd)
                                {
                                    client_index = i;
                                    break;
                                }
                            is_free[client_index] = true;
                            clients_count--;

                            /* CLOSING CONNECTION AND DELETING CLIENT FROM EPOLL */
                            close(events[n].data.fd);
                            epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, events[n].data.fd, NULL);

                            fprintf(stderr, "Client disconnected\n");
                            /* END OF DISCONNECTING */
                        }
                        else
                        {
                            int_number--;  // to match indexing
                            if (city_ownership[int_number] != owner)
                            {
                                // CHANGING OWNERSHIP
                                city_ownership[int_number] = owner;
                                // SENDING TO ALL
                                for (int i = 0; i < clients_count; i++)
                                {
                                    if (is_free[i] == false)
                                    {
                                        // printf("Sending %s to %d %ld\n", client_message, i, sizeof(client_message));
                                        if (send(clients_fds[i], client_message, 4, 0) < 0)
                                        {
                                            ERR("send");
                                        }
                                    }
                                }
                                fprintf(stderr, "Sucessfully send message to all clients!\n");
                            }
                        }
                    }
                }
            }
        }
    }
    for (int i = 0; i < clients_count; i++)
        close(clients_fds[i]);
}
/* KONIEC ETAPU 2 I RESZTY */

int main(int argc, char **argv)
{
    char *program_name = argv[0];
    if (argc != 2)
    {
        usage(program_name);
    }

    uint16_t port = atoi(argv[1]);
    if (port == 0)
    {
        usage(argv[0]);
    }

    int tcp_listen_socket;
    int new_flags;
    if (sethandler(SIG_IGN, SIGPIPE))
        ERR("Setting SIGPIPE handler:");
    if (sethandler(sigint_handler, SIGINT))
        ERR("Setting SIGINT handler:");

    tcp_listen_socket = bind_tcp_socket(port, BACKLOG_SIZE);
    new_flags = fcntl(tcp_listen_socket, F_GETFL);
    fcntl(tcp_listen_socket, F_SETFL, new_flags);

    // Listen for clients:
    if (listen(tcp_listen_socket, 1) < 0)
    {
        printf("Error while listening\n");
        return EXIT_FAILURE;
    }
    printf("\nListening for incoming connections...\n");

    /* ETAP 1 */
    /* CONNECTING CLIENT */
    //    int client_sock;
    //    // Accept an incoming connection:
    //    client_sock = add_new_client(tcp_listen_socket);
    //    if (client_sock < 0)
    //    {
    //        ERR("cannot add new client");
    //    }
    //
    //    printf("Client %d connected!\nWaiting for 4 bytes long message...\n", client_sock);
    //
    //    char message_buffer[BUFF_SIZE];
    //    int bytes_received = recv(client_sock, message_buffer, BUFF_SIZE, 0);
    //    if (bytes_received == 0)
    //        fprintf(stderr, "Client disconnected!\n");
    //    else if (bytes_received == -1)
    //        ERR("recv");
    //    else
    //    {
    //        fprintf(stderr, "Received message from client: %s\n", message_buffer);
    //    }
    //
    //    close(client_sock);
    /* END OF ETAP 1 */

    do_server(tcp_listen_socket);

    return EXIT_SUCCESS;
}
