// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

#define FLAG 0x7E
#define A_fromT 0x03
#define A_fromR 0x01
#define C_SET 0x03
#define C_UA 0x07

volatile int STOP = FALSE;

typedef enum{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    DATA
} receiverState;

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 30; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    receiverState recState = START;
    int tries = 0;

    // Loop for input
    unsigned char buf[8*BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    unsigned char readChar = 0, BCC2 = 0x00;
    int dataCount = 0;

    while (STOP == FALSE && read(fd, &readChar, 1) > 0){
        // Returns after 5 chars have been input
        //int bytes = read(fd, buf, BUF_SIZE);
        //buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

        printf("Received 0x%02X\n", readChar);
        /*for(int i = 0; i < 5; i++){
            printf("buf[%d] = 0x%02X\n", i, buf[i]);
        }*/
        //printf(":%s:%d\n", buf, bytes);

        switch(recState){
            case START:
                printf("START\n");
                if(readChar == FLAG)
                    recState = FLAG_RCV;
                break;

            case FLAG_RCV:
                printf("FLAG_RCV\n");
                if(readChar == A_fromT)
                    recState = A_RCV;
                else if (readChar != FLAG)
                    recState = START;
                break;

            case A_RCV:
                printf("A_RCV\n");
                if(readChar == C_SET)
                    recState = C_RCV;
                else if(readChar == FLAG)
                    recState = FLAG_RCV;
                else
                    recState = START;
                break;

            case C_RCV:
                printf("C_RCV\n");
                if(readChar == A_fromT ^ C_SET)
                    recState = DATA;
                else if(readChar == FLAG)
                    recState = FLAG_RCV;
                else
                    recState = START;
                break;

            case DATA:
                if(readChar == FLAG){
                    printf("END FLAG\n");
                    buf[0] = buf[4] = readChar;
                    // Checking BCC2
                    printf("Checking BCC2 - %d\n", BCC2);
                    if(!BCC2){
                        printf("BCC2 Check\n");
                        STOP = TRUE;
                    }
                    else
                        printf("Bad BCC2\n");
                        //recState = START;
                        STOP = TRUE;
                }
                else{
                    printf("DATA %d\n", dataCount);
                    BCC2 ^= readChar;
                    dataCount++;
                }
                break;
        }
    }
    printf("STOP\n");

    // UA
    buf[1] = A_fromT;
    buf[2] = C_UA;
    buf[3] = buf[1] ^ buf[2];
    buf[5] = '\0';

    printf("Acknowledging\n");
    for(int i = 0; i < 5; i++){
        printf("buf[%d] = 0x%02X\n", i, buf[i]);
    }

    printf("%d bytes written back\n", (int) write(fd, buf, BUF_SIZE));

    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide



    sleep(1);
    
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}