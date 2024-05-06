#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include "potato.h"
#include <cerrno>

using namespace std;

class Player
{
public:
    int player_id;
    int num_players;
    // File descriptor for connection to the player on the left
    int asClientfd;
    // File descriptor for accepting the player on the right
    int asServerfd;
    // File descriptor for connection to the ringmaster
    int ringmaster_fd;
    int player_port;

    // Local
    const char *hostname;
    const char *port;

    int leftNeighborPort;
    string leftNeighborIP;
    int rightNeighborPort;
    string rightNeighborIP;

    Potato potato;

    // Constructor
    Player(const char *Hostname, const char *Port) : hostname(Hostname), port(Port)
    {
    }

    // Destructor
    ~Player()
    {
        close(asClientfd);
        close(asServerfd);
        close(ringmaster_fd);
    }

    void communicateWithRingmaster();
    void setupPlayerServer();
    void communicateWithNeighbor();
    int handleRingmasterInput();
    void handleClientInput();
    void handleServerInput();
    void playerPlay();

    void forwardPotato(Potato &potato)
    {
        // srand((unsigned int)time(NULL) + player_id);
        // Randomly choose left(0) or right(1)
        int direction = rand() % 2;

        if (potato.hops_left > 1)
        {
            potato.recordPassTo(player_id);
            cout << "Sending potato to " << (direction == 0 ? ((player_id - 1 + num_players) % num_players) : ((player_id + 1) % num_players)) << endl;
            // int fd = (direction == 0) ? asClientfd : asServerfd;
            int fd = (direction == 0) ? asServerfd : asClientfd;
            send(fd, &potato, sizeof(potato), 0);
        }
        else // Last hop
        {
            cout << "I'm it" << endl;
            potato.recordPassTo(player_id);
            send(ringmaster_fd, &potato, sizeof(potato), 0); // Send back 
        }
    }
};

void Player::communicateWithRingmaster()
{
    int status;
    // int socket_fd;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;

    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(hostname, port, &host_info, &host_info_list);
    if (status != 0)
    {
        cerr << "player: cannot get address info for host" << endl;
        exit(EXIT_FAILURE);
    } // if

    ringmaster_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (ringmaster_fd == -1)
    {
        cerr << "player: cannot create socket" << endl;
        exit(EXIT_FAILURE);
    } // if

    status = connect(ringmaster_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1)
    {
        cerr << "player: cannot connect to socket" << endl;
        exit(EXIT_FAILURE);
    } // if

    freeaddrinfo(host_info_list);

    // first time recv for id
    recv(ringmaster_fd, &player_id, sizeof(player_id), MSG_WAITALL);
    // second time recv for player number
    recv(ringmaster_fd, &num_players, sizeof(num_players), MSG_WAITALL);

    // find the port on which your client program has bound to listen
    struct sockaddr_in addr_in;
    socklen_t sin_size = sizeof(addr_in);
    if (getsockname(asClientfd, (struct sockaddr *)&addr_in, &sin_size) == -1)
    {
        cerr << "player: Cannot find the listening port of client" << endl;
        exit(EXIT_FAILURE);
    }
    // Extract the port from the socket address
    player_port = ntohs(addr_in.sin_port);
    // output and Send back
    cout << "Connected as player " << player_id << " out of " << num_players << " total players" << endl;
    send(ringmaster_fd, &player_port, sizeof(player_port), 0);

    // Receive left neighbor info
    int left_info_length;
    recv(ringmaster_fd, &left_info_length, sizeof(left_info_length), 0);
    char *left_buffer = new char[left_info_length + 1];
    recv(ringmaster_fd, left_buffer, left_info_length, 0);
    left_buffer[left_info_length] = '\0';
    leftNeighborIP = strtok(left_buffer, ":");
    leftNeighborPort = atoi(strtok(NULL, ":"));
    delete[] left_buffer;

    // cout << leftNeighborPort << "RECEIVE4:::::LLLeft" << endl;

    // Receive right neighbor info
    int right_info_length;
    recv(ringmaster_fd, &right_info_length, sizeof(right_info_length), 0);
    char *right_buffer = new char[right_info_length + 1];
    recv(ringmaster_fd, right_buffer, right_info_length, 0);
    right_buffer[right_info_length] = '\0';
    rightNeighborIP = strtok(right_buffer, ":");
    rightNeighborPort = atoi(strtok(NULL, ":"));
    delete[] right_buffer;
    // cout << rightNeighborPort << "RECEIVE5:::::LLLeft" << endl;
}

void Player::setupPlayerServer()
{
    // int sockfd, new_fd;
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    const char *PORT = "0"; 

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        cerr << "player_serv: cannot get address info for host" << endl;
        exit(EXIT_FAILURE);
    }
    // Create socket
    if ((asClientfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        cerr << "player_serv: cannot create socket" << endl;
        exit(EXIT_FAILURE);
    }
    // Set socket options to allow address reuse
    if (setsockopt(asClientfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        cerr << "player_serv: cannot setsockopt" << endl;
        exit(EXIT_FAILURE);
    }

    // Specify 0 in sin_port when calling bind, OS assign Port
    struct sockaddr_in *addr_in = (struct sockaddr_in *)(servinfo->ai_addr);
    addr_in->sin_port = 0;

    // Bind socket to addr and port
    if (bind(asClientfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        cerr << "player_serv: cannot bind socket" << endl;
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);

    if (listen(asClientfd, BACKLOG) == -1)
    {
        cerr << "player_serv: cannot listen on socket" << endl;
        exit(EXIT_FAILURE);
    }
}

void Player::communicateWithNeighbor()
{
    struct addrinfo hints, *res;
    char ipstr[INET6_ADDRSTRLEN];
    int port;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(leftNeighborIP.c_str(), to_string(leftNeighborPort).c_str(), &hints, &res) != 0)
    {
        cerr << "player_nei: Cannot get address for left neighbor" << endl;
        exit(EXIT_FAILURE);
    }
    asServerfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (asServerfd == -1)
    {
        cerr << "player_nei: Cannot create socket for left neighbor" << endl;
        exit(EXIT_FAILURE);
    }
    if (connect(asServerfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        std::cerr << "player_nei: Cannot connect to left neighbor" << endl;
        close(asServerfd);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);

    struct sockaddr_storage their_addr;
    socklen_t sin_size = sizeof(their_addr);
    asClientfd = accept(asClientfd, (struct sockaddr *)&their_addr, &sin_size);
    if (asClientfd == -1)
    {
        cerr << "player_nei: Cannot accept the right client" << endl;
        exit(EXIT_FAILURE);
    }
}


int Player::handleRingmasterInput() {
    ssize_t bytes_received = recv(ringmaster_fd, &potato, sizeof(potato), MSG_WAITALL);
    if (bytes_received <= 0) {
        cerr << "Error receiving potato from ringmaster: " << strerror(errno) << endl;
        exit(EXIT_FAILURE); 
    }
    if (potato.isGameEnds()) {
        //exit(EXIT_SUCCESS); 
        return 1;
    }
    forwardPotato(potato);
    return 0;
}

void Player::handleClientInput() {
    ssize_t bytes_received = recv(asClientfd, &potato, sizeof(potato), MSG_WAITALL);
    if (bytes_received <= 0) {
        
        exit(EXIT_FAILURE); 
    }
    forwardPotato(potato);
}

void Player::handleServerInput() {
    ssize_t bytes_received = recv(asServerfd, &potato, sizeof(potato), MSG_WAITALL);
    if (bytes_received <= 0) {
        
        exit(EXIT_FAILURE); 
    }
    forwardPotato(potato);
}

void Player::playerPlay() {
    fd_set read_fds;
    //Potato potato;
    srand((unsigned int)time(NULL) + player_id);

    while (1) {
        // Clear set
        FD_ZERO(&read_fds);
        // Put set
        FD_SET(ringmaster_fd, &read_fds);
        FD_SET(asClientfd, &read_fds);
        FD_SET(asServerfd, &read_fds);

        int max_fd = max(asClientfd, asServerfd);
        max_fd = max(ringmaster_fd, max_fd) + 1;
        int n = select(max_fd, &read_fds, NULL, NULL, NULL);
        if (n < 0) {
            cerr << "Error in player: Select error" << endl;
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(ringmaster_fd, &read_fds)) {
            int sig = handleRingmasterInput();
            if (sig == 1) {
                return;
            }
        }
        if (FD_ISSET(asClientfd, &read_fds)) {
            handleClientInput();
        }
        if (FD_ISSET(asServerfd, &read_fds)) {
            handleServerInput();
        }
    }
}


int main(int argc, char **argv)
{
    if (argc != 3)
    {
        cerr << "Number of command arguments incorrect" << endl;
        exit(EXIT_FAILURE);
    }
    const char *hostname = argv[1];
    const char *port = argv[2];

    Player *player = new Player(hostname, port);

    player->setupPlayerServer();

    player->communicateWithRingmaster();

    //  player->setupPlayerServer();
    player->communicateWithNeighbor();

    player->playerPlay();

    delete player;

    exit(EXIT_SUCCESS);
}
