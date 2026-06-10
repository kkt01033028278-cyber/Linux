#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_PROCESSES 2

// 프로세스 명부 구조체
typedef struct {
    const char *name;       // 프로세스 이름 (로그용)
    const char *path;       // 실행 파일 경로
    pid_t pid;              // 현재 PID
    int restart_count;      // 부활 횟수
} ProcessInfo;

// 감시할 자식 프로세스 명부 등록
ProcessInfo process_list[MAX_PROCESSES] = {
    {"Worker_A (Normal)", "./worker_a", 0, 0},
    {"Worker_B (Target)", "./worker_b", 0, 0}
};

// 자식 프로세스를 낳고(fork) 실행(execl)하는 헬퍼 함수
pid_t spawn_process(const char *path) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // 자식 프로세스의 공간: 자신을 새로운 프로그램으로 덮어씌움
        execl(path, path, (char *)NULL);
        
        // execl이 실패했을 경우에만 아래 코드가 실행됨
        perror("실행 파일 호출 실패 (경로 확인 필요)"); 
        exit(1); 
    }
    
    // 부모 프로세스의 공간: 자식의 PID를 반환
    return pid;
}

int main() {
    // 1. 방어막 전개: 터미널 종료(SIGHUP) 및 Ctrl+C(SIGINT) 방어
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    printf("[OS_CORE] Watchdog 데몬 부팅 완료. 자식 프로세스 가동을 시작합니다.\n");
    printf("==============================================================\n");

    // 2. 초기 프로세스 가동 (명부를 순회하며 모두 실행)
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_list[i].pid = spawn_process(process_list[i].path);
        printf("[OS_CORE] %s 가동 성공 (PID: %d)\n", process_list[i].name, process_list[i].pid);
    }

    // 3. 무한 감시 루프 (프로세스 스케줄러의 핵심)
    while (1) {
        int status;
        // 누군가 죽을 때까지 커널 단에서 대기 (CPU 점유율 0%)
        pid_t died_pid = wait(&status); 

        if (died_pid > 0) {
            // 누가 죽었는지 명부에서 탐색
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (process_list[i].pid == died_pid) {
                    process_list[i].restart_count++;
                    
                    printf("\n[ALERT] 🚨 %s (PID: %d) 비정상 종료 감지!\n", process_list[i].name, died_pid);
                    printf("[ALERT] 누적 복구 횟수: %d회. 즉시 재시작을 시도합니다.\n", process_list[i].restart_count);

                    // 새 생명 부여 (다시 fork)
                    process_list[i].pid = spawn_process(process_list[i].path);
                    printf("[RECOVERY] ♻️ %s 복구 완료. (새 PID: %d)\n", process_list[i].name, process_list[i].pid);
                    break;
                }
            }
        }
    }
    return 0;
}
