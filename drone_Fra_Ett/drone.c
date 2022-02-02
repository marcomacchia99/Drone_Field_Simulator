// Compile with: gcc drone.c -lm -o drone

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

#define MAX_X 80 // max x value
#define MAX_Y 40 // max y value
#define MAX_CHARGE 100

/* COLORS */
#define RESET "\033[0m"
#define BHBLK "\e[1;90m"
#define BHRED "\e[1;91m"
#define BHGRN "\e[1;92m"
#define BHYEL "\e[1;93m"
#define BHBLU "\e[1;94m"
#define BHMAG "\e[1;95m"
#define BHCYN "\e[1;96m"
#define BHWHT "\e[1;97m"

typedef struct drone_position_t
{
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

void signal_handler( int sig ) {
    /* Function to handle the SIGWINCH signal. The OS send this signal to the process when the size of
    the terminal changes. */

    if (sig == SIGWINCH) {
    /* If the size of the terminal changes, clear and restart the grafic interface. */
        endwin();
        initscr(); // Init the console screen.
        refresh();
        clear();
    }
}

/* GLOBAL VARIABLES */

int command = 0; // Command received.
// FILE *log_file;              // log file.
drone_position actual_position; // actual drone position
drone_position next_position;   // next drone position
drone_position landed;          // position for idle status
int battery;                    // this variable takes into account the battry status
bool map[40][80] = {};          // matrix that represent the maze. '1' stands for a visited position, '0' not visited.
int step = 1;                   // drone movement step
bool direction = true;          // toggle for exploring direction
char str[50];

/* FUNCTIONS HEADERS */
float float_rand(float min, float max);
// void logPrint ( char * string );
void compute_next_position();
void recharge(int sockfd);
void loading_bar(int percent, int buf_size);
void setup_colors();

/* FUNCTIONS */
float float_rand(float min, float max)
{

    /* Function to generate a randomic position. */

    float scale = rand() / (float)RAND_MAX;
    return min + scale * (max - min); // [min, max]
}

// void logPrint ( char * string ) {
//     /* Function to print on log file adding time stamps. */

//     time_t ltime = time(NULL);
//     fprintf( log_file, "%.19s: %s", ctime( &ltime ), string );
//     fflush(log_file);
// }
int test = 0;
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
        cycles++;                                                        // increment cycles
    } while (map[next_position.y][next_position.x] == 1 && cycles < 10); // if an allowed positon is not found for more than 10 times, stop the while cycle
}

void recharge(int sockfd)
{

    CHECK(write(sockfd, &landed, sizeof(drone_position))); // dummy command for idle status

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
    direction = !direction; // once the battery is fully charged, change exploration direction
    mvaddstr(40, 50, "               ");
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
        mvaddch(40, i+1, '#');
        attron(COLOR_PAIR(3));
    }

    for (int j = 0; j < PROG - num_chars - 1; j++)
    {
        mvaddch(40, i+j+1, ' ');
    }

    attron(COLOR_PAIR(4));
    sprintf(str,"] %d %% BATTERY  ", percent / (buf_size / 100));
    mvaddstr(40, 31, str);
    attroff(COLOR_PAIR(4));

    refresh();
}

void setup_colors() {
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_WHITE, COLOR_BLACK);
}

/* MAIN */

int main()
{

    int ret; // this is the select() system call return value
    // char str[80];              // String to print on log file.

    int sockfd;                             // File descriptors.
    int clilen, data;                       // Length of client address and variable for the read data.
    int portno = 8080;                      // Used port number.
    struct sockaddr_in serv_addr, cli_addr; // Address of the server and address of the client.

    // Initial Position
    actual_position.x = (int)round(float_rand(0, MAX_X-1));
    actual_position.y = (int)round(float_rand(0, MAX_Y-1));

    // Dummy Position
    landed.x = MAX_X * 2;
    landed.y = MAX_Y * 2;

    // Battery fully charged
    battery = MAX_CHARGE;

    // log_file = fopen("../log_file/Log.txt", "a"); // Open the log file.
    // logPrint("motor_x   : Motor x started.\n");

    /* Signals that the process can receive. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sa.sa_flags = SA_RESTART;
    
    /* sigaction for SIGWINCH */
    CHECK(sigaction(SIGWINCH,&sa,NULL));

    /* Opens socket */

    sockfd = CHECK(socket(AF_INET, SOCK_STREAM, 0)); // Creates a new socket.

    /* Initialize the serv_addr struct. */
    bzero((char *)&serv_addr, sizeof(serv_addr)); // Sets all arguments of serv_addr to zero.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    CHECK(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))); // Listens on the socket for connections. (struct sockaddr *)&serv_addr

    /* Screen setup. */
    initscr(); // Init the console screen.
    refresh();
    clear();

    if (has_colors()) {
        setup_colors();
    }

    while (1)
    {

        // sprintf(str, "motor_x   : command received = %d.\n", command);
        // logPrint(str);

        if (battery == 0)
        { // Low battery
            attron(COLOR_PAIR(2));
            mvaddstr(43, 0, "Low battery, landing for recharging.");
            attroff(COLOR_PAIR(2));
            refresh();

            recharge(sockfd); // send dummy position

            mvaddstr(43, 0, "                                    ");
            refresh();
        }

        compute_next_position(); // looks for a new position

        sprintf(str, "Next X = %d, Next Y = %d \n", next_position.x, next_position.y);
        mvaddstr(42, 0, str);
        refresh();

        CHECK(write(sockfd, &next_position, sizeof(drone_position))); // Writes the next position.

        mvaddstr(44, 0, "Data correctly written into the socket");
        refresh();

        CHECK(read(sockfd, &command, sizeof(int))); // Reads a feedback. ~ command = 0 if next_position is not allowed. ~ command = 1 if next posotion is allowed

        mvaddstr(45, 0, "Feedback correctly read from master process \n" RESET);
        refresh();

        // Not allowed
        if (command == 0)
        {
            mvaddstr(47, 0, "Drone stopped, position not allowed");
            refresh();
        }

        // Allowed
        if (command == 1)
        {
            mvaddstr(46, 0, "Position allowed, drone is moving   ");
            refresh();
            actual_position = next_position;
            map[actual_position.y][actual_position.x] = true;
        }

        for (int i = 0; i < 40; i++)
        {
            for (int j = 0; j < 80; j++)
            {
                if (map[i][j] == 0) // not visited positions
                {
                    attron(COLOR_PAIR(1));
                    mvaddch(i, j, '0');
                    attroff(COLOR_PAIR(1));
                }
                else // visited positions
                {
                    attron(COLOR_PAIR(2));
                    mvaddch(i, j, '1');
                    attroff(COLOR_PAIR(2));
                }
            }
        }
        refresh();

        // sprintf(str, "motor_x   : x_position = %f\n", x_position);
        // logPrint(str);

        // Battery decreases
        battery--;
        loading_bar(battery, MAX_CHARGE); // graphical tool that represents a decharging bar

        // sleep for 0.1 second
        usleep(100000);

    } // End of the while cycle.

    // fclose(log_file); // Close log file.

    return 0;
}
