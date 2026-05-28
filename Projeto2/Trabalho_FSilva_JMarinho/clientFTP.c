/**
 * CLIENTE FTP - Programa que estabelece conexão com servidor FTP
 *
 * Funcionalidade:
 * - Faz login num servidor FTP usando URL no formato: ftp://[utilizador[:palavra]@]host[:porta]/caminho
 * - Se o caminho termina em '/' ou está vazio: lista os ficheiros (comando LIST)
 * - Se é um ficheiro: descarrega o ficheiro (comandos PASV e RETR)
 *
 * Modo de uso: clientFTP ftp://[user[:pass]@]host[:port]/path [ficheiro_local]
 * Exemplo: clientFTP ftp://anonimo:anonimo@ftp.exemplo.com/pub/ficheiro.txt
 */

// ===== BIBLIOTECAS NECESSÁRIAS =====
#include <stdio.h>          // Para entrada/saída (printf, fprintf, fopen, etc.)
#include <stdlib.h>         // Para funções gerais (malloc, exit, atoi, etc.)
#include <string.h>         // Para manipulação de strings (strcpy, strcmp, etc.)
#include <unistd.h>         // Para funções de sistema (close, etc.)
#include <errno.h>          // Para tratamento de erros
#include <stdarg.h>         // Para argumentos variáveis em funções
#include <sys/types.h>      // Para tipos de dados do sistema
#include <sys/socket.h>     // Para operações com sockets (socket, connect, send, recv, etc.)
#include <netinet/in.h>     // Para estruturas de endereços de rede (sockaddr_in, htons, etc.)
#include <arpa/inet.h>      // Para conversão de endereços (inet_pton, etc.)
#include <netdb.h>          // Para DNS (gethostbyname, herror, etc.)
#include <ctype.h>

// ===== CONSTANTES =====
#define DEFAULT_FTP_PORT "21"  // Porta padrão do FTP é a 21
#define BUFFER_SIZE 8192        // Tamanho do buffer para ler/escrever dados (8 KB)

// ===== ESTRUTURA DE DADOS PARA GUARDAR INFORMAÇÕES DA URL =====
/**
 * Esta estrutura guarda os diferentes componentes extraídos de uma URL FTP.
 * Por exemplo: ftp://utilizador:palavra@host.com:2121/caminho/ficheiro.txt
 */
typedef struct {
    char user[128];     // Nome de utilizador para fazer login
    char pass[128];     // Palavra-passe para fazer login
    char host[256];     // Endereço do servidor (ex: ftp.exemplo.com)
    char port[8];       // Porta do servidor (ex: 21)
    char path[1024];    // Caminho no servidor (ex: /pub/ficheiro.txt)
} ftp_url_t;

// ===== FUNÇÕES DE UTILIDADE =====

/**
 * Função die() - Termina o programa com mensagem de erro
 *
 * O que faz:
 * - Imprime uma mensagem de erro no ecrã
 * - Termina o programa (exit)
 *
 * Parâmetros:
 * - fmt: Mensagem formatada (como em printf)
 * - ...: Argumentos adicionais (como em printf)
 */
static void die(const char *fmt, ...) {
    va_list ap;                    // Variável para guardar argumentos
    va_start(ap, fmt);             // Preparar para ler argumentos
    vfprintf(stderr, fmt, ap);     // Imprimir erro no ecrã
    va_end(ap);                    // Terminar leitura de argumentos
    exit(EXIT_FAILURE);            // Sair com erro
}

/**
 * Função usage() - Mostra como usar o programa
 *
 * O que faz:
 * - Imprime as instruções de como correr o programa
 * - Mostra um exemplo de utilização
 * - Termina o programa
 *
 * Parâmetro:
 * - prog: Nome do programa (argv[0])
 */
static void usage(const char *prog) {
    // Mostrar como usar o programa
    fprintf(stderr,
            "Usage: %s ftp://[user[:pass]@]host[:port]/path [local-file]\n",
            prog);
    // Mostrar um exemplo prático
    fprintf(stderr, "Example: %s ftp://anonymous:anon@ftp.example.com/pub/file.txt\n", prog);
    exit(EXIT_FAILURE);
}

/**
 * Função starts_with() - Verifica se uma string começa com um prefixo
 *
 * O que faz:
 * - Compara o início de uma string com um prefixo dado
 * - Retorna verdadeiro se a string começa com esse prefixo
 *
 * Parâmetros:
 * - s: A string a verificar
 * - prefix: O prefixo a procurar (ex: "ftp://")
 *
 * Retorno:
 * - 1 (verdadeiro) se a string começa com o prefixo
 * - 0 (falso) se não começa
 */
static int starts_with(const char *s, const char *prefix) {
    // strncasecmp compara de forma insensível a maiúsculas/minúsculas
    return strncasecmp(s, prefix, strlen(prefix)) == 0;
}

/**
 * Função parse_url() - Separa uma URL FTP nos seus componentes
 *
 * O que faz:
 * - Recebe uma URL completa (ex: ftp://user:pass@host:port/path)
 * - Extrai cada parte: utilizador, palavra-passe, host, porta, caminho
 * - Guarda tudo na estrutura fornecida
 *
 * Parâmetros:
 * - url: URL completa a processar
 * - u: Apontador para estrutura onde guardar os componentes
 */
static void parse_url(const char *url, ftp_url_t *u) {
    char *p;           // Apontador temporário para procurar caracteres
    char tmp[2048];    // Buffer temporário para guardar a URL sem o "ftp://"

    // Verificar se URL começa com "ftp://"
    if (!starts_with(url, "ftp://")) {
        die("Error: URL must start with ftp://\n");
    }

    // Copiar URL sem o prefixo "ftp://" (que tem 6 caracteres)
    strncpy(tmp, url + 6, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    // Definir valores padrão
    strcpy(u->user, "anonymous");        // Utilizador padrão
    strcpy(u->pass, "anonymous@");       // Palavra-passe padrão
    strcpy(u->port, DEFAULT_FTP_PORT);   // Porta padrão (21)
    u->path[0] = '\0';                   // Caminho vazio

    // Procurar o "slash" que separa o host do caminho
    p = strchr(tmp, '/');
    if (p) {
        // Host vai desde o início até ao primeiro "/"
        size_t len = p - tmp;  // Calcular tamanho do host
        if (len >= sizeof(u->host)) {
            die("Error: host too long\n");
        }
        memcpy(u->host, tmp, len);     // Copiar host
        u->host[len] = '\0';           // Terminar string
        snprintf(u->path, sizeof(u->path), "%s", p);  // Copiar caminho
    } else {
        // Se não há "/", tudo é o host
        snprintf(u->host, sizeof(u->host), "%s", tmp);
        u->path[0] = '\0';  // Sem caminho
    }

    // Procurar o "@" que separa as credenciais do host
    // (ex: "utilizador:palavra@host" -> utilizar está antes do @)
    p = strchr(u->host, '@');
    if (p) {
        char creds[256];  // Buffer para guardar credenciais
        size_t hostlen = u->host + sizeof(u->host) - p;

        // Extrair a parte das credenciais (antes do @)
        strncpy(creds, u->host, p - u->host);
        creds[p - u->host] = '\0';

        if (strlen(creds) >= sizeof(u->user) + sizeof(u->pass)) {
            die("Error: credentials too long\n");
        }

        // Procurar o ":" que separa utilizador de palavra-passe
        char *sep = strchr(creds, ':');
        if (sep) {
            // Há utilizador E palavra-passe
            *sep = '\0';
            strncpy(u->user, creds, sizeof(u->user) - 1);
            u->user[sizeof(u->user) - 1] = '\0';
            strncpy(u->pass, sep + 1, sizeof(u->pass) - 1);
            u->pass[sizeof(u->pass) - 1] = '\0';
        } else {
            // Há só utilizador, sem palavra-passe
            strncpy(u->user, creds, sizeof(u->user) - 1);
            u->user[sizeof(u->user) - 1] = '\0';
        }

        // Remover as credenciais do host (mover o que está após @ para o início)
        memmove(u->host, p + 1, strlen(p + 1) + 1);
    }

    // Procurar o ":" que separa o host da porta
    // (ex: "host.com:2121" -> porta está após o :)
    p = strchr(u->host, ':');
    if (p) {
        *p = '\0';  // Cortar o host no ":"
        // Copiar a porta (tudo depois do ":")
        strncpy(u->port, p + 1, sizeof(u->port) - 1);
        u->port[sizeof(u->port) - 1] = '\0';
    }

    // Validação final: o host é obrigatório
    if (u->host[0] == '\0') {
        die("Error: missing host in URL\n");
    }
}

/**
 * Função basename_path() - Extrai o nome do ficheiro a partir de um caminho
 *
 * O que faz:
 * - Procura o último "/" no caminho
 * - Retorna apenas o nome do ficheiro (parte após o último "/")
 * - Se não encontrar nome, retorna NULL
 *
 * Exemplos:
 * - "/pub/ficheiro.txt" -> "ficheiro.txt"
 * - "/pub/" -> NULL
 * - "/pub" -> "pub"
 *
 * Parâmetro:
 * - path: Caminho completo
 *
 * Retorno:
 * - Apontador para o nome do ficheiro, ou NULL se vazio
 */
static char *basename_path(const char *path) {
    // Procurar o último "/" no caminho
    const char *p = strrchr(path, '/');

    // Se não há "/" ou o que vem após é vazio, não há nome de ficheiro
    if (!p || p[1] == '\0') {
        return NULL;
    }

    // Retornar apontador para o nome (tudo após o último "/")
    return (char *)(p + 1);
}

/**
 * Função open_tcp_connection() - Abre uma conexão TCP com um servidor
 *
 * O que faz:
 * - Converte o nome do servidor em endereço IP (usando DNS)
 * - Cria um socket TCP
 * - Tenta conectar ao servidor
 * - Retorna o socket se conseguir, ou -1 se falhar
 *
 * Parâmetros:
 * - host: Nome do servidor (ex: "ftp.exemplo.com")
 * - port: Porta em forma de texto (ex: "21")
 *
 * Retorno:
 * - Socket (número positivo) se sucesso
 * - -1 se falha
 */
static int open_tcp_connection(const char *host, const char *port) {
    struct hostent *h;      // Estrutura para guardar informações de DNS
    struct sockaddr_in addr; // Endereço do servidor
    int sock = -1;           // Socket (file descriptor)
    int i;                   // Contador para tentar vários endereços IP

    // Converter nome do servidor em endereço IP(s)
    h = gethostbyname(host);
    if (!h) {
        herror("gethostbyname");  // Imprimir erro de DNS
        return -1;
    }

    // Tentar conectar a cada endereço IP disponível
    for (i = 0; h->h_addr_list[i] != NULL; ++i) {
        // Criar um novo socket TCP
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket");
            return -1;
        }

        // Preparar estrutura com endereço do servidor
        memset(&addr, 0, sizeof(addr));           // Limpar memória
        addr.sin_family = AF_INET;                 // Usar IPv4
        addr.sin_port = htons((unsigned short)atoi(port));  // Converter porta para network byte order
        memcpy(&addr.sin_addr, h->h_addr_list[i], h->h_length);  // Copiar endereço IP

        // Tentar conectar
        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            return sock;  // Sucesso! Retornar socket
        }

        // Se falhou, fechar socket e tentar o próximo IP
        close(sock);
        sock = -1;
    }

    // Todos os IPs falharam
    return -1;
}

/**
 * Função read_line() - Lê uma linha completa de um socket
 *
 * O que faz:
 * - Lê caracteres um a um do socket
 * - Para quando encontra uma quebra de linha (\n)
 * - Retorna a linha completa num buffer
 *
 * Parâmetros:
 * - sock: Socket para ler
 * - buffer: Onde guardar a linha lida
 * - size: Tamanho máximo do buffer
 *
 * Retorno:
 * - Número de bytes lidos
 * - 0 se conexão fechou
 * - -1 se erro
 */
static ssize_t read_line(int sock, char *buffer, size_t size) {
    ssize_t total = 0;  // Contador de bytes lidos
    char c;              // Caractere lido

    // Ler caracteres até encher o buffer ou encontrar quebra de linha
    while (total + 1 < (ssize_t)size) {
        // Ler 1 byte do socket
        ssize_t n = recv(sock, &c, 1, 0);

        if (n <= 0) {
            // Conexão fechou ou erro
            if (n == 0 && total > 0) break;  // Fechamento normal
            return n;                         // Erro ou desconexão
        }

        // Adicionar caractere ao buffer
        buffer[total++] = c;

        // Se é quebra de linha, parar
        if (c == '\n') break;
    }

    // Terminar a string com nulo
    if (total >= 0) {
        buffer[total] = '\0';
    }

    return total;
}

/**
 * Função read_reply() - Lê uma resposta completa do servidor FTP
 *
 * O que faz:
 * - Lê linhas até receber a resposta final do servidor
 * - Extrai o código de resposta (3 dígitos no início)
 * - Extrai a mensagem de resposta
 * - Em FTP, linhas que começam com "DDD " (3 dígitos e espaço) indicam fim
 *
 * Exemplos de respostas FTP:
 * - "220 Welcome to FTP server\r\n" (uma linha apenas)
 * - "150 Opening data connection\r\n230 User logged in\r\n" (várias linhas)
 *
 * Parâmetros:
 * - sock: Socket de controlo FTP
 * - code: Apontador para guardar o código (ex: "220")
 * - message: Apontador para guardar a mensagem
 * - msglen: Tamanho máximo da mensagem
 *
 * Retorno:
 * - 0 se sucesso
 * - -1 se erro
 */
static int read_reply(int sock, char *code, char *message, size_t msglen) {
    char line[BUFFER_SIZE];         // Buffer para cada linha lida
    int finished = 0;               // Flag para indicar se resposta terminou
    char expected_code[4] = "";     // Código extraído (ex: "220")

    message[0] = '\0';              // Limpar mensagem

    // Ler linhas até ter resposta completa
    while (!finished) {
        // Ler uma linha
        ssize_t n = read_line(sock, line, sizeof(line));
        if (n <= 0) {
            return -1;  // Erro ao ler
        }

        // Verificar se primeira linha tem código FTP (3 dígitos)
        if (n >= 4 && isdigit((unsigned char)line[0]) &&
            isdigit((unsigned char)line[1]) &&
            isdigit((unsigned char)line[2])) {

            // Guardar o código da primeira vez
            if (expected_code[0] == '\0') {
                strncpy(expected_code, line, 3);
                expected_code[3] = '\0';
            }

            // Se há espaço após código, é a última linha
            if (line[3] == ' ') {
                finished = 1;
            }
        }

        // Guardar a primeira linha como mensagem
        if (message[0] == '\0') {
            strncpy(message, line, msglen - 1);
            message[msglen - 1] = '\0';
        }
    }

    // Copiar código para output
    if (code) {
        strncpy(code, expected_code, 4);
    }
    return 0;
}

/**
 * Função send_command() - Envia um comando para o servidor FTP
 *
 * O que faz:
 * - Formata um comando (como printf)
 * - Envia o comando completo pelo socket
 *
 * Parâmetros:
 * - sock: Socket de controlo FTP
 * - fmt: Formato do comando (ex: "USER %s\r\n")
 * - ...: Argumentos (como em printf)
 *
 * Retorno:
 * - 0 se enviado com sucesso
 * - -1 se erro
 */
static int send_command(int sock, const char *fmt, ...) {
    char buffer[BUFFER_SIZE];  // Buffer para o comando formatado
    va_list ap;                // Lista de argumentos variáveis

    // Preparar argumentos
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);  // Formatar comando
    va_end(ap);

    // Enviar o comando
    size_t len = strlen(buffer);
    if (send(sock, buffer, len, 0) != (ssize_t)len) {
        perror("send");  // Erro ao enviar
        return -1;
    }
    return 0;
}

/**
 * Função parse_pasv_reply() - Extrai IP e porta da resposta PASV
 *
 * O que faz:
 * - O comando PASV do FTP retorna uma string com IP e porta
 * - Essa função extrai esses dados da resposta
 *
 * Exemplo de resposta:
 * - "227 Entering Passive Mode (192,168,1,100,195,21)"
 * - Extrai: IP=192.168.1.100, porta = 195*256 + 21 = 50005
 *
 * Parâmetros:
 * - reply: A resposta do servidor
 * - ip: Buffer onde guardar o IP extraído
 * - iplen: Tamanho máximo do buffer IP
 * - port: Apontador para guardar a porta
 *
 * Retorno:
 * - 0 se sucesso
 * - -1 se erro ao parsing
 */
static int parse_pasv_reply(const char *reply, char *ip, size_t iplen, int *port) {
    // Procurar o "(" que começa os números
    const char *p = strchr(reply, '(');
    int nums[6];  // Array para guardar 6 números (4 para IP, 2 para porta)

    if (!p) {
        return -1;  // Não encontrou o "("
    }

    // Ler os 6 números separados por vírgulas
    if (sscanf(p + 1, "%d,%d,%d,%d,%d,%d", &nums[0], &nums[1], &nums[2], &nums[3], &nums[4], &nums[5]) != 6) {
        return -1;  // Não conseguiu ler os 6 números
    }

    // Montar o IP a partir dos primeiros 4 números
    snprintf(ip, iplen, "%d.%d.%d.%d", nums[0], nums[1], nums[2], nums[3]);

    // Calcular porta a partir dos últimos 2 números
    // (cada número é um byte, porta = high_byte * 256 + low_byte)
    *port = nums[4] * 256 + nums[5];

    return 0;
}

/**
 * Função open_pasv_data_connection() - Abre conexão de dados em modo PASV
 *
 * O que faz:
 * - Em FTP normal, o cliente diz ao servidor "Coloque-se à escuta"
 * - O servidor então diz ao cliente "Espera em X.X.X.X:YYYY"
 * - Esta função faz exatamente isso:
 *   1) Envia comando PASV
 *   2) Recebe resposta com IP e porta
 *   3) Conecta nesse IP e porta
 * - Retorna um novo socket para transferência de dados
 *
 * Parâmetro:
 * - ctrl_sock: Socket de controlo já conectado
 *
 * Retorno:
 * - Socket de dados se sucesso
 * - -1 se erro
 */
static int open_pasv_data_connection(int ctrl_sock) {
    char reply[BUFFER_SIZE];  // Resposta do servidor
    char code[4];             // Código da resposta (ex: "227")
    char ip[64];              // IP extraído da resposta
    int port;                 // Porta extraída da resposta
    int data_sock;            // Socket de dados (a retornar)

    // Enviar comando PASV
    if (send_command(ctrl_sock, "PASV\r\n") < 0) {
        return -1;
    }

    // Ler resposta do comando PASV
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        return -1;
    }

    // Verificar se resposta começou com "2" (sucesso)
    if (code[0] != '2') {
        fprintf(stderr, "PASV failed: %s", reply);
        return -1;
    }

    // Extrair IP e porta da resposta
    if (parse_pasv_reply(reply, ip, sizeof(ip), &port) < 0) {
        fprintf(stderr, "Failed to parse PASV reply: %s", reply);
        return -1;
    }

    // Criar um novo socket TCP para os dados
    data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock < 0) {
        perror("socket");
        return -1;
    }

    // Preparar endereço para conectar
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;                      // IPv4
    addr.sin_port = htons((unsigned short)port);   // Porta

    // Converter IP de texto para formato binário
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(data_sock);
        return -1;
    }

    // Conectar ao servidor no IP e porta extraídos
    if (connect(data_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect data");
        close(data_sock);
        return -1;
    }

    return data_sock;  // Retornar socket de dados aberto
}

/**
 * Função ftp_login() - Faz login no servidor FTP
 *
 * O que faz:
 * - Envia nome de utilizador (comando USER)
 * - Se o servidor pedir, envia a palavra-passe (comando PASS)
 * - Verifica se login foi bem-sucedido
 *
 * Protocolo:
 * 1. Enviar: USER nomedoutilizador
 *    Resposta: 331 (precisa de palavra-passe) ou 230 (já entrou)
 * 2. Se 331, enviar: PASS palavra-passe
 *    Resposta: 230 (sucesso) ou 530 (falha)
 *
 * Parâmetros:
 * - ctrl_sock: Socket de controlo conectado
 * - url: Estrutura com utilizador e palavra-passe
 *
 * Retorno:
 * - 0 se login bem-sucedido
 * - -1 se erro
 */
static int ftp_login(int ctrl_sock, const ftp_url_t *url) {
    char reply[BUFFER_SIZE];  // Resposta do servidor
    char code[4];             // Código de resposta

    // Enviar nome de utilizador
    if (send_command(ctrl_sock, "USER %s\r\n", url->user) < 0) return -1;
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) return -1;

    // Se servidor responde "3", quer a palavra-passe
    if (code[0] == '3') {
        if (send_command(ctrl_sock, "PASS %s\r\n", url->pass) < 0) return -1;
        if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) return -1;
    }

    // Verificar se sucesso (código começado em "2")
    if (code[0] != '2') {
        fprintf(stderr, "Login failed: %s", reply);
        return -1;
    }

    return 0;  // Login bem-sucedido
}

/**
 * Função ftp_set_binary() - Define modo de transferência para binário
 *
 * O que faz:
 * - Em FTP há dois modos: ASCII (texto) e BINARY (binário)
 * - BINARY preserva ficheiros exatamente como estão (necessário para programas, imagens, etc.)
 * - Envia comando TYPE I (Image = Binário)
 *
 * Parâmetro:
 * - ctrl_sock: Socket de controlo conectado
 *
 * Retorno:
 * - 0 se sucesso
 * - -1 se erro
 */
static int ftp_set_binary(int ctrl_sock) {
    char reply[BUFFER_SIZE];  // Resposta do servidor
    char code[4];             // Código de resposta

    // Enviar comando TYPE I (Image = modo binário)
    if (send_command(ctrl_sock, "TYPE I\r\n") < 0) return -1;
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) return -1;

    // Verificar sucesso
    if (code[0] != '2') {
        fprintf(stderr, "TYPE failed: %s", reply);
        return -1;
    }

    return 0;
}

/**
 * Função ftp_retr_file() - Descarrega um ficheiro do servidor FTP
 *
 * O que faz:
 * - Abre conexão de dados (PASV)
 * - Envia comando RETR para começar a transferência
 * - Recebe todos os dados através da conexão de dados
 * - Guarda os dados num ficheiro local
 *
 * Parâmetros:
 * - ctrl_sock: Socket de controlo FTP
 * - path: Caminho do ficheiro no servidor (ex: "/pub/ficheiro.txt")
 * - local_file: Nome do ficheiro local para guardar
 *
 * Retorno:
 * - 0 se download bem-sucedido
 * - -1 se erro
 */
static int ftp_retr_file(int ctrl_sock, const char *path, const char *local_file) {
    char reply[BUFFER_SIZE];      // Resposta do servidor
    char code[4];                 // Código de resposta
    int data_sock;                // Socket para transferência de dados
    FILE *out = NULL;             // Ficheiro local aberto
    char buffer[BUFFER_SIZE];     // Buffer para dados recebidos
    ssize_t n;                    // Número de bytes recebidos

    // Abrir conexão de dados em modo PASV
    data_sock = open_pasv_data_connection(ctrl_sock);
    if (data_sock < 0) return -1;

    // Enviar comando RETR (retrieve = descarregar)
    if (send_command(ctrl_sock, "RETR %s\r\n", path) < 0) {
        close(data_sock);
        return -1;
    }

    // Ler resposta do RETR
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        close(data_sock);
        return -1;
    }

    // Código "1" significa está a transferir dados
    if (code[0] != '1') {
        fprintf(stderr, "RETR failed: %s", reply);
        close(data_sock);
        return -1;
    }

    // Abrir ficheiro local para escrita
    out = fopen(local_file, "wb");  // wb = write binary
    if (!out) {
        perror("fopen");
        close(data_sock);
        return -1;
    }

    // Ler dados do socket e escrever no ficheiro
    while ((n = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        // Escrever dados no ficheiro
        if (fwrite(buffer, 1, n, out) != (size_t)n) {
            perror("fwrite");  // Erro ao escrever
            fclose(out);
            close(data_sock);
            return -1;
        }
    }

    // Fechar sockets e ficheiro
    close(data_sock);
    fclose(out);

    // Ler resposta final do servidor
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        return -1;
    }

    // Código "2" significa transferência completa
    if (code[0] != '2') {
        fprintf(stderr, "Transfer failed: %s", reply);
        return -1;
    }

    // Sucesso!
    printf("Downloaded %s\n", local_file);
    return 0;
}

/**
 * Função ftp_list() - Lista ficheiros de um diretório no servidor
 *
 * O que faz:
 * - Abre conexão de dados (PASV)
 * - Envia comando LIST para obter lista de ficheiros
 * - Recebe a lista e imprime na consola
 *
 * Parâmetros:
 * - ctrl_sock: Socket de controlo FTP
 * - path: Caminho do diretório a listar (ex: "/pub") ou vazio para listar atual
 *
 * Retorno:
 * - 0 se lista bem-sucedida
 * - -1 se erro
 */
static int ftp_list(int ctrl_sock, const char *path) {
    char reply[BUFFER_SIZE];      // Resposta do servidor
    char code[4];                 // Código de resposta
    int data_sock;                // Socket de dados
    char buffer[BUFFER_SIZE];     // Buffer para dados da lista
    ssize_t n;                    // Número de bytes recebidos

    // Abrir conexão de dados em modo PASV
    data_sock = open_pasv_data_connection(ctrl_sock);
    if (data_sock < 0) return -1;

    // Enviar comando LIST (com caminho se fornecido)
    if (path[0]) {
        // Path fornecido
        if (send_command(ctrl_sock, "LIST %s\r\n", path) < 0) {
            close(data_sock);
            return -1;
        }
    } else {
        // Path vazio - listar diretório atual
        if (send_command(ctrl_sock, "LIST\r\n") < 0) {
            close(data_sock);
            return -1;
        }
    }

    // Ler resposta do LIST
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        close(data_sock);
        return -1;
    }

    // Código "1" significa está a enviar dados
    if (code[0] != '1') {
        fprintf(stderr, "LIST failed: %s", reply);
        close(data_sock);
        return -1;
    }

    // Ler dados da lista e imprimir no ecrã
    while ((n = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        // Escrever dados para stdout (consola)
        fwrite(buffer, 1, n, stdout);
    }

    // Fechar socket de dados
    close(data_sock);

    // Ler resposta final do servidor
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        return -1;
    }

    // Código "2" significa operação completa
    if (code[0] != '2') {
        fprintf(stderr, "LIST error: %s", reply);
        return -1;
    }

    return 0;  // Sucesso!
}

// ===== FUNÇÃO PRINCIPAL =====

/**
 * main() - Função principal do programa
 *
 * Fluxo do programa:
 * 1. Verificar argumentos da linha de comando
 * 2. Fazer parsing da URL FTP
 * 3. Conectar ao servidor
 * 4. Fazer login
 * 5. Definir modo binário
 * 6. Listar ficheiros OU descarregar um ficheiro
 * 7. Desconectar
 *
 * Parâmetros:
 * - argc: Número de argumentos
 * - argv: Array de argumentos (argv[0]=programa, argv[1]=URL FTP, argv[2]=ficheiro local opcional)
 *
 * Retorno:
 * - EXIT_SUCCESS se sucesso
 * - EXIT_FAILURE se erro
 */
int main(int argc, char **argv) {
    // ===== VARIÁVEIS =====
    ftp_url_t url;              // Estrutura para guardar componentes da URL
    int ctrl_sock;              // Socket de controlo FTP
    char reply[BUFFER_SIZE];    // Resposta do servidor
    char code[4];               // Código de resposta
    char local_file[1024] = ""; // Nome do ficheiro local
    int do_list = 0;            // Flag: fazer LIST (1) ou RETR (0)

    // Verificar se foi fornecida a URL
    if (argc < 2) {
        usage(argv[0]);
    }

    // Fazer parsing da URL (extrair utilizador, host, porta, caminho, etc.)
    parse_url(argv[1], &url);

    // Se foi fornecido nome de ficheiro local
    if (argc >= 3) {
        strncpy(local_file, argv[2], sizeof(local_file) - 1);
        local_file[sizeof(local_file) - 1] = '\0';
    }

    // Decidir se é LIST ou RETR:
    // - Se caminho vazio ou termina em "/" -> fazer LIST
    if (url.path[0] == '\0' || url.path[strlen(url.path) - 1] == '/') {
        do_list = 1;  // Será um LIST (listar diretório)
    }

    // Se for RETR mas não forneceu nome local, extrair do caminho
    if (!do_list && local_file[0] == '\0') {
        // Extrair nome do ficheiro do caminho (ex: /pub/ficheiro.txt -> ficheiro.txt)
        char *name = basename_path(url.path);
        if (name) {
            strncpy(local_file, name, sizeof(local_file) - 1);
            local_file[sizeof(local_file) - 1] = '\0';
        } else {
            // Se não conseguiu extrair nome, fazer LIST em vez disso
            do_list = 1;
        }
    }

    // ===== FASE 1: CONECTAR =====
    // Tentar conectar ao servidor FTP
    ctrl_sock = open_tcp_connection(url.host, url.port);
    if (ctrl_sock < 0) {
        die("Failed to connect to %s:%s\n", url.host, url.port);
    }

    // Receber mensagem de boas-vindas do servidor
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        die("No welcome from server\n");
    }
    if (code[0] != '2') {
        die("Server error: %s", reply);
    }

    // ===== FASE 2: FAZER LOGIN =====
    if (ftp_login(ctrl_sock, &url) < 0) {
        close(ctrl_sock);
        return EXIT_FAILURE;
    }

    // ===== FASE 3: DEFINIR MODO BINÁRIO =====
    if (ftp_set_binary(ctrl_sock) < 0) {
        close(ctrl_sock);
        return EXIT_FAILURE;
    }

    // ===== FASE 4: TRANSFERIR DADOS =====
    // Fazer LIST ou RETR dependendo do tipo de caminho
    int status = 0;
    if (do_list) {
        // Listar diretório
        status = ftp_list(ctrl_sock, url.path);
    } else {
        // Descarregar ficheiro
        status = ftp_retr_file(ctrl_sock, url.path, local_file);
    }

    // ===== FASE 5: DESCONECTAR =====
    // Enviar comando QUIT
    if (send_command(ctrl_sock, "QUIT\r\n") < 0) {
        close(ctrl_sock);
        return EXIT_FAILURE;
    }

    // Ler resposta final
    read_reply(ctrl_sock, code, reply, sizeof(reply));

    // Fechar socket
    close(ctrl_sock);

    // Retornar resultado
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
