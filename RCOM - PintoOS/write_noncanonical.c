// Write to serial port in non-canonical mode
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
//#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256
#define PACK_SIZE 64
#define PACK_HOLDER_SIZE 64

#define FLAG 0x7E
#define MY_ADRESS 0x03
#define NOT_MY_ADRESS 0x01
#define C_SET 0x03
#define C_UA 0x07
#define ESC 0x7D

typedef enum
{
    SIGA,
    FLAG_RCV,
    ADRESS,
    CONTROL,
    LER_PAYLOAD,
    PAROU_CARAI
    
} State;

volatile int STOP = FALSE;

int     //return 1 se lido, 0 se não leu nada
readRead(int fd, unsigned char *buf)
{
    int alarmCount = 0;
    while (alarmCount < 3)
    {
        //printf("test clock\n");
        int bytes;
        
        bytes = read(fd, buf, 1);
        if(bytes == 0){
            alarmCount++;
            printf("não a panhei\n");
            //printf("Alarmcount: %d\n", alarmCount);
            return 1;
        }
        else{
            printf("a panhei %d bytes\n", bytes);
            // for(int i = 0; i < 5; i++){
            //     printf("buf2[%d] = 0x%02X\n", i, buf[i]);
            // }
            return 0;
        }
    }
    STOP = TRUE;

}

unsigned  int calcular_xor(const unsigned char a[], unsigned int xor, int size) {

    for(int i = 0; i < size; i++)
        xor^=a[i];
    return xor;
}

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

    unsigned char setFrame[5] = {FLAG, MY_ADRESS, C_SET, (MY_ADRESS ^ C_SET), FLAG};
    if (write(fd, setFrame, 5) != 5)
    {
        perror("write SET");
        close(fd);
        return -1;
    }

    State state = SIGA;
    unsigned char c;
    int gotUA = 0;

    while (!gotUA && read(fd, &c, 1) > 0)
    {
        switch (state)
        {
            case SIGA:
                if (c == FLAG)
                    state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (c == NOT_MY_ADRESS)
                    state = ADRESS;
                else if (c == FLAG)
                    state = FLAG_RCV;
                else
                    state = SIGA;
                break;
            case ADRESS:
                if (c == C_UA)
                    state = CONTROL;
                else if (c == FLAG)
                    state = FLAG_RCV;
                else
                    state = SIGA;
                break;
            case CONTROL:
                if (c == (NOT_MY_ADRESS ^ C_UA))
                    gotUA = 1;
                else if (c == FLAG)
                    state = FLAG_RCV;
                else
                    state = SIGA;
                break;
            default:
                state = SIGA;
                break;
        }
    }

    if (!gotUA)
    {
        fprintf(stderr, "llopen: UA not received\n");
        close(fd);
        return -1;
    }

    printf("llopen: link established\n");
    return fd;
}

int llwrite(int fd, const char packs[PACK_HOLDER_SIZE][2 * PACK_SIZE])
{
    unsigned char buf[8 * BUF_SIZE + 1] = {0};
    unsigned char pseudoBuf = 0;

    int insertionPos = 4;
    int currenteSize = 5;

    buf[0] = FLAG;
    buf[1] = MY_ADRESS;
    buf[2] = MY_ADRESS;
    buf[3] = buf[1] ^ buf[2];
    buf[4] = FLAG;

    // unsigned char packs[PACK_HOLDER_SIZE][2 * PACK_SIZE] = {
    //     {0xA1, 0xA2, 0xA3, ESC, FLAG, FLAG, 0xA7, FLAG, 0, 0, 0, 0, 0, 0, 0, 0},
    //     {0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0},
    //     {0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0, 0, 0, 0, 0, 0, 0, 0},
    //     {0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0, 0, 0, 0, 0, 0, 0, 0},
    //     {0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0, 0, 0, 0, 0, 0, 0, 0}
    // };

    unsigned char packSizes[PACK_HOLDER_SIZE] = {0};
    unsigned int xor = 0;

    for (int i = 0; i < PACK_HOLDER_SIZE; i++)
    {
        xor = calcular_xor(packs[i], xor, 2 * PACK_SIZE);

        int lastId = PACK_SIZE - 1;
        for (int j = 0; j < 2 * PACK_SIZE;)
        {
            if (packs[i][j] == FLAG || packs[i][j] == ESC)
            {
                memmove(&packs[i][j + 2], &packs[i][j + 1], lastId - j);
                if (packs[i][j] == FLAG)
                {
                    packs[i][j] = ESC;
                    packs[i][j + 1] = 0x5E;
                }
                else
                {
                    packs[i][j + 1] = 0x5D;
                }
                lastId++;
                j += 2;
            }
            else
            {
                j++;
            }
        }

        packSizes[i] = lastId + 1;
        memmove(&buf[insertionPos + packSizes[i]], &buf[insertionPos], currenteSize - insertionPos);
        memcpy(&buf[insertionPos], packs[i], packSizes[i]);
        insertionPos += packSizes[i];
        currenteSize += packSizes[i];

        if (i == PACK_HOLDER_SIZE - 1)
        {
            memmove(&buf[insertionPos + 1], &buf[insertionPos], currenteSize - insertionPos);
            memcpy(&buf[insertionPos], &xor, 1);
            insertionPos += 1;
            currenteSize += 1;
        }
    }

    for (int i = 0; i < currenteSize; i++)
        printf("buf[%d] = 0x%02X\n", i, buf[i]);

    int bytes = write(fd, buf, currenteSize);
    printf("%d bytes written\n", bytes);

    State state = SIGA;
    int gotAck = 0;

    while (!gotAck && read(fd, &pseudoBuf, 1) > 0)
    {
        switch (state)
        {
            case SIGA:
                if (pseudoBuf == FLAG)
                    state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (pseudoBuf == MY_ADRESS)
                    state = ADRESS;
                else if (pseudoBuf == FLAG)
                    state = FLAG_RCV;
                else
                    state = SIGA;
                break;
            case ADRESS:
                if (pseudoBuf == C_UA)
                    state = CONTROL;
                else if (pseudoBuf == FLAG)
                    state = FLAG_RCV;
                else
                    state = SIGA;
                break;
            case CONTROL:
                if (pseudoBuf == (MY_ADRESS ^ C_UA))
                    gotAck = 1;
                else if (pseudoBuf == FLAG)
                    state = FLAG_RCV;
                else
                    state = SIGA;
                break;
            default:
                state = SIGA;
                break;
        }
    }

    if (!gotAck)
    {
        fprintf(stderr, "llwrite: ACK not received\n");
        return -1;
    }

    printf("llwrite: ACK received\n");
    return 0;
}

int llclose(int fd, struct termios *oldtio)
{
    if (tcsetattr(fd, TCSANOW, oldtio) == -1)
    {
        perror("tcsetattr");
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
    if (fd < 0)
        return 1;

    if (llwrite(fd, ) != 0)
        fprintf(stderr, "llwrite failed\n");

    if (llclose(fd, &oldtio) != 0)
        fprintf(stderr, "llclose failed\n");

    return 0;
}
