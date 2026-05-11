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

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 

#define FALSE 0
#define TRUE 1

// -----------------------------------------------------------------
// ESTADOS DA MÁQUINA DE ESTADOS
// -----------------------------------------------------------------
typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP_STATE
} State;
// -----------------------------------------------------------------

volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n");
        exit(1);
    }

    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio, newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    
    // ATENÇÃO À CONFIGURAÇÃO PARA A MÁQUINA DE ESTADOS
    newtio.c_cc[VTIME] = 0;   
    newtio.c_cc[VMIN] = 1;  // LER BYTE A BYTE (bloqueia até receber 1 byte)

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // -----------------------------------------------------------------
    // LÓGICA DE RECEÇÃO - MÁQUINA DE ESTADOS PARA A TRAMA SET
    // -----------------------------------------------------------------
    State state = START;
    unsigned char byte_rcv;
    unsigned char A_field = 0x03; // Endereço esperado
    unsigned char C_field = 0x03; // Controlo esperado (SET)

    printf("A aguardar trama SET...\n");

    while (state != STOP_STATE)
    {
        // Lê um byte da porta série
        if (read(fd, &byte_rcv, 1) > 0) 
        {
            switch (state) 
            {
                case START:
                    if (byte_rcv == 0x7E) state = FLAG_RCV;
                    break;
                    
                case FLAG_RCV:
                    if (byte_rcv == A_field) state = A_RCV;
                    else if (byte_rcv != 0x7E) state = START;
                    break;
                    
                case A_RCV:
                    if (byte_rcv == C_field) state = C_RCV;
                    else if (byte_rcv == 0x7E) state = FLAG_RCV;
                    else state = START;
                    break;
                    
                case C_RCV:
                    if (byte_rcv == (A_field ^ C_field)) state = BCC_OK;
                    else if (byte_rcv == 0x7E) state = FLAG_RCV;
                    else state = START;
                    break;
                    
                case BCC_OK:
                    if (byte_rcv == 0x7E) state = STOP_STATE;
                    else state = START;
                    break;
                    
                default:
                    break;
            }
        }
    }

    printf("Trama SET recebida com sucesso e validada!\n");

    // -----------------------------------------------------------------
    // CRIAÇÃO E ENVIO DA TRAMA UA (Resposta do Recetor)
    // -----------------------------------------------------------------
    unsigned char ua_frame[5];
    ua_frame[0] = 0x7E; // FLAG
    ua_frame[1] = 0x03; // A (Comando do Emissor / Resposta do Recetor)
    ua_frame[2] = 0x07; // C (Controlo: UA)
    ua_frame[3] = ua_frame[1] ^ ua_frame[2]; // BCC1 (A XOR C)
    ua_frame[4] = 0x7E; // FLAG

    int bytes_written = write(fd, ua_frame, 5);
    printf("%d bytes written\n", bytes_written);
    printf("Trama UA enviada com sucesso!\n");
    // -----------------------------------------------------------------
    // -----------------------------------------------------------------

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}