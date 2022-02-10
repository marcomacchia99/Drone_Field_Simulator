// Compile with: gcc drone.c -lm -lncurses -o drone

/* LIBRARIES */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <netdb.h>
#include <sys/socket.h>
#include <math.h>
#include <ncurses.h>

#define MAX_X 80        // max x value
#define MAX_Y 40        // max y value
#define MAX_CHARGE 100  // max battery level
#define STATUS_IDLE 0   // idle drone status
#define STATUS_ACTIVE 1 // active drone status

/* FUNCTIONS HEADERS */
float float_rand(float min, float max);
void logPrint(char *string);
void compute_next_position();
void recharge(int sockfd);
void loading_bar(int percent, int buf_size);
void setup_colors();
void setup_map();
int change_velocity(int dt);

void signal_handler(int sig)
{
    /* Function to handle the SIGWINCH signal. The OS send this signal to the process when the size of
    the terminal changes. */

    if (sig == SIGWINCH)
    {
        /* If the size of the terminal changes, clear and restart the grafic interface. */
        endwin();
        initscr(); // Init the console screen.
        clear();
        setup_map();
        refresh();
    }
}

typedef struct drone_position_t
{
    //timestamp of message
    time_t timestamp;
    //drone status
    int status;
    // x position
    int x;
    // y position
    int y;
} drone_position;

/* Defining CHECK() tool. By using this method the code results ligher and cleaner */
#define CHECK(X) (                                                                                            \
{                                                                                                         \
    int __val = (X);                                                                                      \
    (__val == -1 ? (                                                                                      \
     {                                                                                  \
         fprintf(stderr, "ERROR (" __FILE__ ":%d) -- %s\n", __LINE__, strerror(errno)); \
         exit(-1);                                                                      \
         -1;                                                                            \
     })                                                                                 \
    : __val);                                                                                \
})

/* GLOBAL VARIABLES */

int command = 0;          // Command received.
FILE *log_file;           // log file.
int battery = MAX_CHARGE; // this variable takes into account the battry status
bool map[40][80] = {};    // matrix that represent the maze. '1' stands for a visited position, '0' not visited.
int step = 1;             // drone movement step
bool direction = true;    // toggle for exploring direction
char str[50];             // string buffer
int dt = 100000;          // time step

drone_position next_position = {.status = STATUS_ACTIVE};                           // next drone position                                 // position for idle status
drone_position actual_position = {.status = STATUS_ACTIVE, .x = 10, .y = 10};       // actual drone position

/* FUNCTIONS */
float float_rand(float min, float max)
{

    /* Function to generate a randomic position. */

    float scale = rand() / (float)RAND_MAX;
    return min + scale * (max - min); // [min, max]
}

void logPrint(char *string)
{
    /* Function to print on log file adding time stamps. */
    time_t ltime = time(NULL);
    fprintf(log_file, "%.19s: %s", ctime(&ltime), string);
    fflush(log_file);
}

void compute_next_position()
{
    /* Function to compute a new position. A better coverage algorithm will be implemented */
    int cycles = 0;
    do
    {
        if (command == 0) // next position not allowed

            step = -step; // changes direction

        if (direction) // exploring in the horizontal direction
        {
            next_position.x = actual_position.x + step;                              // increases the x coordinate with a step
            next_position.y = (int)round(actual_position.y + float_rand(-0.8, 0.8)); // randomly modifies the y coordinate
        }
        else
        {
            next_position.y = actual_position.y + step;                              // increases the y coordinate with a step
            next_position.x = (int)round(actual_position.x + float_rand(-0.8, 0.8)); // randomly modifies the x coordinate
        }

        if (next_position.x > MAX_X - 1) // right maze bound reached
        {
            step = -step; // change step direction
            next_position.x = MAX_X - 1;
        }

        if (next_position.x < 0) // left maze bound reached
        {
            step = -step; // change step direction
            next_position.x = 0;
        }

        if (next_position.y > MAX_Y - 1) // upper maze bound reached
        {
            next_position.y = MAX_Y - 1; // change step direction
            step = -step;
        }

        if (next_position.y < 0) // bottom maze bound reached
        {
            next_position.y = 0; // change step direction
            step = -step;
        }
        cycles++; // increment cycles

    } while (map[next_position.y][next_position.x] == 1 && cycles < 10); // if an allowed positon is not found for more than 10 times, stop the while cycle

    sprintf(str, "Next X = %d, Next Y = %d\n", next_position.x, next_position.y);
    mvaddstr(42, 0, str);
    refresh();

    logPrint(str);
}

void recharge(int sockfd)
{
    attron(COLOR_PAIR(2));
    mvaddstr(43, 0, "Low battery, landing for recharging.");
    attroff(COLOR_PAIR(2));

    logPrint("Low battery, landing for recharging.\n");
    next_position.timestamp = time(NULL);
    next_position.status = STATUS_IDLE;
    CHECK(write(sockfd, &next_position, sizeof(drone_position))); // dummy command for idle status

    attron(COLOR_PAIR(1));
    mvaddstr(40, 50, "Recharging...");
    attroff(COLOR_PAIR(1));
    refresh();

    for (int i = 1; i <= MAX_CHARGE; i++)
    {
        usleep(50000);
        loading_bar(i, MAX_CHARGE); // graphical tool that represents a recharging bar
    }

    battery = MAX_CHARGE;
    next_position.status = STATUS_ACTIVE;
    direction = !direction; // once the battery is fully charged, change exploration direction

    logPrint("Battery fully recharged.\n");

    mvaddstr(40, 50, "               ");
    mvaddstr(43, 0, "                                    ");
    refresh();
}

void loading_bar(int percent, int buf_size)
{

    /*This is a simple graphical feature that we implemented. It is a loading bar that graphically
        rapresents the progress percentage of battery recharging.  */

    const int PROG = 30;
    int num_chars = (percent / (buf_size / 100)) * PROG / 100;

    attron(COLOR_PAIR(4));
    mvaddch(40, 0, '[');
    attroff(COLOR_PAIR(4));

    int i;
    for (i = 0; i <= num_chars; i++)
    {
        attron(COLOR_PAIR(3));
        mvaddch(40, i + 1, '#');
        attron(COLOR_PAIR(3));
    }

    for (int j = 0; j < PROG - num_chars - 1; j++)
    {
        mvaddch(40, i + j + 1, ' ');
    }

    attron(COLOR_PAIR(4));
    sprintf(str, "] %d %% BATTERY  ", percent / (buf_size / 100));
    mvaddstr(40, 31, str);
    attroff(COLOR_PAIR(4));

    refresh();
}

void setup_colors()
{ // colors using ncurses library

    if (!has_colors())
    {
        endwin();
        printf("This terminal is not allowed to print colors.\n");
        exit(1);
    }

    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_WHITE, COLOR_BLACK);
}

void setup_map()
{
    for (int i = 0; i < 40; i++)
    {
        for (int j = 0; j < 80; j++)
        {
            if (map[i][j] == false)
            {
                attron(COLOR_PAIR(1));
                mvaddch(i, j, '0');
                attroff(COLOR_PAIR(1));
            }
            else
            {
                attron(COLOR_PAIR(2));
                mvaddch(i, j, '1');
                attroff(COLOR_PAIR(2));
            }
        }
    }
}


int change_velocity(int dt){

    char input;
    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(STDIN_FILENO, &rset);
    int ret = CHECK(select(FD_SETSIZE, &rset, NULL, NULL, &tv));

    if (FD_ISSET(STDIN_FILENO, &rset) != 0) { // There is something to read!
        input = getchar(); // Update the command.
    }
    if (input == 49){
        dt = 200000;
    }
    else if (input == 50){
        dt = 100000;
    }
    else if (input == 51){
        dt = 70000;
    }
    else if (input == 52){
        dt = 40000;
    }
    else if (input == 53){
        logPrint("Exit command received!");
        logPrint("Killing the process...");
        endwin();
        kill(getpid(), SIGKILL);
    }

    return dt;
}


/* MAIN */
int main()
{
    int sockfd;                   // File descriptors.
    int portno = 8080;            // Used port number.
    struct sockaddr_in serv_addr; // Address of the server and address of the client.
    srand(time(NULL));            // Randomize

    /* Open and write on the log file. */
    log_file = fopen("./log_file.txt", "w");
    logPrint("Log file successfully created.\n");
    sprintf(str, "Drone 007 PID is: %d \n", getpid());
    logPrint(str);

    /* Signals that the process can receive. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = SA_RESTART;

    /* sigaction for SIGWINCH */
    CHECK(sigaction(SIGWINCH, &sa, NULL));

    /* Opens socket */
    sockfd = CHECK(socket(AF_INET, SOCK_STREAM, 0)); // Creates a new socket.

    /* Initialize the serv_addr struct. */
    bzero((char *)&serv_addr, sizeof(serv_addr)); // Sets all arguments of serv_addr to zero.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    CHECK(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))); // Listens on the socket for connections. (struct sockaddr *)&serv_addr

    logPrint("Connection successfully established.\n");

    /* Screen setup. */
    initscr(); // Init the console screen.
    refresh();
    clear();
    setup_colors();
    setup_map();


    while (1)
    {
        if (battery == 0) // Low battery
            recharge(sockfd);

        compute_next_position(); // looks for a new position
        next_position.timestamp = time(NULL);
        CHECK(write(sockfd, &next_position, sizeof(drone_position))); // Writes the next position.

        mvaddstr(44, 0, "Data correctly written into the socket");
        refresh();

        logPrint("Data correctly written into the socket.\n");

        CHECK(read(sockfd, &command, sizeof(int))); // Reads a feedback. ~ command = 0 if next_position is not allowed. ~ command = 1 if next posotion is allowed

        mvaddstr(45, 0, "Feedback correctly read from master process");
        refresh();

        logPrint("Feedback correctly read from master process. \n");

        // Not allowed
        if (command == 0)
        {
            attron(COLOR_PAIR(2));
            mvaddstr(46, 0, "Drone stopped, position not allowed");
            attroff(COLOR_PAIR(2));
            refresh();

            logPrint("Drone stopped, position not allowed.\n");
        }

        // Allowed
        if (command == 1)
        {
            attron(COLOR_PAIR(1));
            mvaddstr(46, 0, "Position allowed, drone is moving   ");
            attroff(COLOR_PAIR(1));
            refresh();

            logPrint("Position allowed, drone is moving.\n");

            actual_position = next_position;
            map[actual_position.y][actual_position.x] = true;
            attron(COLOR_PAIR(2));
            mvaddch(actual_position.y, actual_position.x, '1');
            attroff(COLOR_PAIR(2));
        }

        attron(COLOR_PAIR(3));
        mvaddstr(47, 0, "Select one of the following commands for changing the drone's velocity:");
        mvaddstr(48, 0, "[1] SLOW");
        mvaddstr(49, 0, "[2] DEFAULT");
        mvaddstr(50, 0, "[3] FAST");
        mvaddstr(51, 0, "[4] VERY FAST");
        attroff(COLOR_PAIR(3));
        attron(COLOR_PAIR(2));
        mvaddstr(52, 0, "[5] Quit the application");
        attroff(COLOR_PAIR(2));

        refresh();

        // Battery decreases
        battery--;
        loading_bar(battery, MAX_CHARGE); // graphical tool that represents a decharging bar

        dt = change_velocity(dt); // change the dt time quantum depending on the slected drone velocity
        
        usleep(dt); // sleep for dt second

    } // End of the while cycle.

    fclose(log_file); // Close log file.

    return 0;
}