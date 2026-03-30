#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h> // Necessário para os alarmes

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 

#define FALSE 0
#define TRUE 1

// Estados para a Máquina de Estados
typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP_STATE
} State;

// Variáveis globais para o Alarme
int alarmEnabled = FALSE;
int alarmCount = 0;

// Função que é chamada quando o alarme dispara
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarme #%d disparado!\n", alarmCount);
}

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
    
    // Leitura não bloqueante para permitir que o ciclo continue a verificar o alarme
    newtio.c_cc[VTIME] = 1; // Timeout de 0.1s
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    // Configurar a função que lida com o alarme
    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    // -----------------------------------------------------------------
    // LÓGICA DE ENVIO DO SET E RECEÇÃO DO UA (C/ RETRANSMISSÃO)
    // -----------------------------------------------------------------
    unsigned char set_frame[5] = {0x7E, 0x03, 0x03, 0x03 ^ 0x03, 0x7E};
    
    State state = START;
    unsigned char byte_rcv;
    unsigned char A_field_ua = 0x03; 
    unsigned char C_field_ua = 0x07; // Controlo esperado para a trama UA

    while (alarmCount < 3 && state != STOP_STATE)
    {
        if (alarmEnabled == FALSE)
        {
            write(fd, set_frame, 5);
            printf("Trama SET enviada.\n");
            
            alarm(3); // Ativa alarme de 3 segundos
            alarmEnabled = TRUE;
            state = START; // Reinicia a máquina de estados
        }

        // Tenta ler 1 byte (não bloqueia para sempre por causa do VTIME e VMIN)
        if (read(fd, &byte_rcv, 1) > 0)
        {
            printf("0x%02hhX\n",byte_rcv);
            switch (state) 
            {
                case START:
                    printf("1");
                    if (byte_rcv == 0x7E) state = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    printf("2");
                    if (byte_rcv == A_field_ua) state = A_RCV;
                    else if (byte_rcv != 0x7E) state = START;
                    break;
                case A_RCV:
                    printf("3");
                    if (byte_rcv == C_field_ua) state = C_RCV;
                    else if (byte_rcv == 0x7E) state = FLAG_RCV;
                    else state = START;
                    break;
                case C_RCV:
                    printf("4");
                    if (byte_rcv == (A_field_ua ^ C_field_ua)) state = BCC_OK;
                    else if (byte_rcv == 0x7E) state = FLAG_RCV;
                    else state = START;
                    break;
                case BCC_OK:
                    printf("5");
                    if (byte_rcv == 0x7E) {
                        state = STOP_STATE;
                        alarm(0); // DESLIGA O ALARME! Recebemos o UA.
                        printf("Trama UA recebida com sucesso! Ligacao estabelecida.\n");
                    }
                    else state = START;
                    break;
                default:
                    break;
            }
        }
    }

    if (state != STOP_STATE) {
        printf("Falha na ligacao: Trama UA nao recebida apos 3 tentativas.\n");
    }
    // -----------------------------------------------------------------

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    return 0;
}