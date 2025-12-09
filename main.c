/*
 * 시스템 프로그래밍 과제: 파일 시그니처 기반 파일 타입 판별 프로그램
 *
 * 설명:
 * 이 프로그램은 사용자가 입력한 파일 또는 디렉토리를 탐색하여,
 * 미리 정의된 시그니처(Magic Number) 데이터베이스(file_sig.data)와 대조해
 * 파일의 타입을 식별합니다.
 *
 * 주요 기능:
 * 1. file_sig.data 파일 파싱 및 시그니처 메모리 적재
 * 2. 재귀적 디렉토리 탐색 (Recursive Directory Traversal)
 * 3. 16진수 시그니처 패턴 매칭 (Sliding Window 기법 적용)
 *
 * 작성 환경: Ubuntu Linux (GCC)
 */

 #include <stdio.h>      // 표준 입출력 (fopen, printf, etc.)
 #include <stdlib.h>     // 일반 유틸리티 (malloc, exit, strtoul)
 #include <string.h>     // 문자열 처리 (strtok, strncpy, strcmp)
 #include <ctype.h>      // 문자 유형 검사
 #include <dirent.h>     // [System Call] 디렉토리 처리 (opendir, readdir)
 #include <sys/stat.h>   // [System Call] 파일 상태 확인 (lstat, S_ISDIR)
 #include <unistd.h>     // UNIX 표준 시스템 콜
 
 // 상수 정의
 #define MAX_SIG_LEN 128     // 시그니처의 최대 바이트 길이
 #define MAX_SIGS 100        // 등록 가능한 최대 시그니처 개수
 #define MAX_PATH 4096       // 리눅스 파일 시스템의 최대 경로 길이
 
 // 파일 앞부분을 읽어들일 버퍼 크기.
 // MP4 등 일부 파일은 시그니처가 0번 offset이 아닌 4번 offset 등에 위치할 수 있으므로
 // 넉넉하게 1KB를 읽어서 탐색합니다.
 #define READ_BUFFER_SIZE 1024 
 
 // 파일 시그니처 정보를 구조체로 정의하여 관리
 typedef struct {
     unsigned char bytes[MAX_SIG_LEN]; // 시그니처 바이너리 데이터 (unsigned char 사용: 0~255 값 표현)
     int len;                          // 시그니처의 실제 바이트 길이
     char typeName[64];                // 파일 타입 명칭 (예: PNG, JPG)
     int isComment;                    // 주석 여부 플래그 (1: 주석, 0: 유효한 시그니처)
 } FileSignature;
 
 // 전역 변수: 시그니처 목록을 저장할 배열과 현재 개수
 FileSignature signatures[MAX_SIGS];
 int sig_count = 0;
 
 /*
  * 함수: parseHex
  * 목적: 공백으로 구분된 16진수 문자열을 바이트 배열로 변환합니다.
  * 예시: "FF D8" -> {0xFF, 0xD8}
  * 매개변수:
  * - hexStr: 파싱할 문자열
  * - outBytes: 결과가 저장될 바이트 배열
  * 반환값: 변환된 바이트 수
  */
 int parseHex(char *hexStr, unsigned char *outBytes) {
     int count = 0;
     // strtok를 사용하여 공백(" ")을 기준으로 토큰 분리
     char *token = strtok(hexStr, " ");
     while (token != NULL) {
         // '#'으로 시작하는 토큰(주석) 처리 로직
         // 예: "#7F" 같은 경우 '#'을 제외하고 숫자만 파싱
         if (token[0] == '#') {
              if(strlen(token) > 1) 
                  // strtoul: 문자열을 unsigned long 정수로 변환 (16진수 모드)
                  outBytes[count++] = (unsigned char)strtoul(token + 1, NULL, 16);
         } else {
             outBytes[count++] = (unsigned char)strtoul(token, NULL, 16);
         }
         token = strtok(NULL, " "); // 다음 토큰으로 이동
     }
     return count;
 }
 
 /*
  * 함수: loadSignatures
  * 목적: file_sig.data 파일을 읽어 프로그램 메모리에 시그니처 정보를 로드합니다.
  */
 void loadSignatures(const char *filename) {
     FILE *fp = fopen(filename, "r");
     if (!fp) {
         perror("file_sig.data open error"); // 파일 열기 실패 시 에러 출력
         exit(1); // 프로그램 비정상 종료
     }
 
     char line[512];
     // 파일의 끝까지 한 줄씩 읽음
     while (fgets(line, sizeof(line), fp)) {
         // 문자열 끝의 개행 문자(\n) 제거
         line[strcspn(line, "\n")] = 0;
         
         // 빈 줄은 무시
         if (strlen(line) == 0) continue;
 
         // 최대 시그니처 개수 초과 시 중단
         if (sig_count >= MAX_SIGS) break;
 
         FileSignature *sig = &signatures[sig_count];
         
         // 줄의 첫 글자가 '#'이면 주석 처리된 시그니처로 표시
         sig->isComment = (line[0] == '#');
 
         // 파이프('|') 문자를 기준으로 시그니처(Hex)와 타입이름 분리
         char *delimiter = strchr(line, '|');
         if (delimiter) {
             *delimiter = 0; // '|' 위치에 NULL을 넣어 문자열을 두 개로 나눔
             char *hexPart = line;             // 앞부분: Hex String
             char *namePart = delimiter + 1;   // 뒷부분: Type Name
 
             // 타입 이름 복사
             strncpy(sig->typeName, namePart, sizeof(sig->typeName) - 1);
             
             // Hex 문자열을 바이트 배열로 변환하여 저장
             sig->len = parseHex(hexPart, sig->bytes);
             
             sig_count++; // 등록된 시그니처 개수 증가
         }
     }
     fclose(fp);
 }
 
 /*
  * 함수: printUsageAndInfo
  * 목적: 프로그램 사용법 및 로드된 시그니처 정보를 출력합니다.
  * (과제 요구사항에 따른 출력 포맷 준수)
  */
 void printUsageAndInfo(const char *progName) {
     // filesig_length 출력 (주석 포함 전체 라인 수 혹은 로드된 수)
     printf("filesig_length = %d :", sig_count);
     
     // 유효한(주석이 아닌) 시그니처 목록 출력
     for (int i = 0; i < sig_count; i++) {
         if (!signatures[i].isComment) {
             printf(" [%s]", signatures[i].typeName);
         }
     }
     printf("\n");
     // 사용법 안내 메시지 출력
     printf("Usage: %s (filename | dirname)\n", progName);
 }
 
 /*
  * 함수: checkFile
  * 목적: 단일 파일을 열어 시그니처 패턴이 존재하는지 검사합니다.
  * 알고리즘: Sliding Window 기법
  * - 파일 헤더가 정확히 0번지부터 시작하지 않는 경우(예: MP4의 offset 4)를
  * 대비하여 버퍼 내에서 패턴을 이동하며 비교합니다.
  */
 void checkFile(const char *filepath) {
     // 파일을 바이너리 읽기 모드("rb")로 열기
     FILE *fp = fopen(filepath, "rb");
     if (!fp) return; // 파일 열기 실패 시 조용히 리턴 (권한 문제 등)
 
     unsigned char buffer[READ_BUFFER_SIZE];
     // 정의된 버퍼 크기만큼 파일 앞부분을 읽어옴
     size_t bytesRead = fread(buffer, 1, READ_BUFFER_SIZE, fp);
     fclose(fp); // 파일 처리가 끝났으므로 닫음
 
     // 모든 시그니처에 대해 반복 검사
     for (int i = 0; i < sig_count; i++) {
         if (signatures[i].isComment) continue; // 주석 처리된 시그니처는 검사 제외
         
         // 읽어온 파일 내용이 시그니처 길이보다 짧으면 비교 불가 -> 패스
         if (bytesRead < signatures[i].len) continue;
 
         // [패턴 매칭] 슬라이딩 윈도우 검색
         // offset: 버퍼 내에서 비교를 시작할 위치
         int found = 0;
         for (int offset = 0; offset <= bytesRead - signatures[i].len; offset++) {
             int match = 1;
             // 시그니처의 모든 바이트가 일치하는지 확인
             for (int j = 0; j < signatures[i].len; j++) {
                 if (buffer[offset + j] != signatures[i].bytes[j]) {
                     match = 0; // 불일치 발생 시 즉시 중단
                     break;
                 }
             }
             if (match) {
                 found = 1;
                 break; // 패턴 발견 시 오프셋 루프 탈출
             }
         }
 
         // 일치하는 시그니처를 찾은 경우 결과 출력 후 함수 종료
         if (found) {
             printf("File type of %s is %s.\n", filepath, signatures[i].typeName);
             return; // 하나의 파일은 하나의 타입으로만 판별하고 종료
         }
     }
 }
 
 /*
  * 함수: processPath
  * 목적: 입력된 경로가 파일인지 디렉토리인지 판별하고 적절한 동작을 수행합니다.
  * 디렉토리일 경우 재귀적으로 하위 내용을 탐색합니다.
  */
 void processPath(const char *basePath) {
     struct stat st;
     
     // lstat: 파일의 속성을 가져옴 (심볼릭 링크 자체의 속성 확인)
     if (lstat(basePath, &st) == -1) return;
 
     // 1. 디렉토리인 경우 (S_ISDIR 매크로 사용)
     if (S_ISDIR(st.st_mode)) {
         DIR *dir = opendir(basePath); // 디렉토리 열기
         if (!dir) return;
 
         struct dirent *entry;
         // 디렉토리 내의 모든 항목을 하나씩 읽음
         while ((entry = readdir(dir)) != NULL) {
             // 현재 디렉토리(.)와 상위 디렉토리(..)는 무한 루프 방지를 위해 제외
             if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                 continue;
 
             // 전체 경로 생성 (부모경로/파일명)
             char newPath[MAX_PATH];
             snprintf(newPath, sizeof(newPath), "%s/%s", basePath, entry->d_name);
             
             // 재귀 호출 (Recursive Call) 탐색
             processPath(newPath);
         }
         closedir(dir); // 디렉토리 닫기
     } 
     // 2. 일반 파일인 경우 (S_ISREG 매크로 사용)
     else if (S_ISREG(st.st_mode)) {
         checkFile(basePath); // 파일 시그니처 검사 수행
     }
 }
 
 int main(int argc, char *argv[]) {
     // 1. 시그니처 데이터 로드
     loadSignatures("file_sig.data");
 
     // 2. 명령행 인자 검증
     struct stat st;
     int pathExists = 0;
     // 인자가 있고, 해당 경로가 실제 존재하는지 확인
     if (argc >= 2 && lstat(argv[1], &st) == 0) {
         pathExists = 1;
     }
 
     // 인자가 없거나 경로가 유효하지 않으면 Usage 출력 후 종료
     if (argc < 2 || !pathExists) {
         printUsageAndInfo(argv[0]);
         return 0;
     }
 
     // 3. 실행 헤더 정보 출력
     // 과제 예시와 동일하게 시그니처 리스트를 먼저 출력함
     printf("filesig_length = %d :", sig_count);
     for (int i = 0; i < sig_count; i++) {
         if (!signatures[i].isComment) {
             printf(" [%s]", signatures[i].typeName);
         }
     }
     printf("\n");
 
     // 4. 경로 탐색 및 파일 타입 판별 시작
     processPath(argv[1]);
 
     return 0;
 }