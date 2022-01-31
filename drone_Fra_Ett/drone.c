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

#define MAX_X 80 // max x value
#define MAX_Y 40 // max y value
#define MAX_CHARGE 100

#define RED '\033[1;31m'
#define GREEN '\033[1;32m'
#define BLUE '\033[1;34m'
#define NC '\033[0m'

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

/* GLOBAL VARIABLES */

int command = 0; // Command received.
// FILE *log_file;              // Log file.
drone_position actual_position; // actual drone position
drone_position next_position;   // next drone position
drone_position landed;          // position for idle status
int battery;

/* FUNCTIONS HEADERS */
float float_rand(float min, float max);
// void logPrint ( char * string );
void compute_next_position();
void recharge(int sockfd);

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

void compute_next_position()
{
    /* Function to compute a new position. A better coverage algorithm will be implemented */

    next_position.x = (int)round(actual_position.x + float_rand(-1, 1));
    next_position.y = (int)round(actual_position.x + float_rand(-1, 1));
}

void recharge(int sockfd)
{

    CHECK(write(sockfd, &landed, sizeof(drone_position))); // Dummy command.

    sleep(5); // recharging time

    battery = MAX_CHARGE;
    printf("Battery fully charged. \n");
}

/* MAIN */

int main()
{

    printf("start \n");
    fflush(stdout);

    int ret; // this is the select() system call return value
    // char str[80];              // String to print on log file.

    int sockfd;                             // File descriptors.
    int clilen, data;                       // Length of client address and variable for the read data.
    int portno = 8080;                      // Used port number.
    struct sockaddr_in serv_addr, cli_addr; // Address of the server and address of the client.

    // Initial Position
    actual_position.x = 10;
    actual_position.y = 10;

    // Dummy Position
    landed.x = MAX_X * 2;
    landed.y = MAX_Y * 2;

    // Battery fully charged
    battery = MAX_CHARGE;

    // log_file = fopen("../log_file/Log.txt", "a"); // Open the log file.
    // logPrint("motor_x   : Motor x started.\n");

    /* Opens socket */

    sockfd = CHECK(socket(AF_INET, SOCK_STREAM, 0)); // Creates a new socket.

    printf("create socket \n");
    fflush(stdout);

    /* Initialize the serv_addr struct. */
    bzero((char *)&serv_addr, sizeof(serv_addr)); // Sets all arguments of serv_addr to zero.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    CHECK(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))); // Listens on the socket for connections. (struct sockaddr *)&serv_addr

    printf("connected \n");
    fflush(stdout);

    while (1)
    {

        // sprintf(str, "motor_x   : command received = %d.\n", command);
        // logPrint(str);

        if (battery == 0)
        { // Low battery
            printf("Low battery, landing for recharging. \n");
            fflush(stdout);

            recharge(sockfd);
        }

        compute_next_position();
        printf("next_x = %d, next_y =%d \n", next_position.x, next_position.y);
        fflush(stdout);

        CHECK(write(sockfd, &next_position, sizeof(drone_position))); // Writes the next position.

        printf("written \n");
        fflush(stdout);

        CHECK(read(sockfd, &command, sizeof(int))); // Reads a feedback. ~ command = 0 if next_position is not allowed. ~ command = 1 if next posotion is allowed

        printf("read \n");
        fflush(stdout);

        // Not allowed
        if (command == 0)
        {
            printf("stay \n");
            fflush(stdout);
        }

        // Allowed
        if (command == 1)
        {
            printf("let's move \n");
            fflush(stdout);
            actual_position = next_position;
        }

        // sprintf(str, "motor_x   : x_position = %f\n", x_position);
        // logPrint(str);

        // Battery decreases
        battery--;

        // sleep for 1 second
        sleep(1);

    } // End of the while cycle.

    // fclose(log_file); // Close log file.

    return 0;
}
