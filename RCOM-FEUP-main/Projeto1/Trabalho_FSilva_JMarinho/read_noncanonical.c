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
#include <time.h>
#include <stdint.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define LINK_SPEED 38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 262144
#define MAX_FILE_SIZE 100000
#define MAX_IDLE_READS 10

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

typedef struct {
    uint64_t bytes_total;      // tudo (headers + payload)
    uint64_t bytes_payload;    // só dados úteis
    // uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t retransmissions;

    double total_time;         // tempo total medido
    double total_latency;      // soma das latências
    uint64_t latency_samples;  // nº amostras
} metrics_t;

metrics_t m = {0};

double now_sec() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
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
    unsigned char address = 0;
    unsigned char ctrl = 0;
    unsigned char bcc1 = 0;
    unsigned char frameData[8 * BUF_SIZE + 1];
    int frameLen = 0;
    int esc = 0;
    int idleCount = 0;
    static int expectedSeq = 0;

    memset(dataBuffer, 0, 8*BUF_SIZE + 1);
    frameLen = 0;
    esc = 0;

    while (1)
    {
        int count = read(fd, &readChar, 1);
        if (count < 0)
            return -1;
        if (count == 0)
        {
            if (++idleCount >= MAX_IDLE_READS)
                return -1;
            continue;
        }
        idleCount = 0;

        switch (recState)
        {
            case START:
                if (readChar == FLAG)
                    recState = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (readChar == A_fromT)
                {
                    address = readChar;
                    recState = A_RCV;
                }
                else if (readChar == FLAG)
                {
                    recState = FLAG_RCV;
                }
                else
                {
                    recState = START;
                }
                break;
            case A_RCV:
                ctrl = readChar;
                recState = C_RCV;
                break;
            case C_RCV:
                bcc1 = readChar;
                if (bcc1 == (address ^ ctrl))
                {
                    recState = DATA;
                    frameLen = 0;
                    esc = 0;
                }
                else if (readChar == FLAG)
                {
                    recState = FLAG_RCV;
                }
                else
                {
                    recState = START;
                }
                break;
            case DATA:
                if (readChar == ESC)
                {
                    esc = 1;
                }
                else if (readChar == FLAG && !esc)
                {
                    if (frameLen < 1)
                    {
                        recState = START;
                        break;
                    }

                    unsigned char receivedBCC2 = frameData[frameLen - 1];
                    unsigned char calcBCC2 = 0;
                    for (int i = 0; i < frameLen - 1; i++)
                        calcBCC2 ^= frameData[i];

                    int seq = ctrl & 1;
                    unsigned char ackCtrl = C_UA ^ seq;
                    unsigned char ackFrame[5] = {FLAG, A_fromR, ackCtrl, (A_fromR ^ ackCtrl), FLAG};

                    if (calcBCC2 != receivedBCC2)
                    {
                        printf("packet bad bcc2 seq=%d\n", seq);
                        ackCtrl = C_UA ^ (expectedSeq ^ 1);
                        unsigned char nackFrame[5] = {FLAG, A_fromR, ackCtrl, (A_fromR ^ ackCtrl), FLAG};
                        if (write(fd, nackFrame, 5) != 5)
                        {
                            perror("write UA");
                            return -1;
                        }
                        printf("NACK sent seq=%d\n", expectedSeq ^ 1);
                        return 0;
                    }

                    if (seq != expectedSeq)
                    {
                        printf("duplicate packet received seq=%d expected=%d\n", seq, expectedSeq);
                        unsigned char duplicateAck[5] = {FLAG, A_fromR, ackCtrl, (A_fromR ^ ackCtrl), FLAG};
                        if (write(fd, duplicateAck, 5) != 5)
                        {
                            perror("write UA");
                            return -1;
                        }
                        printf("duplicate ACK sent seq=%d\n", seq);
                        return 0;
                    }

                    if (write(fd, ackFrame, 5) != 5)
                    {
                        perror("write UA");
                            return -1;
                    }
                    printf("ACK sent seq=%d\n", seq);

                    int payloadLen = frameLen - 1;
                    printf("packet received seq=%d len=%d\n", seq, payloadLen);
                    memcpy(dataBuffer, frameData, payloadLen);
                    expectedSeq ^= 1;

                    m.packets_received++;
                    m.bytes_total += frameLen;
                    m.bytes_payload += payloadLen;

                    return payloadLen;
                }
                else
                {
                    if (esc)
                    {
                        readChar ^= 0x20;
                        esc = 0;
                    }
                    if (frameLen < (int)sizeof(frameData))
                        frameData[frameLen++] = readChar;
                }
                break;
        }

    }

    return -1;
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
    
    //****************
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
    double t_end = 0;
    double t_start = now_sec();
    unsigned char dataBuffer[8*BUF_SIZE + 1];

    // Receive file size first
    long fileSize;
    int sizeBytes = 0;
    do
    {
        sizeBytes = llread(fd, dataBuffer);
        if (sizeBytes < 0)
        {
            fprintf(stderr, "Failed to receive file size\n");
            close(fd);
            return 1;
        }
    } while (sizeBytes == 0);
    if ((size_t)sizeBytes < sizeof(long))
    {
        fprintf(stderr, "Failed to receive file size\n");
        close(fd);
        return 1;
    }
    memcpy(&fileSize, dataBuffer, sizeof(long));
    if (fileSize > MAX_FILE_SIZE)
    {
        fileSize = MAX_FILE_SIZE;
    }
    printf("Expecting file size: %ld bytes\n", fileSize);

    // Receive chunks until total received equals fileSize
    char receivedData[MAX_FILE_SIZE] = {0};
    int totalReceived = 0;
    while (totalReceived < fileSize)
    {
        int bytesRead = llread(fd, dataBuffer);
        if (bytesRead < 0)
        {
            fprintf(stderr, "Error receiving chunk, stopping after %d bytes\n", totalReceived);
            break;
        }
        if (bytesRead == 0)
            continue;
        int toCopy = (fileSize - totalReceived) > bytesRead ? bytesRead : (fileSize - totalReceived);
        memcpy(receivedData + totalReceived, dataBuffer, toCopy);
        totalReceived += toCopy;
        printf("[");
        for (int i = 0; i<totalReceived/ (double)fileSize * 50; i++)
        {
            printf("|");
        }
        for (int i = 0; i<50-(totalReceived/ (double)fileSize * 50); i++)
        {
            printf(" ");
        }
        printf("]");
        printf("progresso: %.2f%%\n",  totalReceived/ (double)fileSize * 100);
    }
    t_end = now_sec();

    printf("Receiving finished, writing %d of %ld bytes to recebido.gif\n", totalReceived, fileSize);
    // Write the received data to a .gif file
    FILE *fp = fopen("recebido.gif", "wb");
    if (fp)
    {
        fwrite(receivedData, 1, totalReceived, fp);
        fclose(fp);
        printf("File written: recebido.gif (%d bytes)\n", totalReceived);
    }
    else
    {
        perror("fopen");
    }

    sleep(1);

    if (llclose(fd, &oldtio) != 0)
        fprintf(stderr, "llclose failed\n");


    //*********************
    m.total_time = t_end - t_start;

    double throughput = m.bytes_total / m.total_time;
    double goodput   = m.bytes_payload / m.total_time;
    // double avg_lat   = m.total_latency / m.latency_samples;
    double efficiency = goodput*8/LINK_SPEED;

    printf("STATS---------------------------------------------\n");
    printf("Time: %.2fs\n", m.total_time);
    printf("Throughput: %.2f B/s\n", throughput);
    printf("Goodput: %.2f B/s\n", goodput);
    // printf("Latência média: %.6f s\n", avg_lat);
    printf("Eficiência: %.2f%%\n", efficiency*100);
    // printf("Retransmissões: %lu\n", m.retransmissions);

    return 0;
}