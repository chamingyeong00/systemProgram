#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_SIG_LEN 128
#define MAX_SIGS 100
#define MAX_PATH 4096
// 파일 앞부분을 넉넉하게 읽어서 시그니처가 뒤로 밀려있는 경우(MP4 offset 4 등)를 대비
#define READ_BUFFER_SIZE 1024 

typedef struct {
    unsigned char bytes[MAX_SIG_LEN];
    int len;
    char typeName[64];
    int isComment;
} FileSignature;

FileSignature signatures[MAX_SIGS];
int sig_count = 0;

int parseHex(char *hexStr, unsigned char *outBytes) {
    int count = 0;
    char *token = strtok(hexStr, " ");
    while (token != NULL) {
        if (token[0] == '#') {
             if(strlen(token) > 1) 
                 outBytes[count++] = (unsigned char)strtoul(token + 1, NULL, 16);
        } else {
            outBytes[count++] = (unsigned char)strtoul(token, NULL, 16);
        }
        token = strtok(NULL, " ");
    }
    return count;
}

void loadSignatures(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("file_sig.data open error");
        exit(1);
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        if (sig_count >= MAX_SIGS) break;

        FileSignature *sig = &signatures[sig_count];
        sig->isComment = (line[0] == '#');

        char *delimiter = strchr(line, '|');
        if (delimiter) {
            *delimiter = 0;
            char *hexPart = line;
            char *namePart = delimiter + 1;

            strncpy(sig->typeName, namePart, sizeof(sig->typeName) - 1);
            sig->len = parseHex(hexPart, sig->bytes);
            sig_count++;
        }
    }
    fclose(fp);
}

void printUsageAndInfo(const char *progName) {
    printf("filesig_length = %d :", sig_count);
    for (int i = 0; i < sig_count; i++) {
        if (!signatures[i].isComment) {
            printf(" [%s]", signatures[i].typeName);
        }
    }
    printf("\n");
    printf("Usage: %s (filename | dirname)\n", progName);
}

// [핵심 변경] 파일 내에서 시그니처를 검색하는 방식으로 변경
void checkFile(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return;

    unsigned char buffer[READ_BUFFER_SIZE];
    size_t bytesRead = fread(buffer, 1, READ_BUFFER_SIZE, fp);
    fclose(fp);

    for (int i = 0; i < sig_count; i++) {
        if (signatures[i].isComment) continue;
        if (bytesRead < signatures[i].len) continue;

        // 슬라이딩 윈도우 검색: 
        // 0번 인덱스부터 (읽은크기 - 시그니처길이)까지 이동하며 패턴 검사
        // MP4는 보통 offset 4, MPEG는 가변적이므로 이 방식이 유효함
        int found = 0;
        for (int offset = 0; offset <= bytesRead - signatures[i].len; offset++) {
            int match = 1;
            for (int j = 0; j < signatures[i].len; j++) {
                if (buffer[offset + j] != signatures[i].bytes[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                found = 1;
                break; // 찾았으면 루프 탈출
            }
        }

        if (found) {
            printf("File type of %s is %s.\n", filepath, signatures[i].typeName);
            return; // 하나라도 찾으면 종료 (우선순위는 signatures 배열 순서)
        }
    }
}

void processPath(const char *basePath) {
    struct stat st;
    if (lstat(basePath, &st) == -1) return;

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(basePath);
        if (!dir) return;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char newPath[MAX_PATH];
            snprintf(newPath, sizeof(newPath), "%s/%s", basePath, entry->d_name);
            processPath(newPath);
        }
        closedir(dir);
    } else if (S_ISREG(st.st_mode)) {
        checkFile(basePath);
    }
}

int main(int argc, char *argv[]) {
    loadSignatures("file_sig.data");

    struct stat st;
    int pathExists = 0;
    if (argc >= 2 && lstat(argv[1], &st) == 0) {
        pathExists = 1;
    }

    if (argc < 2 || !pathExists) {
        printUsageAndInfo(argv[0]);
        return 0;
    }

    printf("filesig_length = %d :", sig_count);
    for (int i = 0; i < sig_count; i++) {
        if (!signatures[i].isComment) {
            printf(" [%s]", signatures[i].typeName);
        }
    }
    printf("\n");

    processPath(argv[1]);

    return 0;
}