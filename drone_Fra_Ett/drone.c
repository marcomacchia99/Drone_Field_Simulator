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

#define MAX_X 80     //max x value
#define MAX_Y 40     //max y value
#define MAX_CHARGE 100

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

/* Defining CHECK() tool. By using this method the code results ligher and cleaner */
#define CHECK(X) ({int __val = (X); (__val == -1 ? ({fprintf(stderr,"ERROR (" __FILE__ ":%d) -- %s\n",__LINE__,strerror(errno)); exit(-1);-1;}) : __val); })

/* GLOBAL VARIABLES */
int command = 0;                                // Command received.
// FILE *log_file;                                 // Log file.
drone_position actual_position;
drone_position next_position;
drone_position landed;
int battery;

/* FUNCTIONS HEADERS */
float float_rand( float min, float max );
// void logPrint ( char * string );
void compute_next_position();
void recharge( int newsockfd );

/* FUNCTIONS */
float float_rand( float min, float max ) {
    /* Function to generate a randomic error. */

    float scale = rand() / (float)RAND_MAX;
    return min + scale * (max - min); // [min, max]
}

// void logPrint ( char * string ) {
//     /* Function to print on log file adding time stamps. */

//     time_t ltime = time(NULL);
//     fprintf( log_file, "%.19s: %s", ctime( &ltime ), string );
//     fflush(log_file);
// }

void compute_next_position() {
    next_position.x = (int)round(actual_position.x + float_rand(-1, 1));
    next_position.y = (int)round(actual_position.x + float_rand(-1, 1));
}

void recharge( int newsockfd ) {

    CHECK(write(newsockfd, &landed, sizeof(drone_position)));   // Writes command.

    sleep(5);

    battery = MAX_CHARGE;
    printf("Battery fully charged. \n");
}

/* MAIN */

int main() {

    printf("start \n");
    fflush(stdout);

    int ret;                   //select() system call return value
    char str[80];              // String to print on log file.

    int sockfd, newsockfd;                    // File descriptors.
    int clilen, data;                         // Length of client address and variable for the read data.
    int portno;                               // Used port number for socket connection.
    struct sockaddr_in serv_addr, cli_addr;   // Address of the server and address of the client.

    actual_position.x = 10;
    actual_position.y = 10;
    landed.x = MAX_X*2;
    landed.y = MAX_Y*2;
    battery = MAX_CHARGE;

    // log_file = fopen("../log_file/Log.txt", "a"); // Open the log file.

    // logPrint("motor_x   : Motor x started.\n");

    /* Opens socket */

    sockfd = CHECK(socket(AF_INET, SOCK_STREAM, 0));   // Creates a new socket.

    printf("create socket \n");
    fflush(stdout);

    /* Initialize the serv_addr struct. */
    bzero((char *)&serv_addr, sizeof(serv_addr));      // Sets all arguments of serv_addr to zero.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    CHECK(connect(sockfd, &serv_addr, sizeof(serv_addr)));   // Listens on the socket for connections.

    printf("connected \n");
    fflush(stdout);

    // socklen_t len = sizeof(serv_addr);

    // CHECK(getsockname(sockfd, (struct sockaddr *)&serv_addr, &len));   // Gets the port number chosen by the OS.

    /* Prints on the terminal and on the log file the used port. */
    // printf("Used port number: %d\n", ntohs(serv_addr.sin_port));
    // sprintf(str, "Consumer Socket    :Used port number: %d \n", ntohs(serv_addr.sin_port));
    // logPrint(str);

    // portno = htons(serv_addr.sin_port);   // Stores the port number in network byte order.

    while (1) {

        // sprintf(str, "motor_x   : command received = %d.\n", command);
        // logPrint(str);

        if (battery == 0) {
            printf("Low battery, landing for recharging. \n");
            fflush(stdout);

            recharge(newsockfd);
        }

        compute_next_position();
        printf("next_x = %d, next_y =%d \n", next_position.x, next_position.y);
        fflush(stdout);

        CHECK(write(newsockfd, &next_position, sizeof(drone_position)));   // Writes command.

        printf("written \n");
        fflush(stdout);

        CHECK(read(newsockfd, &command, sizeof(int)));   // Reads command.

        printf("red \n");
        fflush(stdout);
            
        if (command == 0) {
            printf("stay \n");
            fflush(stdout);
        }
            
        if (command == 1) {
            printf("let's move \n");
            fflush(stdout);
            actual_position = next_position;
        }

        // est_pos_x = x_position + float_rand(-0.005, 0.005); //compute the estimated position

        // sprintf(str, "motor_x   : x_position = %f\n", x_position);
        // logPrint(str);

        /* Sleeps. If the command does not change, repeats again the same command. */
        battery--;
        sleep(1); //sleep for 1 second

    } // End of the while cycle.

    // logPrint("motor_x   : Motor x ended.\n");

    // fclose(log_file); // Close log file.

    return 0;
}
