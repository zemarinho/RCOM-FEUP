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

#define FLAG 0x7E
#define MY_ADRESS 0x03
#define NOT_MY_ADRESS 0x01
#define C_SET 0x03
#define C_UA 0x07

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




unsigned  int calcular_xor(const unsigned char a, unsigned int xor) {

    for(int i = 0; i < a.size(); i++)
        xor^=a[i];
    return xor;
}

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

    // Open serial port device for reading and writing, and not as controlling tty
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

    // Create string to send
    unsigned char buf[8*BUF_SIZE + 1] = {0};
    unsigned char buf2[8*BUF_SIZE + 1] = {0};
    unsigned char pseudoBuf = 0;
    unsigned char packs[31][2*8] = {0};


    /*for(int i = 0; i < BUF_SIZE; i++){
        buf[i] = 'a' + i % 26;
    }*/

    int insertionPos = 4; //posição atual onde se pode inserir novo pacote
    int currenteSize = 5; //número atual de elementos "úteis"

    buf[0] = 0x7E;
    buf[1] = 0x03;
    buf[2] = 0x03;
    buf[3] = buf[1] ^ buf[2];
    buf[4] = 0x7E;
    
    packs[0] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0, 0, 0, 0, 0, 0, 0, 0};
    packs[1] = {0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0};
    packs[2] = {0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0, 0, 0, 0, 0, 0, 0, 0};
    packs[3] = {0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0, 0, 0, 0, 0, 0, 0, 0};
    packs[4] = {0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0, 0, 0, 0, 0, 0, 0, 0};
    
    unsigned int xor;
    for (int i = 0; i<packs.size(); i++)
    {
        if (!i)
        {
            xor = 0;
        }
        memmove(&buf[insertionPos + packs[0].size()], &buf[insertionPos], currenteSize - insertionPos);
        memccpy(&buf[insertionPos], pack[0], pack[0].size());
        insertionPos += pack[0].size();
        currenteSize += pack[0].size();
        xor = calcular_xor(packs[i], xor);
        if (i = packs.size()-1)
        {
            memmove(&buf[insertionPos + 1], &buf[insertionPos], currenteSize - insertionPos);
            memccpy(&buf[insertionPos], xor, 1);
            insertionPos += pack[0].size();
            currenteSize += pack[0].size();
        }
    }





    //for (int i = 0; i< buf)
    for(int i = 0; i < 16; i++){
           printf("buf[%d] = 0x%02X\n", i, buf[i]);
        }
        
        // In non-canonical mode, '\n' does not end the writing.
        // Test this condition by placing a '\n' in the middle of the buffer.
        // The whole buffer must be sent even with the '\n'.
    
    //buf[6] = '\n';
        
    for (int i=0; i< BUF_SIZE +1; i++)
    {
        buf2[i] = buf[i];
    }
    int bytes = write(fd, buf, BUF_SIZE);
    printf("%d bytes written\n", bytes);

    // Wait until all bytes have been written to the serial port
    sleep(1);

    //UA
    /*struct sigaction act = {0};
    act.sa_handler = &alarmHandler;

    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }*/

    State state = SIGA;

    int readCounter = 0;
    while (STOP == FALSE)
    {
        //printf("WHILEEEEEE\n");

        //readRead(fd, buf);
        // Returns after 5 chars have been input    
        printf("analyzing\n");

        buf[bytes] = '\0'; // Set end of string to '\0', so we can printf
        if (readCounter >= 5)
        {
            break;
        }
        readCounter++;
        if (read(fd, &pseudoBuf, 1) > 0)
        {
            //printf("IFFFFFFFF\n");
            readCounter = 0;
            if (readCounter > 0)
                write(fd, buf, BUF_SIZE);
            else
            {
                switch (state)
                {
                    case SIGA:
                        printf("Comecei a ler\n");
                        if (pseudoBuf == FLAG)
                        {
                            printf("Recebi flag inicial: 0x%02X\n", pseudoBuf);
                            state = FLAG_RCV;
                        }
                        break;

                    case FLAG_RCV:
                        if (pseudoBuf == MY_ADRESS)
                        {
                            printf("recebi adress: 0x%02X\n", pseudoBuf);
                            state = ADRESS;
                        }
                        else if ( pseudoBuf == FLAG)
                            break;
                        else
                            state == SIGA;
                        break;

                    case ADRESS:
                        if (pseudoBuf == C_UA)
                        {
                            printf("recebi o controlo: 0x%02X\n", pseudoBuf);
                            state = CONTROL;
                        }
                        else if ( pseudoBuf == FLAG)
                            state = FLAG_RCV;
                        else
                            state == SIGA;

                        break;

                    case CONTROL:
                        if (pseudoBuf == (MY_ADRESS ^ C_UA))
                        {
                            state = LER_PAYLOAD;
                            printf("Ingnorando o Payload: 0x%02X\n", pseudoBuf);
                        }
                        else if ( pseudoBuf == FLAG)
                            state = FLAG_RCV;
                        else
                            state == SIGA;
                        break;

                    case LER_PAYLOAD:
                        if (pseudoBuf == FLAG)
                        {
                            state = PAROU_CARAI;
                        }
                        else
                            state = SIGA;
                        break;

                    case PAROU_CARAI:
                        printf("Parou\n");
                        state == SIGA;
                        STOP = TRUE;
                        printf("Hand mass age\n");
                        //printf("Voltando ao início\n");
                        break;
                }
            }
        }




        //DEBUG
        // for(int i = 0; i < 5; i++){
            // printf("var%d = 0x%02X\n", i, buf[i]);
        // }
        //printf(":%s:%d\n", buf, bytes);
        //printf("var1 = 0x%02X\n", buf[1]);
        //printf("var2 = 0x%02X\n", buf[2]);
        
        //UA
        // if (buf2[1] == 0x01 && buf2[2] == 0x07)
        //     printf("Mass age good recipt!\n");
        // else
        //     printf("bed mass age\n");

        // STOP = TRUE;

        //alarmHandler();


        //if (buf[0] == 'z')
        //    STOP = TRUE;
    }
    printf("Cheguei ao fim\n");
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
