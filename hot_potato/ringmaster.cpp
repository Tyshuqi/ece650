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
#include "potato.h"

using namespace std;

class Ringmaster
{
public:
    int port_num;
    int num_players;
    int num_hops;
    int *player_fds;
    int *player_ports;
    string *player_ips;

    Ringmaster(char *argv[]) : port_num(atoi(argv[1])), num_players(atoi(argv[2])), num_hops(atoi(argv[3]))
    {
        // for player_fds
        player_fds = new int[num_players];
        // for  player_ports
        player_ports = new int[num_players];
        // player_ips
        player_ips = new string[num_players];
    }

    ~Ringmaster()
    {
        delete[] player_fds;
        delete[] player_ips;
        delete[] player_ports;
    }

    const char *get_in_addr(struct sockaddr *sa, char *s, size_t maxlen)
    {
        if (sa->sa_family == AF_INET)
        { // IPv4
            struct sockaddr_in *addr_in = (struct sockaddr_in *)sa;
            return inet_ntop(AF_INET, &addr_in->sin_addr, s, maxlen);
        }
        else
        { // IPv6
            struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)sa;
            return inet_ntop(AF_INET6, &addr_in6->sin6_addr, s, maxlen);
        }
    }

    void setupServer(char *PORT);
    void sendNeighbour();
    void startAndEndGame();
};

void Ringmaster::setupServer(char *PORT)
{
    int sockfd;                         // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo;   // ssqomit *p; //hints: specify criteria for the desired socket addresses//servinfo:actual socket address information returned by getaddrinfo()
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    // Get addr info for host
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        cerr << "server: cannot get address info for host" << endl;
        exit(EXIT_FAILURE);
    }
    // Create socket
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        cerr << "server: cannot create socket" << endl;
        exit(EXIT_FAILURE);
    }
    // Set socket options to allow address reuse
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        cerr << "server: cannot setsockopt" << endl;
        exit(EXIT_FAILURE);
    }
    // Bind socket to addr and port
    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        // close(sockfd);
        cerr << "server: cannot bind socket" << endl;
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1)
    {
        cerr << "server: cannot listen on socket" << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Potato Ringmaster" << endl;
    cout << "Players = " << num_players << endl;
    cout << "Hops = " << num_hops << endl;

    // accept 
    for (int i = 0; i < num_players; i++)
    {
        sin_size = sizeof(their_addr);
        int cur_player_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        // Temporary storage for the port
        int temp_port;

        player_fds[i] = cur_player_fd;
        // Get ip of players
        player_ips[i] = get_in_addr((struct sockaddr *)&their_addr, s, sizeof(s));

        send(cur_player_fd, &i, sizeof(i), 0);
        send(cur_player_fd, &num_players, sizeof(num_players), 0);

        // recv(cur_player_fd, &player_ports[i], sizeof(player_ports[i]), MSG_WAITALL);
        ssize_t bytes_received = recv(cur_player_fd, &temp_port, sizeof(temp_port), MSG_WAITALL);

        if (bytes_received == -1)
        {
            // Error occurred in receiving data
            cerr << "Error in receiving data from player " << i << ": " << strerror(errno) << endl;
            close(cur_player_fd);
            exit(EXIT_FAILURE);
        }
        else if (bytes_received == 0)
        {
            // Connection has been closed
            cerr << "Player " << i << " has closed the connection unexpectedly." << endl;
            close(cur_player_fd);
        }
        else if (bytes_received == sizeof(temp_port))
        {
            player_ports[i] = temp_port;
        }
        else
        {
            // Received an unexpected number of bytes
            cerr << "Unexpected number of bytes received from player " << i << ". Expected: " << sizeof(temp_port) << ", but got: " << bytes_received << endl;
            close(cur_player_fd);
        }

        cout << "Player " << i << " is ready to play" << endl;
    }
}

void Ringmaster::sendNeighbour()
{
    for (int i = 0; i < num_players; i++)
    {
        int cur_player_fd = player_fds[i];
        int left_neighbor_id = (i - 1 + num_players) % num_players;
        int right_neighbor_id = (i + 1) % num_players;

        // Prepare left neighbor's info
        string left_info = player_ips[left_neighbor_id] + ":" + to_string(player_ports[left_neighbor_id]);
        // Prepare right neighbor's info
        string right_info = player_ips[right_neighbor_id] + ":" + to_string(player_ports[right_neighbor_id]);

        // Send left neighbor's info
        int left_info_length = left_info.length();
        send(cur_player_fd, &left_info_length, sizeof(left_info_length), 0); // Send the length first
        send(cur_player_fd, left_info.c_str(), left_info_length, 0);         // Then send the actual data
        // cout << left_info.c_str() << "LLLeft" << endl;

        // Send right neighbor's info
        int right_info_length = right_info.length();
        send(cur_player_fd, &right_info_length, sizeof(right_info_length), 0);
        send(cur_player_fd, right_info.c_str(), right_info_length, 0);

        // cout << right_info.c_str() << "RRRRight" << endl;
    }
}

void Ringmaster::startAndEndGame()
{
    Potato potato(num_hops);
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
// edge case
    if (num_hops == 0)
    {
        for (int i = 0; i < num_players; i++)
        {
            send(player_fds[i], &potato, sizeof(potato), 0);
        }
        //Close all player sockets
        for (int i = 0; i < num_players; i++)
        {
            close(player_fds[i]);
        }
        potato.printTrace();
        exit(EXIT_SUCCESS);
    }

    // Randomly select a player and send the “potato” to the selected player
    srand((unsigned int)time(NULL));
    int random = rand() % num_players;
    send(player_fds[random], &potato, sizeof(potato), 0);
    cout << "Ready to start the game, sending potato to player " << random << endl;

    int max_fd = 0;
    for (int i = 0; i < num_players; i++)
    {
        if (player_fds[i] > max_fd)
        {
            max_fd = player_fds[i];
        }
    }
    max_fd = max_fd + 1;

    // Put in set
    for (int i = 0; i < num_players; i++)
    {
        FD_SET(player_fds[i], &read_fds);
    }

    // Wait for activity on any player socket
    int n = select(max_fd, &read_fds, NULL, NULL, NULL);
    if (n < 0)
    {
        cerr << "Select error" << endl;
        exit(EXIT_FAILURE);
    }

    // Check each player socket for data
    for (int i = 0; i < num_players; i++)
    {
        if (FD_ISSET(player_fds[i], &read_fds))
        {
            // Game ends when ringmaster recv potato from one of player
            ssize_t bytes_received = recv(player_fds[i], &potato, sizeof(potato), MSG_WAITALL);
            if (bytes_received == -1)
            {
                cerr << "Error receiving potato from player " << i << ": " << strerror(errno) << endl;
                close(player_fds[i]);
                exit(EXIT_FAILURE);
            }
            break;
        }
    }
    // Shut down
    for (int i = 0; i < num_players; i++)
    {
        send(player_fds[i], &potato, sizeof(potato), 0);
    }
    // Close all player sockets
    for (int i = 0; i < num_players; i++)
    {
        close(player_fds[i]);
    }
    potato.printTrace();
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        cerr << "Number of command arguments incorrect" << endl;
        exit(EXIT_FAILURE);
    }
    if (atoi(argv[2]) <= 1)
    {
        cerr << " Number of players must greater than 1" << endl;
        exit(EXIT_FAILURE);
    }
    if (atoi(argv[3]) < 0 || atoi(argv[3]) > 512)
    {
        cerr << "Number of hops invalid" << endl;
        exit(EXIT_FAILURE);
    }
    char *PORT = argv[1];
    Ringmaster *master = new Ringmaster(argv);

    master->setupServer(PORT);
    master->sendNeighbour();
    master->startAndEndGame();

    delete master;
    exit(EXIT_SUCCESS);
}
