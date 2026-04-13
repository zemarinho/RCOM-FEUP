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
#include <time.h>
#include <stdint.h>
//#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256
#define PACK_SIZE 32
#define PACK_HOLDER_SIZE 32
#define MAX_FILE_SIZE 1000000

#define FLAG 0x7E
#define MY_ADRESS 0x03
#define NOT_MY_ADRESS 0x01
#define C_SET 0x03
#define C_UA 0x07
#define ESC 0x7D
#define MAX_RETRIES 5

typedef enum
{
    SIGA,
    FLAG_RCV,
    ADRESS,
    CONTROL,
    LER_PAYLOAD,
    PAROU_CARAI
    
} State;

typedef struct {
    uint64_t bytes_total;      // tudo (headers + payload)
    uint64_t bytes_payload;    // só dados úteis
    uint64_t packets_sent;
    // uint64_t packets_received;
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
    return 0;

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
    newtio.c_cc[VTIME] = 60;
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
    printf("SET sent\n");

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

    printf("UA received\n");
    printf("llopen: link established\n");
    return fd;
}


int llwrite(int fd, const unsigned char *payload, int payloadLen, int seq)
{
    
    unsigned char buf[PACK_HOLDER_SIZE * PACK_SIZE * 2 + 20] = {0};
    int insertionPos = 0;
    unsigned char pseudoBuf = 0;
    
    unsigned char ctrl = seq & 1;
    buf[insertionPos++] = FLAG;
    buf[insertionPos++] = MY_ADRESS;
    buf[insertionPos++] = ctrl;
    buf[insertionPos++] = MY_ADRESS ^ ctrl;
    
    unsigned char bcc2 = 0;
    for (int i = 0; i < payloadLen; i++)
    {
        unsigned char byte = payload[i];
        bcc2 ^= byte;
        if (byte == FLAG || byte == ESC)
        {
            buf[insertionPos++] = ESC;
            buf[insertionPos++] = byte ^ 0x20;
        }
        else
        {
            buf[insertionPos++] = byte;
        }
    }
    
    if (bcc2 == FLAG || bcc2 == ESC)
    {
        buf[insertionPos++] = ESC;
        buf[insertionPos++] = bcc2 ^ 0x20;
    }
    else
    {
        buf[insertionPos++] = bcc2;
    }
    
    buf[insertionPos++] = FLAG;
    
    m.packets_sent++;
    m.bytes_total += insertionPos;
    m.bytes_payload += payloadLen;

    int bytes = write(fd, buf, insertionPos);
    if (bytes < 0)
    {
        perror("write");
        return -1;
    }

    printf("packet sent seq=%d len=%d\n", ctrl, payloadLen);
    unsigned char expectedAck = C_UA ^ ctrl;
    State state = SIGA;
    int gotAck = 0;

    while (!gotAck)
    {
        int count = read(fd, &pseudoBuf, 1);
        if (count < 0)
            return -1;
        if (count == 0)
        {
            printf("waiting for ACK seq=%d\n", ctrl);
            return -1;
        }

        switch (state)
        {
            case SIGA:
                if (pseudoBuf == FLAG)
                    state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (pseudoBuf == NOT_MY_ADRESS)
                    state = ADRESS;
                else if (pseudoBuf == FLAG)
                    state = FLAG_RCV;
                else
                    state = SIGA;
                break;
            case ADRESS:
                if (pseudoBuf == expectedAck)
                    state = CONTROL;
                else if (pseudoBuf == FLAG)
                    state = FLAG_RCV;
                else
                    state = SIGA;
                break;
            case CONTROL:
                if (pseudoBuf == (NOT_MY_ADRESS ^ expectedAck))
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

    printf("ACK received seq=%d\n", ctrl);
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
    
    //************************
    if (argc < 3)
    {
        printf("Incorrect program usage\n"
            "Usage: %s <SerialPort> <ImageFile>\n"
            "Example: %s /dev/ttyS1 image.gif\n",
            argv[0],
            argv[0]);
            return 1;
        }
        
        struct termios oldtio;
        int fd = llopen(argv[1], &oldtio);
        if (fd < 0)
            return 1;
        double t_start = now_sec();

    // Read the entire .gif file
    FILE *fp = fopen(argv[2], "rb");
    if (!fp)
    {
        perror("fopen");
        close(fd);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    rewind(fp);
    if (fileSize > MAX_FILE_SIZE)
    {
        fileSize = MAX_FILE_SIZE;  // Truncate if too large
    }
    char fileData[MAX_FILE_SIZE];
    fread(fileData, 1, fileSize, fp);
    fclose(fp);

    // Send file size first
    int seq = 0;
    int attempts = 0;
    while (attempts < MAX_RETRIES)
    {
        printf("sending size packet seq=%d\n", seq);
        if (llwrite(fd, (unsigned char *)&fileSize, sizeof(long), seq) == 0)
            break;
        attempts++;
        printf("retry size packet seq=%d attempt=%d\n", seq, attempts + 1);
        sleep(1);
    }
    if (attempts == MAX_RETRIES)
    {
        fprintf(stderr, "llwrite failed for size packet\n");
        close(fd);
        return 1;
    }

    // Send in chunks
    int chunkSize = PACK_HOLDER_SIZE * PACK_SIZE;  // 4096 bytes of raw data
    int numChunks = (fileSize + chunkSize - 1) / chunkSize;
    int chunkchunk = 0;

    for (int chunk = 0; chunk < numChunks; chunk++)
    {
        int start = chunk * chunkSize;
        int toSend = (fileSize - start) > chunkSize ? chunkSize : (fileSize - start);
        seq ^= 1;
        attempts = 0;
        while (attempts < MAX_RETRIES)
        {
            printf("sending chunk %d seq=%d len=%d\n", chunk, seq, toSend);
            if (llwrite(fd, (unsigned char *)fileData + start, toSend, seq) == 0)
            {
                //////////////////////Progress Bar
                chunkchunk += toSend;
                break;
            }
            attempts++;
            printf("retry chunk %d seq=%d attempt=%d\n", chunk, seq, attempts + 1);
            sleep(1);
        }
        if (attempts == MAX_RETRIES)
        {
            fprintf(stderr, "llwrite failed for chunk %d\n", chunk);
            close(fd);
            return 1;
        }
        printf("[");
        for (int i = 0; i<chunkchunk/ (double)fileSize * 50; i++)
        {
            printf("X");
        }
        for (int i = 0; i<50-(chunkchunk/ (double)fileSize * 50); i++)
        {
            printf("_");
        }
        printf("]");
        printf("progresso: %.2f%%\n",  chunkchunk/ (double)fileSize * 100);

    }
    double t_end = now_sec();
    if (llclose(fd, &oldtio) != 0)
        fprintf(stderr, "llclose failed\n");

    //*******************

    m.total_time = t_end - t_start;

    double throughput = m.bytes_total / m.total_time;
    double goodput   = m.bytes_payload / m.total_time;
    // double avg_lat   = m.total_latency / m.latency_samples;
    double efficiency = (double)m.bytes_payload / m.bytes_total;

    printf("STATS---------------------------------------------\n");
    printf("Time: %.2fs\n", m.total_time);
    printf("Throughput: %.2f B/s\n", throughput);
    printf("Goodput: %.2f B/s\n", goodput);
    // printf("Latência média: %.6f s\n", avg_lat);
    printf("Eficiência: %.2f\n", efficiency);
    // printf("Retransmissões: %lu\n", m.retransmissions);
    return 0;
}
