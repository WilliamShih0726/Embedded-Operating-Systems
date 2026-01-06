#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

int fd;

void sigint_handler(int signo) {
    close(fd);
    printf("\n程式已結束\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <device_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);

    if ((fd = open(argv[1], O_RDONLY)) < 0) {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }

    char key;
    while (1) {
        int ret = read(fd, &key, 1);
        if (ret == -1) {
            perror("read()");
            exit(EXIT_FAILURE);
        }

        printf("讀到按鍵：%c\n", key);
    }

    close(fd);
    return 0;
}
