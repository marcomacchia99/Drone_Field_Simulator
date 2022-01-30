#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_DRONES 4 //max drones number
#define MAX_X 80     //max x value
#define MAX_Y 40     //max y value

#define RED '\033[1;31m'
#define GREEN '\033[1;32m'
#define BLUE '\033[1;34m'
#define NC '\033[0m'

typedef struct drone_position_t
{
    //x position
    int x;
    //y position
    int y;

} drone_position;

//socket file descriptor
int fd_socket;
//socket connection with drones file descriptors
int fd_drones[MAX_DRONES] = {0, 0, 0, 0};
//drones number
int drones_no = 0;
//port number
int portno;

//array of drones position
drone_position positions[MAX_DRONES];

//array of drones status
int drones_status[MAX_DRONES] = {-1, -1, -1, -1};

//server and client addresses
struct sockaddr_in server_addr, client_addr;

//flag if the consumer received all the data
int flag_terminate_process = 0;

//variables for select function
struct timeval timeout;
fd_set readfds;
fd_set dronesfds;

//pointer to log file
FILE *logfile;

//This function checks if something failed, exits the program and prints an error in the logfile
int check(int retval)
{
    if (retval == -1)
    {
        fprintf(logfile, "\nmaster - ERROR (" __FILE__ ":%d) -- %s\n", __LINE__, strerror(errno));
        fflush(logfile);
        fclose(logfile);
        if (errno == EADDRINUSE)
        {
            printf("\tError: address already in use. please change port\n");
            fflush(stdout);
            exit(-100);
        }
        else
        {

            printf("\tAn error has been reported on log file.\n");
            fflush(stdout);
            exit(-1);
        }
    }
    return retval;
}

void close_program(int sig)
{
    if (sig == SIGTERM)
    {
        flag_terminate_process = 1;
    }
    return;
}

void check_new_connection()
{
    if (drones_no == MAX_DRONES)
        return;
    else
    {

        //set timeout for select
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;

        FD_ZERO(&readfds);
        //add the selected file descriptor to the selected fd_set
        FD_SET(fd_socket, &readfds);

        //take number of request
        int req_no = check(select(FD_SETSIZE + 1, &readfds, NULL, NULL, &timeout));
        for (int i = 0; i < req_no; i++)
        {
            //define client length
            int client_length = sizeof(client_addr);

            //enstablish connection
            fd_drones[drones_no] = check(accept(fd_socket, (struct sockaddr *)&client_addr, &client_length));
            if (fd_drones[drones_no] < 0)
            {
                check(-1);
            }
            //write on log file
            fprintf(logfile, "master - drone %d connected\n", drones_no + 1);
            fflush(logfile);
            drones_status[drones_no] = 1;
            drones_no++;
        }
    }
    return;
}

void check_move_request()
{
    if (drones_no == 0)
        return;
    else
    {

        //set timeout for select
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;

        FD_ZERO(&dronesfds);
        //add the selected file descriptor to the selected fd_set
        for (int i = 0; i < drones_no; i++)
        {
            FD_SET(fd_drones[i], &dronesfds);
        }

        //take number of request
        int req_no = check(select(FD_SETSIZE + 1, &dronesfds, NULL, NULL, &timeout));

        //for every request
        for (int i = 0; i < req_no; i++)
        {
            //find the drone that has made the request
            for (int j = 0; j < drones_no; j++)
            {
                //if this drone has made a request
                if (FD_ISSET(fd_drones[j], &dronesfds))
                {
                    //read requested position
                    drone_position request_position;
                    check(read(fd_drones[j], &request_position, sizeof(request_position)));

                    //if MAX_X*2 and MAX_Y*2 is sent via position, the drone becomes in idle state
                    if (request_position.x == MAX_X * 2 && request_position.y == MAX_Y * 2)
                    {
                        drones_status[j] = 0;
                    }
                    else
                    {
                        //check if the movement is safe
                        int verdict = check_safe_movement(j, request_position);
                        //tell the drones if it can move
                        write(fd_drones[j], &verdict, sizeof(verdict));

                        //if the drone can move, update its position
                        if (verdict)
                        {
                            positions[j].x == request_position.x;
                            positions[j].y == request_position.y;
                        }
                        break;
                    }
                }
            }
        }
    }
    return;
}

int check_safe_movement(int drone, drone_position request_position)
{
    //check for map edges
    if (request_position.x < 0 || request_position.x > MAX_X || request_position.y < 0 || request_position.y > MAX_Y)
        return 0;

    for (int i = 0; i < drones_no; i++)
    {
        //check if there can be a collision between others drones
        if (positions[i].x == request_position.x && positions[i].y == request_position.y)
            return 0;
    }
    return 1;
}

void update_map()
{

    return;
}

int main(int argc, char *argv[])
{

    //open log file in write mode
    // logfile = fopen("./../logs/master_log.txt", "a");
    logfile = fopen("master_log.txt", "a");
    if (logfile == NULL)
    {
        printf("an error occured while creating master's log File\n");
        return 0;
    }
    fprintf(logfile, "starting master\n");
    fflush(logfile);

    // //getting port number
    // if (argc < 2)
    // {
    //     fprintf(stderr, "master - ERROR, no port provided\n");
    //     exit(0);
    // }
    // portno = atoi(argv[1]);

    portno = 8080;

    //write on log file
    fprintf(logfile, "master - received portno %d\n", portno);
    fflush(logfile);

    //create socket
    fd_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_socket < 0)
    {
        check(-1);
    }

    //write on log file
    fprintf(logfile, "master - socket created\n");
    fflush(logfile);

    //set server address for connection
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(portno);

    //bind socket
    check(bind(fd_socket, (struct sockaddr *)&server_addr,
               sizeof(server_addr)));

    //write on log file
    fprintf(logfile, "master - socket bound\n");
    fflush(logfile);

    //wait for connections
    check(listen(fd_socket, 5));

    while (!flag_terminate_process)
    {
        //check if a new drone send a connection request
        check_new_connection();

        //check if some drones want to move
        check_move_request();

        //update drones map on the console
        update_map();
    }

    //close sockets
    check(close(fd_socket));
    for (int i = 0; i < drones_no; i++)
    {
        check(close(fd_drones[i]));
    }

    //write on log file
    fprintf(logfile, "master - all socket closed\n");
    fflush(logfile);

    //close log file
    fclose(logfile);

    return 0;
}