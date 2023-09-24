#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 1024
#define CONFIG_FILE "dns_proxy_config.txt"

// Структура для хранения конфигурации
struct Config {
    char upstream_dns_server[16];
    char blacklist_file[256];
    char default_response[16];
};

// Функция для загрузки конфигурации из файла
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

// Функция для проверки, содержится ли домен в "черном" списке
int is_blacklisted(char *domain, char *blacklist_file) {
    FILE *file = fopen(blacklist_file, "r");
    if (file == NULL) {
        perror("Failed to open blacklist file");
        return 0; // Если файл не найден, не блокируем запрос
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strlen(line) - 1] = '\0'; // Удаляем символ новой строки
        if (strcmp(domain, line) == 0) {
            fclose(file);
            return 1; // Домен найден в "черном" списке, блокируем запрос
        }
    }

    fclose(file);
    return 0; // Домен не найден в "черном" списке, не блокируем запрос
}

int main() {
    struct Config config;
    load_config(&config);

    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[MAX_BUFFER_SIZE];

    // Создаем сокет
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // Настраиваем адрес сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53); // Порт DNS
    inet_pton(AF_INET, "0.0.0.0", &(server_addr.sin_addr));

    // Привязываем сокет к локальному адресу и порту
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(1);
    }

    printf("DNS proxy server listening on port 53...\n");

    while (1) {
        // Принимаем DNS-запрос от клиента
        memset(buffer, 0, sizeof(buffer));
        if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len) == -1) {
            perror("recvfrom");
            continue;
        }

        // Извлекаем доменное имя из DNS-запроса (позиция зависит от структуры DNS-пакета)
        char *domain = &buffer[12]; // Пропускаем заголовок DNS-запроса

        // Проверяем, содержится ли домен в "черном" списке
        if (is_blacklisted(domain, config.blacklist_file)) {
            // Если домен заблокирован, отправляем клиенту ответ по умолчанию
            struct sockaddr_in fake_response_addr;
            memset(&fake_response_addr, 0, sizeof(fake_response_addr));
            fake_response_addr.sin_family = AF_INET;
            fake_response_addr.sin_port = client_addr.sin_port;
            inet_pton(AF_INET, config.default_response, &(fake_response_addr.sin_addr));
            sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&fake_response_addr, client_len);
        } else {
            // Если домен не заблокирован, пересылаем запрос к удаленному DNS-серверу
            struct sockaddr_in upstream_dns_addr;
            memset(&upstream_dns_addr, 0, sizeof(upstream_dns_addr));
            upstream_dns_addr.sin_family = AF_INET;
            upstream_dns_addr.sin_port = htons(53); // Порт DNS
            inet_pton(AF_INET, config.upstream_dns_server, &(upstream_dns_addr.sin_addr));
            sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&upstream_dns_addr, sizeof(upstream_dns_addr));

            // Получаем ответ от удаленного DNS-сервера и отправляем его клиенту
            memset(buffer, 0, sizeof(buffer));
            if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len) == -1) {
                perror("recvfrom");
                continue;
            }

            sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, client_len);
        }
    }

    close(sockfd);
    return 0;
}
