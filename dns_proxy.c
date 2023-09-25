#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 1024
#define CONFIG_FILE "dns_proxy_config.txt"

struct Config {
    char upstream_dns_server[16];
    char blacklist_file[256];
    char default_response[16];
};

void load_config(struct Config *config) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (file == NULL) {
        perror("Failed to open config file");
        exit(1);
    }
    fscanf(file, "upstream_dns_server = %s\n", config->upstream_dns_server);
    fscanf(file, "blacklist_file = %s\n", config->blacklist_file);
    fscanf(file, "default_response = %s\n", config->default_response);
    fclose(file);
}

int is_blacklisted(char *domain, char *blacklist_file) {
    FILE *file = fopen(blacklist_file, "r");
    if (file == NULL) {
        perror("Failed to open blacklist file");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        size_t line_length = strlen(line);

        // Проверяем, является ли последний символ символом новой строки
        if (line_length > 0 && line[line_length - 1] == '\n') {
            line[line_length - 1] = '\0'; // Удаляем символ новой строки
        }

        printf("log blacklist. domain = %s, line = %s\n", domain, line);
        if (strcmp(domain, line) == 0) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

int main() {
    struct Config config;
    load_config(&config);

    int server_socket, upstream_socket;
    struct sockaddr_in server_addr, upstream_dns_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[MAX_BUFFER_SIZE];

    if ((server_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    if ((upstream_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(1);
    }

    printf("DNS proxy server listening on port 53...\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recvfrom(server_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len) == -1) {
            perror("recvfrom");
            continue;
        }

        char *domain = &buffer[12]; // Предполагается, что buffer содержит данные и domain указывает на начало строки
        char cleaned_domain[MAX_BUFFER_SIZE - 12]; // Создаем буфер для очищенной строки
        int cleaned_index = 0; // Индекс для очищенной строки

        for (int i = 0; domain[i] != '\0'; i++) {
            if (domain[i] != '\x02' && domain[i] != '\x06') { // Проверяем, не является ли текущий символ STX или ACK
                cleaned_domain[cleaned_index++] = domain[i]; // Копируем символ в очищенную строку
            }
        }

        cleaned_domain[cleaned_index] = '\0'; // Добавляем завершающий нуль-символ

        if (is_blacklisted(cleaned_domain, config.blacklist_file)) {
            printf("Attempt to access blacklisted domain\n");
            struct sockaddr_in fake_response_addr;
            memset(&fake_response_addr, 0, sizeof(fake_response_addr));
            fake_response_addr.sin_family = AF_INET;
            fake_response_addr.sin_port = client_addr.sin_port;
            inet_pton(AF_INET, config.default_response, &(fake_response_addr.sin_addr));
            sendto(server_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&fake_response_addr, client_len);
        } else {
            printf("Request in handling\n");
            memset(&upstream_dns_addr, 0, sizeof(upstream_dns_addr));
            upstream_dns_addr.sin_family = AF_INET;
            upstream_dns_addr.sin_port = htons(53);
            inet_pton(AF_INET, config.upstream_dns_server, &(upstream_dns_addr.sin_addr));
            sendto(upstream_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&upstream_dns_addr, sizeof(upstream_dns_addr));

            memset(buffer, 0, sizeof(buffer));
            if (recvfrom(upstream_socket, buffer, sizeof(buffer), 0, NULL, NULL) == -1) {
                perror("recvfrom");
                continue;
            }

            sendto(server_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, client_len);
        }
    }

    close(server_socket);
    close(upstream_socket);
    return 0;
}
