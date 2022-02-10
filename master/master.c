// compile with gcc master.c - lncurses - o master

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
#include <ncurses.h>
#include <time.h>

#define MAX_DRONES 4 // max drones number
#define MAX_X 80     // max x value
#define MAX_Y 40     // max y value

// #define RED '\033[1;31m'
// #define GREEN '\033[1;32m'
// #define BLUE '\033[1;34m'
// #define GRAY '\033[1;30m'
// #define NC '\033[0m'

#define DRONE '*'
#define EXPLORED '.'

#define STATUS_IDLE 0
#define STATUS_ACTIVE 1

typedef struct drone_position_t
{
    // timestamp of message
    time_t timestamp;
    // drone status
    int status;
    // x position
    int x;
    // y position
    int y;

} drone_position;

// socket file descriptor
int fd_socket;
// socket connection with drones file descriptors
int fd_drones[MAX_DRONES] = {0, 0, 0, 0};
// drones number
int drones_no = 0;
// port number
int portno;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// array of drones position
drone_position positions[MAX_DRONES];

// array of drones status
int drones_status[MAX_DRONES] = {-1, -1, -1, -1};

// drone_position drones[MAX_DRONES];

//////////////////////////////////////////////////////////////////////////////////////////////////////

int explored_positions[MAX_X][MAX_Y] = {0};

// server and client addresses
struct sockaddr_in server_addr, client_addr;

// flag if the consumer received all the data
int flag_terminate_process = 0;

// variables for select function
struct timeval td;
fd_set readfds;
fd_set dronesfds;

// pointer to log file
FILE *logfile;

// variable for logging event time
time_t logtime;

// functions headers
int check(int retval);
void close_program(int sig);
void check_new_connection();
int check_safe_movement(int drone, drone_position request_position);
void check_move_request();
void update_map();
void setup_colors();
void init_console();

// This function checks if something failed, exits the program and prints an error in the logfile
int check(int retval)
{
    if (retval == -1)
    {
        logtime = time(NULL);
        fprintf(logfile, "%.19s: master - ERROR (" __FILE__ ":%d) -- %s\n", ctime(&logtime), __LINE__, strerror(errno));
        fflush(logfile);
        if (errno == EINTR)
        {
            return retval;
        }
        fclose(logfile);
        if (errno == EADDRINUSE)
        {
            endwin();
            printf("\tError: address already in use. please change port\n");
            fflush(stdout);
            exit(-100);
        }
        else
        {
            endwin();
            printf("\tAn error has been reported on log file.\n");
            fflush(stdout);
            exit(-1);
        }
    }
    return retval;
}

#define CHECK(X) (                                                                                                                                        \
    {                                                                                                                                                     \
        int __val = (X);                                                                                                                                  \
        (__val == -1 ? (                                                                                                                                  \
                           {                                                                                                                              \
                               fprintf(logfile, "%.19s: master - ERROR (" __FILE__ ":%d) -- %s\n", ctime(&logtime), __LINE__, strerror(errno));           \
                               fflush(logfile);                                                                                                           \
                               (errno == EINTR ? ()                                                                                                       \
                                               : (                                                                                                        \
                                                     {                                                                                                    \
                                                         (errono == EADDRINUSE ? (                                                                        \
                                                                                     {                                                                    \
                                                                                         fclose(logfile);                                                 \
                                                                                         endwin();                                                        \
                                                                                         printf("\tError: address already in use. please change port\n"); \
                                                                                         fflush(stdout);                                                  \
                                                                                         exit(-100);                                                      \
                                                                                         -100;                                                            \
                                                                                     })                                                                   \
                                                                               : (                                                                        \
                                                                                     {                                                                    \
                                                                                         fclose(logfile);                                                 \
                                                                                         endwin();                                                        \
                                                                                         printf("\tAn error has been reported on log file.\n");           \
                                                                                         fflush(stdout);                                                  \
                                                                                         exit(-1);                                                        \
                                                                                         -1;                                                              \
                                                                                     }));                                                                 \
                                                     }));                                                                                                 \
                           })                                                                                                                             \
                     : __val);                                                                                                                            \
    })

void close_program(int sig)
{
    if (sig == SIGTERM)
    {
        flag_terminate_process = 1;
    }
    return;
}

// handler for terminal resizing
void signal_handler(int sig)
{

    if (sig == SIGWINCH)
    {
        // force console size
        // printf("\e[8;%d;%dt", MAX_Y + 2, MAX_X + 4);
    }
}

void check_new_connection()
{
    if (drones_no == MAX_DRONES)
        return;
    else
    {

        // set timeout for select
        td.tv_sec = 0;
        td.tv_usec = 1000;

        FD_ZERO(&readfds);
        // add the selected file descriptor to the selected fd_set
        FD_SET(fd_socket, &readfds);

        // take number of request
        int req_no = check(select(FD_SETSIZE + 1, &readfds, NULL, NULL, &td));
        for (int i = 0; i < req_no; i++)
        {
            // define client length
            int client_length = sizeof(client_addr);

            // enstablish connection
            fd_drones[drones_no] = check(accept(fd_socket, (struct sockaddr *)&client_addr, &client_length));
            if (fd_drones[drones_no] < 0)
            {
                check(-1);
            }
            // write on log file
            logtime = time(NULL);
            fprintf(logfile, "%.19s: master - drone %d connected\n", ctime(&logtime), drones_no + 1);

            fflush(logfile);
            drones_status[drones_no] = 1;
            drones_no++;
        }
    }
    return;
}

int check_safe_movement(int drone, drone_position request_position)
{
    // check for map edges
    if (request_position.x < 0 || request_position.x > MAX_X || request_position.y < 0 || request_position.y > MAX_Y)
        return 0;

    // for every drone...
    for (int i = 0; i < drones_no; i++)
    {
        if (i != drone)
        {

            // check if the position requested falls in a 3x3 area surrounding another drone
            for (int j = positions[i].x - 1; j <= positions[i].x + 1; j++)
            {
                for (int k = positions[i].y - 1; k <= positions[i].y + 1; k++)
                {
                    // check if there can be a collision between others drones
                    if (j == request_position.x && k == request_position.y)
                        return 0;
                }
            }
        }
    }
    return 1;
}

void check_move_request()
{
    if (drones_no == 0)
        return;
    else
    {

        // set timeout for select
        td.tv_sec = 0;
        td.tv_usec = 1000;

        FD_ZERO(&dronesfds);
        // add the selected file descriptor to the selected fd_set
        for (int i = 0; i < drones_no; i++)
        {
            FD_SET(fd_drones[i], &dronesfds);
        }

        // take number of request
        int req_no = check(select(FD_SETSIZE + 1, &dronesfds, NULL, NULL, &td));

        // for every request
        for (int i = 0; i < req_no; i++)
        {
            // find the drone that has made the request
            for (int j = 0; j < drones_no; j++)
            {
                // if this drone has made a request
                if (FD_ISSET(fd_drones[j], &dronesfds))
                {
                    // read requested position
                    drone_position request_position;
                    check(read(fd_drones[j], &request_position, sizeof(request_position)));

                    // check drone status, idle or active
                    if (request_position.status == STATUS_IDLE)
                    {
                        // change drone status
                        //  status 1 means active, status 0 means idle. (Initial) status -1 means drone is not connected
                        drones_status[j] = 0;
                        // update map
                        update_map();
                    }
                    else if (request_position.status == STATUS_ACTIVE)
                    {
                        if (drones_status[j] == 0)
                            drones_status[j] = 1;

                        // check if the movement is safe
                        int verdict = check_safe_movement(j, request_position);
                        // tell the drones if it can move
                        write(fd_drones[j], &verdict, sizeof(verdict));

                        // if the drone can move, update its position
                        if (verdict)
                        {
                            // update new position
                            positions[j].x = request_position.x;
                            positions[j].y = request_position.y;

                            // update explored positions
                            explored_positions[request_position.x][request_position.y] = 1;

                            // update map
                            update_map();
                        }
                        break;
                    }
                }
            }
        }
    }
    return;
}

void update_map()
{

    // Reset map
    for (int i = 2; i <= MAX_X + 1; i++)
    {
        for (int j = 1; j <= MAX_Y; j++)
        {
            mvaddch(j, i, ' ');
        }
    }

    // print already explored positions in gray
    for (int i = 0; i < MAX_X; i++)
    {
        for (int j = 0; j < MAX_Y; j++)
        {
            if (explored_positions[i][j] == 1)
            {
                // move cursor to the explored position and add marker
                mvaddch(j + 1, i + 2, EXPLORED);
            }
        }
    }

    // print actual drone position
    for (int i = 0; i < drones_no; i++)
    {
        // blue drone are idle
        if (drones_status[i] == 0)
        {
            attron(COLOR_PAIR(2));
            // print drone
            mvaddch(positions[i].y + 1, positions[i].x + 2, DRONE);
            attroff(COLOR_PAIR(2));
        }
        // green drone are active
        else if (drones_status[i] == 1)
        {
            attron(COLOR_PAIR(1));
            // print drone
            mvaddch(positions[i].y + 1, positions[i].x + 2, DRONE);
            attroff(COLOR_PAIR(1));
        }
    }

    move(43, 0);

    printw("Drones connected:  %d\n", drones_no);

    refresh(); // Send changes to the console.

    return;
}

// setup console colors
void setup_colors()
{

    if (!has_colors())
    {
        endwin();
        printf("This terminal is not allowed to print colors.\n");
        exit(1);
    }

    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
    init_pair(3, COLOR_BLACK, COLOR_BLACK);
    init_pair(4, COLOR_WHITE, COLOR_BLACK);
}

// Function to initialize the console GUI.
// The ncurses library is used.
void init_console()
{

    // set console size
    printf("\e[8;%d;%dt", MAX_Y + 4, MAX_X + 4);

    // init console
    initscr();
    refresh();
    clear();
    setup_colors();

    // resize curses terminal
    resize_term(MAX_Y + 4, MAX_X + 4);

    // hide cursor
    curs_set(0);

    // print top wall
    addstr("||");
    for (int j = 2; j < MAX_X + 2; j++)
    {
        mvaddch(0, j, '=');
    }
    addstr("||");

    // for each line...
    for (int i = 0; i < MAX_Y; i++)
    {
        // print left wall
        addstr("||");

        for (int j = 0; j < MAX_X; j++)
            addch(' ');
        // print right wall
        addstr("||");
    }

    // print bottom wall
    addstr("||");
    for (int j = 2; j < MAX_X + 2; j++)
        mvaddch(MAX_Y + 1, j, '=');
    addstr("||");

    printw("\nDrones connected:  %d", drones_no);

    refresh(); // Send changes to the console.
}

int main(int argc, char *argv[])
{

    // init array of drones position
    for (int i = 0; i < MAX_DRONES; i++)
    {
        positions[i].timestamp = 0;
        positions[i].status = -1;
        positions[i].x = -1;
        positions[i].y = -1;
    }

    // init console GUI
    init_console();

    // handle signal
    signal(SIGWINCH, signal_handler);

    // open log file in write mode
    //  logfile = fopen("./../logs/master_log.txt", "a");
    logfile = fopen("master_log.txt", "a");
    if (logfile == NULL)
    {
        printf("an error occured while creating master's log File\n");
        return 0;
    }

    // variable for logging event time
    logtime = time(NULL);
    fprintf(logfile, "\n%.19s: starting master\n", ctime(&logtime));
    fflush(logfile);

    // //getting port number
    // if (argc < 2)
    // {
    //     fprintf(stderr, "master - ERROR, no port provided\n");
    //     exit(0);
    // }
    // portno = atoi(argv[1]);

    portno = 8080;

    // write on log file
    logtime = time(NULL);
    fprintf(logfile, "%.19s: master - received portno %d\n", ctime(&logtime), portno);
    fflush(logfile);

    // create socket
    fd_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_socket < 0)
    {
        check(-1);
    }

    // write on log file
    logtime = time(NULL);
    fprintf(logfile, "%.19s: master - socket created\n", ctime(&logtime));

    fflush(logfile);

    // set server address for connection
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(portno);

    // bind socket
    check(bind(fd_socket, (struct sockaddr *)&server_addr,
               sizeof(server_addr)));

    // write on log file
    logtime = time(NULL);
    fprintf(logfile, "%.19s: master - socket bound\n", ctime(&logtime));
    fflush(logfile);

    // wait for connections
    check(listen(fd_socket, 5));

    while (!flag_terminate_process)
    {
        // check if a new drone send a connection request
        check_new_connection();

        // check if some drones want to move
        check_move_request();
    }

    // close ncurses console
    endwin();

    // close sockets
    check(close(fd_socket));
    for (int i = 0; i < drones_no; i++)
    {
        check(close(fd_drones[i]));
    }

    // write on log file
    logtime = time(NULL);
    fprintf(logfile, "%.19s: master - all socket closed\n", ctime(&logtime));
    fflush(logfile);

    // close log file
    fclose(logfile);

    return 0;
}