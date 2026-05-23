/**
 * Simple FTP client supporting RFC959 commands and RFC1738 URL syntax.
 * Usage: clientFTP ftp://[user[:pass]@]host[:port]/path [local-file]
 * If the URL path is empty or ends with '/', a LIST is performed.
 * Otherwise the client downloads the file using PASV and RETR.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEFAULT_FTP_PORT "21"
#define BUFFER_SIZE 8192

typedef struct {
    char user[128];
    char pass[128];
    char host[256];
    char port[8];
    char path[1024];
} ftp_url_t;

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s ftp://[user[:pass]@]host[:port]/path [local-file]\n",
            prog);
    fprintf(stderr, "Example: %s ftp://anonymous:anon@ftp.example.com/pub/file.txt\n", prog);
    exit(EXIT_FAILURE);
}

static int starts_with(const char *s, const char *prefix) {
    return strncasecmp(s, prefix, strlen(prefix)) == 0;
}

static void parse_url(const char *url, ftp_url_t *u) {
    char *p;
    char tmp[2048];

    if (!starts_with(url, "ftp://")) {
        die("Error: URL must start with ftp://\n");
    }

    strncpy(tmp, url + 6, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    strcpy(u->user, "anonymous");
    strcpy(u->pass, "anonymous@");
    strcpy(u->port, DEFAULT_FTP_PORT);
    u->path[0] = '\0';

    p = strchr(tmp, '/');
    if (p) {
        size_t len = p - tmp;
        if (len >= sizeof(u->host)) {
            die("Error: host too long\n");
        }
        memcpy(u->host, tmp, len);
        u->host[len] = '\0';
        snprintf(u->path, sizeof(u->path), "%s", p);
    } else {
        snprintf(u->host, sizeof(u->host), "%s", tmp);
        u->path[0] = '\0';
    }

    p = strchr(u->host, '@');
    if (p) {
        char creds[256];
        size_t hostlen = u->host + sizeof(u->host) - p;
        strncpy(creds, u->host, p - u->host);
        creds[p - u->host] = '\0';
        if (strlen(creds) >= sizeof(u->user) + sizeof(u->pass)) {
            die("Error: credentials too long\n");
        }
        char *sep = strchr(creds, ':');
        if (sep) {
            *sep = '\0';
            strncpy(u->user, creds, sizeof(u->user) - 1);
            u->user[sizeof(u->user) - 1] = '\0';
            strncpy(u->pass, sep + 1, sizeof(u->pass) - 1);
            u->pass[sizeof(u->pass) - 1] = '\0';
        } else {
            strncpy(u->user, creds, sizeof(u->user) - 1);
            u->user[sizeof(u->user) - 1] = '\0';
        }
        memmove(u->host, p + 1, strlen(p + 1) + 1);
    }

    p = strchr(u->host, ':');
    if (p) {
        *p = '\0';
        strncpy(u->port, p + 1, sizeof(u->port) - 1);
        u->port[sizeof(u->port) - 1] = '\0';
    }

    if (u->host[0] == '\0') {
        die("Error: missing host in URL\n");
    }
}

static char *basename_path(const char *path) {
    const char *p = strrchr(path, '/');
    if (!p || p[1] == '\0') {
        return NULL;
    }
    return (char *)(p + 1);
}

static int open_tcp_connection(const char *host, const char *port) {
    struct hostent *h;
    struct sockaddr_in addr;
    int sock = -1;
    int i;

    h = gethostbyname(host);
    if (!h) {
        herror("gethostbyname");
        return -1;
    }

    for (i = 0; h->h_addr_list[i] != NULL; ++i) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket");
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((unsigned short)atoi(port));
        memcpy(&addr.sin_addr, h->h_addr_list[i], h->h_length);

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            return sock;
        }
        close(sock);
        sock = -1;
    }

    return -1;
}

static ssize_t read_line(int sock, char *buffer, size_t size) {
    ssize_t total = 0;
    char c;

    while (total + 1 < (ssize_t)size) {
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) {
            if (n == 0 && total > 0) break;
            return n;
        }
        buffer[total++] = c;
        if (c == '\n') break;
    }
    if (total >= 0) {
        buffer[total] = '\0';
    }
    return total;
}

static int read_reply(int sock, char *code, char *message, size_t msglen) {
    char line[BUFFER_SIZE];
    int finished = 0;
    char expected_code[4] = "";

    message[0] = '\0';

    while (!finished) {
        ssize_t n = read_line(sock, line, sizeof(line));
        if (n <= 0) {
            return -1;
        }

        if (n >= 4 && isdigit((unsigned char)line[0]) && isdigit((unsigned char)line[1]) && isdigit((unsigned char)line[2])) {
            if (expected_code[0] == '\0') {
                strncpy(expected_code, line, 3);
                expected_code[3] = '\0';
            }

            if (line[3] == ' ') {
                finished = 1;
            }
        }

        if (message[0] == '\0') {
            strncpy(message, line, msglen - 1);
            message[msglen - 1] = '\0';
        }
    }

    if (code) {
        strncpy(code, expected_code, 4);
    }
    return 0;
}

static int send_command(int sock, const char *fmt, ...) {
    char buffer[BUFFER_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    size_t len = strlen(buffer);
    if (send(sock, buffer, len, 0) != (ssize_t)len) {
        perror("send");
        return -1;
    }
    return 0;
}

static int parse_pasv_reply(const char *reply, char *ip, size_t iplen, int *port) {
    const char *p = strchr(reply, '(');
    int nums[6];

    if (!p) {
        return -1;
    }
    if (sscanf(p + 1, "%d,%d,%d,%d,%d,%d", &nums[0], &nums[1], &nums[2], &nums[3], &nums[4], &nums[5]) != 6) {
        return -1;
    }

    snprintf(ip, iplen, "%d.%d.%d.%d", nums[0], nums[1], nums[2], nums[3]);
    *port = nums[4] * 256 + nums[5];
    return 0;
}

static int open_pasv_data_connection(int ctrl_sock) {
    char reply[BUFFER_SIZE];
    char code[4];
    char ip[64];
    int port;
    int data_sock;

    if (send_command(ctrl_sock, "PASV\r\n") < 0) {
        return -1;
    }
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        return -1;
    }
    if (code[0] != '2') {
        fprintf(stderr, "PASV failed: %s", reply);
        return -1;
    }
    if (parse_pasv_reply(reply, ip, sizeof(ip), &port) < 0) {
        fprintf(stderr, "Failed to parse PASV reply: %s", reply);
        return -1;
    }

    data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(data_sock);
        return -1;
    }

    if (connect(data_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect data");
        close(data_sock);
        return -1;
    }

    return data_sock;
}

static int ftp_login(int ctrl_sock, const ftp_url_t *url) {
    char reply[BUFFER_SIZE];
    char code[4];

    if (send_command(ctrl_sock, "USER %s\r\n", url->user) < 0) return -1;
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) return -1;
    if (code[0] == '3') {
        if (send_command(ctrl_sock, "PASS %s\r\n", url->pass) < 0) return -1;
        if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) return -1;
    }
    if (code[0] != '2') {
        fprintf(stderr, "Login failed: %s", reply);
        return -1;
    }
    return 0;
}

static int ftp_set_binary(int ctrl_sock) {
    char reply[BUFFER_SIZE];
    char code[4];
    if (send_command(ctrl_sock, "TYPE I\r\n") < 0) return -1;
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) return -1;
    if (code[0] != '2') {
        fprintf(stderr, "TYPE failed: %s", reply);
        return -1;
    }
    return 0;
}

static int ftp_retr_file(int ctrl_sock, const char *path, const char *local_file) {
    char reply[BUFFER_SIZE];
    char code[4];
    int data_sock;
    FILE *out = NULL;
    char buffer[BUFFER_SIZE];
    ssize_t n;

    data_sock = open_pasv_data_connection(ctrl_sock);
    if (data_sock < 0) return -1;

    if (send_command(ctrl_sock, "RETR %s\r\n", path) < 0) {
        close(data_sock);
        return -1;
    }
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        close(data_sock);
        return -1;
    }
    if (code[0] != '1') {
        fprintf(stderr, "RETR failed: %s", reply);
        close(data_sock);
        return -1;
    }

    out = fopen(local_file, "wb");
    if (!out) {
        perror("fopen");
        close(data_sock);
        return -1;
    }

    while ((n = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        if (fwrite(buffer, 1, n, out) != (size_t)n) {
            perror("fwrite");
            fclose(out);
            close(data_sock);
            return -1;
        }
    }
    close(data_sock);
    fclose(out);

    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        return -1;
    }
    if (code[0] != '2') {
        fprintf(stderr, "Transfer failed: %s", reply);
        return -1;
    }

    printf("Downloaded %s\n", local_file);
    return 0;
}

static int ftp_list(int ctrl_sock, const char *path) {
    char reply[BUFFER_SIZE];
    char code[4];
    int data_sock;
    char buffer[BUFFER_SIZE];
    ssize_t n;

    data_sock = open_pasv_data_connection(ctrl_sock);
    if (data_sock < 0) return -1;

    if (path[0]) {
        if (send_command(ctrl_sock, "LIST %s\r\n", path) < 0) {
            close(data_sock);
            return -1;
        }
    } else {
        if (send_command(ctrl_sock, "LIST\r\n") < 0) {
            close(data_sock);
            return -1;
        }
    }
    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        close(data_sock);
        return -1;
    }
    if (code[0] != '1') {
        fprintf(stderr, "LIST failed: %s", reply);
        close(data_sock);
        return -1;
    }

    while ((n = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, n, stdout);
    }
    close(data_sock);

    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        return -1;
    }
    if (code[0] != '2') {
        fprintf(stderr, "LIST error: %s", reply);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    ftp_url_t url;
    int ctrl_sock;
    char reply[BUFFER_SIZE];
    char code[4];
    char local_file[1024] = "";
    int do_list = 0;

    if (argc < 2) {
        usage(argv[0]);
    }

    parse_url(argv[1], &url);

    if (argc >= 3) {
        strncpy(local_file, argv[2], sizeof(local_file) - 1);
        local_file[sizeof(local_file) - 1] = '\0';
    }

    if (url.path[0] == '\0' || url.path[strlen(url.path) - 1] == '/') {
        do_list = 1;
    }

    if (!do_list && local_file[0] == '\0') {
        char *name = basename_path(url.path);
        if (name) {
            strncpy(local_file, name, sizeof(local_file) - 1);
            local_file[sizeof(local_file) - 1] = '\0';
        } else {
            do_list = 1;
        }
    }

    ctrl_sock = open_tcp_connection(url.host, url.port);
    if (ctrl_sock < 0) {
        die("Failed to connect to %s:%s\n", url.host, url.port);
    }

    if (read_reply(ctrl_sock, code, reply, sizeof(reply)) < 0) {
        die("No welcome from server\n");
    }
    if (code[0] != '2') {
        die("Server error: %s", reply);
    }

    if (ftp_login(ctrl_sock, &url) < 0) {
        close(ctrl_sock);
        return EXIT_FAILURE;
    }

    if (ftp_set_binary(ctrl_sock) < 0) {
        close(ctrl_sock);
        return EXIT_FAILURE;
    }

    int status = 0;
    if (do_list) {
        status = ftp_list(ctrl_sock, url.path);
    } else {
        status = ftp_retr_file(ctrl_sock, url.path, local_file);
    }

    if (send_command(ctrl_sock, "QUIT\r\n") < 0) {
        close(ctrl_sock);
        return EXIT_FAILURE;
    }
    read_reply(ctrl_sock, code, reply, sizeof(reply));
    close(ctrl_sock);
    return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
