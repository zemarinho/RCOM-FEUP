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
#define ESC 0x7D

volatile int STOP = FALSE;

typedef enum{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    DATA
} receiverState;

int llopen(const char *serialPortName, struct termios *oldtio)
{
    if (serialPortName == NULL || oldtio == NULL)
        return -1;

    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        return -1;
    }

    struct termios newtio;

    if (tcgetattr(fd, oldtio) == -1)
    {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 30;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    receiverState recState = START;
    unsigned char readChar;
    int gotSet = 0;

    while (!gotSet && read(fd, &readChar, 1) > 0)
    {
        switch (recState)
        {
            case START:
                if (readChar == FLAG)
                    recState = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (readChar == A_fromT)
                    recState = A_RCV;
                else if (readChar == FLAG)
                    recState = FLAG_RCV;
                else
                    recState = START;
                break;
            case A_RCV:
                if (readChar == C_SET)
                    recState = C_RCV;
                else if (readChar == FLAG)
                    recState = FLAG_RCV;
                else
                    recState = START;
                break;
            case C_RCV:
                if (readChar == (A_fromT ^ C_SET))
                    recState = DATA;
                else if (readChar == FLAG)
                    recState = FLAG_RCV;
                else
                    recState = START;
                break;
            case DATA:
                if (readChar == FLAG)
                {
                    gotSet = 1;
                }
                else
                {
                    recState = START;
                }
                break;
        }
    }

    if (!gotSet)
    {
        fprintf(stderr, "llopen: SET not received\n");
        close(fd);
        return -1;
    }

    unsigned char uaFrame[5] = {FLAG, A_fromR, C_UA, (A_fromR ^ C_UA), FLAG};
    if (write(fd, uaFrame, 5) != 5)
    {
        perror("write UA");
        close(fd);
        return -1;
    }

    printf("llopen: link established (SET received, UA sent)\n");
    return fd;
}

int llread(int fd, unsigned char *dataBuffer)
{
    receiverState recState = START;
    unsigned char readChar = 0;
    unsigned char BCC2 = 0x00;
    int dataCount = 0;
    int esc = 0;

    memset(dataBuffer, 0, 8*BUF_SIZE + 1);

    while (STOP == FALSE && read(fd, &readChar, 1) > 0)
    {
        printf("Received 0x%02X\n", readChar);

        switch (recState)
        {
            case START:
                if (readChar == FLAG)
                    recState = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (readChar == A_fromT)
                    recState = A_RCV;
                else if (readChar != FLAG)
                    recState = START;
                break;
            case A_RCV:
                if (readChar == C_SET)
                    recState = C_RCV;
                else if (readChar == FLAG)
                    recState = FLAG_RCV;
                else
                    recState = START;
                break;
            case C_RCV:
                if (readChar == (A_fromT ^ C_SET))
                    recState = DATA;
                else if (readChar == FLAG)
                    recState = FLAG_RCV;
                else
                    recState = START;
                break;
            case DATA:
                if (readChar == FLAG)
                {
                    printf("END FLAG\n");
                    if (!BCC2)
                    {
                        printf("BCC2 Check OK\n");
                        STOP = TRUE;
                    }
                    else
                    {
                        printf("Bad BCC2\n");
                        STOP = TRUE;
                    }
                }
                else
                {
                    printf("DATA %d\n", dataCount);
                    if (readChar == ESC)
                    {
                        esc++;
                    }
                    else
                    {
                        if (esc)
                        {
                            dataBuffer[dataCount] = readChar ^ 0x20;
                            esc = 0;
                        }
                        else
                        {
                            dataBuffer[dataCount] = readChar;
                        }
                        BCC2 ^= dataBuffer[dataCount];
                        dataCount++;
                    }
                }
                break;
        }
    }

    printf("DATA RECEIVED:\n");
    for (int i = 0; i < dataCount; i++)
    {
        printf("0x%02X\n", dataBuffer[i]);
    }

    unsigned char ackFrame[5] = {FLAG, A_fromR, C_UA, (A_fromR ^ C_UA), FLAG};
    if (write(fd, ackFrame, 5) != 5)
    {
        perror("write UA");
        return -1;
    }

    printf("llread: UA sent as acknowledgement\n");
    return 0;
}

int llclose(int fd, struct termios *oldtio)
{
    if (tcsetattr(fd, TCSANOW, oldtio) == -1)
    {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    close(fd);
    printf("llclose: link closed\n");
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        return 1;
    }

    struct termios oldtio;
    int fd = llopen(argv[1], &oldtio);
    unsigned char dataBuffer[8*BUF_SIZE + 1];
    if (fd < 0)
        return 1;

    if (llread(fd, dataBuffer) != 0)
        fprintf(stderr, "llread failed\n");

    if (llclose(fd, &oldtio) != 0)
        fprintf(stderr, "llclose failed\n");

    return 0;
}