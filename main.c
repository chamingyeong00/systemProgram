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

// 파일 시그니처 정보를 저장할 구조체
typedef struct {
    unsigned char bytes[MAX_SIG_LEN]; // 시그니처 바이트 배열
    int len;                          // 시그니처 길이
    char typeName[64];                // 파일 타입 이름 (예: PNG)
    int isComment;                    // 주석 여부 (1: 주석, 0: 유효)
} FileSignature;

// 전역 변수: 시그니처 목록과 개수
FileSignature signatures[MAX_SIGS];
int sig_count = 0;

// 16진수 문자열 파싱 함수 (예: "FF D8" -> {0xFF, 0xD8})
int parseHex(char *hexStr, unsigned char *outBytes) {
    int count = 0;
    char *token = strtok(hexStr, " ");
    while (token != NULL) {
        // # 문자가 섞여있다면 제거 (파싱 단계에서 처리)
        if (token[0] == '#') {
            // 첫 글자가 #이면 그 다음 글자부터 파싱하거나, 호출부에서 이미 처리됨
             // strtoul은 숫자가 아닌 문자를 만나면 멈추거나 변환하므로 주의
             // 여기서는 호출부에서 이미 # 처리를 한다고 가정하거나 단순 변환
             if(strlen(token) > 1) 
                 outBytes[count++] = (unsigned char)strtoul(token + 1, NULL, 16);
        } else {
            outBytes[count++] = (unsigned char)strtoul(token, NULL, 16);
        }
        token = strtok(NULL, " ");
    }
    return count;
}

// 시그니처 파일 로드 함수
void loadSignatures(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("file_sig.data open error");
        exit(1);
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // 개행 문자 제거
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        if (sig_count >= MAX_SIGS) break;

        FileSignature *sig = &signatures[sig_count];
        sig->isComment = (line[0] == '#'); // #으로 시작하면 주석 처리

        // '|' 구분자 찾기
        char *delimiter = strchr(line, '|');
        if (delimiter) {
            *delimiter = 0; // 문자열 분리
            char *hexPart = line;
            char *namePart = delimiter + 1;

            // 시그니처 타입 이름 복사
            strncpy(sig->typeName, namePart, sizeof(sig->typeName) - 1);
            
            // 시그니처 바이트 파싱 (주석인 경우 # 제외하고 파싱하기 위해 처리 필요할 수 있음)
            // 과제 예시상 #도 시그니처 데이터 자체에는 포함되나 "무시"하는 것.
            // 여기서는 파싱은 하되 검사시 isComment 플래그로 스킵함.
            sig->len = parseHex(hexPart, sig->bytes);
            
            sig_count++;
        }
    }
    fclose(fp);
}

// 사용법 및 로드된 정보 출력
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

// 단일 파일 검사 함수
void checkFile(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return; // 읽기 실패시 무시

    unsigned char buffer[MAX_SIG_LEN];
    size_t bytesRead = fread(buffer, 1, MAX_SIG_LEN, fp);
    fclose(fp);

    for (int i = 0; i < sig_count; i++) {
        if (signatures[i].isComment) continue; // 주석은 무시

        // 파일 크기가 시그니처보다 작으면 패스
        if (bytesRead < signatures[i].len) continue;

        // 바이트 비교
        int match = 1;
        for (int j = 0; j < signatures[i].len; j++) {
            if (buffer[j] != signatures[i].bytes[j]) {
                match = 0;
                break;
            }
        }

        if (match) {
            printf("File type of %s is %s.\n", filepath, signatures[i].typeName);
            return; // 매칭되면 종료
        }
    }
}

// 경로 처리 함수 (파일이면 검사, 디렉토리면 재귀 탐색)
void processPath(const char *basePath) {
    struct stat st;
    
    // 파일/폴더 정보 확인
    if (lstat(basePath, &st) == -1) {
        return;
    }

    // 디렉토리인 경우
    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(basePath);
        if (!dir) return;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            // 현재(.)와 상위(..) 디렉토리는 무시
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            // 전체 경로 생성
            char newPath[MAX_PATH];
            snprintf(newPath, sizeof(newPath), "%s/%s", basePath, entry->d_name);

            // 재귀 호출
            processPath(newPath);
        }
        closedir(dir);
    } 
    // 일반 파일인 경우
    else if (S_ISREG(st.st_mode)) {
        checkFile(basePath);
    }
}

int main(int argc, char *argv[]) {
    loadSignatures("file_sig.data");

    // 인자가 없거나 잘못된 경로일 경우 체크를 위한 stat
    struct stat st;
    int pathExists = 0;
    if (argc >= 2 && lstat(argv[1], &st) == 0) {
        pathExists = 1;
    }

    // 인자가 없거나 존재하지 않는 경로면 Usage 출력
    if (argc < 2 || !pathExists) {
        printUsageAndInfo(argv[0]);
        return 0;
    }

    // 정상 실행 시 헤더 정보 출력 (Usage 라인 제외)
    // 문제 예시에 따르면 filesig_length 라인은 항상 출력됨 [cite: 13]
    printf("filesig_length = %d :", sig_count);
    for (int i = 0; i < sig_count; i++) {
        if (!signatures[i].isComment) {
            printf(" [%s]", signatures[i].typeName);
        }
    }
    printf("\n");

    // 경로 탐색 시작
    processPath(argv[1]);

    return 0;
}