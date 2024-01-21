#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

const int maxInputLength = 5000;    //input 숫자 최대 길이
int *inputList = NULL;  // 입력 리스트
int vaLength;   //가상주소길이(bits)
int pageSize;   //페이지(프레임) 크기(KB)
int frameNumber; // 프레임 개수 (ex. 32KB / 4KB = 8개)
int prAlgorithm;    //페이지 교체 알고리즘(1.Optimal 2.FIFO 3.LRU 4.Second-Chance)
char* ipFileName;   //입력파일명
char* opFileName;   //출력파일명
int cntFaults = 0;  //page fault 개수
char totalFaults[50];   //cntFaults 문자 포맷팅(천 단위로 쉼표 추가)
bool faultFlag = false; //page fault 발생 시 true
int pageList[100];  //pageList[frameNumber] = pageNumber

void userInput();
void generateRandomInput(int);
void setOpFileName();
void readData();
void writeData();
int simulate(int, int);
int simulateOptimal(int, int);
int simulateFIFO(int);
int simulateLRU(int);
int simulateSecondChance(int);
void outputTotalFaults();

//Optimal용
int optCount();
int optUpdate(int);
void optOutput();
int optDist[100];   // 각 페이지가 후에 재사용될 때까지의 거리
int optFifo[100];   // 후보 여러 개일 때 FIFO로 판단
int optFifoCnt = 1; // FIFO 카운터

//FIFO용
typedef struct {
    int items[5003];
    int front;
    int rear;
} Queue;
Queue q = {.front = 0, .rear = 0};
void fifoInsert(int);
int fifoDelete();

//LRU용
typedef struct node {
    int data;
    struct node *next;
} Stack;
Stack *lruHead = NULL;
void lruInsert(int);
void lruUpdate(int);

//Second-Chance용
typedef struct cq {
    int data;
    bool refBit;
    struct cq *next;
} Circular;
Circular *scHead = NULL;
Circular *vtm = NULL;   // 순환 큐의 현재 위치 표시
void scInsert(int);
void scRefOn(int);
int scUpdate();

int main() {
    userInput();    // 사용자 입력 처리
    setOpFileName();    // 출력 파일 이름 설정
    memset(pageList, -1, sizeof(pageList)); //페이지 리스트 초기화
    readData();
    writeData();   // 알고리즘 수행
}

void userInput() {
    int ip;
    printf("A. Simulation에 사용할 가상주소 길이를 선택하시오 (1. 18bits 2. 19bits 3. 20bits): ");
    scanf("%d", &ip);
    vaLength = 17 + ip;

    printf("\nB. Simulation에 사용할 페이지(프레임)의 크기를 선택하시오 (1. 1KB 2. 2KB 3. 4KB): ");
    scanf("%d", &ip);
    if (ip == 1) pageSize = 1;
    else if (ip == 2) pageSize = 2;
    else if (ip == 3) pageSize = 4;

    printf("\nC. Simulation에 사용할 물리메모리의 크기를 선택하시오 (1. 32KB 2. 64KB): ");
    scanf("%d", &ip);
    int paSize = 32 * ip;
    frameNumber = paSize / pageSize;

    printf("\nD. Simulation에 적용할 Page Replacement 알고리즘을 선택하시오\n(1. Optimal 2. FIFO 3. LRU 4. Second-Chance): ");
    scanf("%d", &prAlgorithm);

    printf("\nE. 가상주소 스트링 입력방식을 선택하시오\n(1. input.in 자동 생성 2. 기존 파일 사용): ");
    scanf("%d", &ip);

    ipFileName = malloc(256 * sizeof(char));
    if (ip == 1) {  //input.in 자동 생성
        strcpy(ipFileName, "input.in");
        generateRandomInput(vaLength);
    } else if (ip == 2) {   //기존 파일 사용
        printf("\nF. 입력 파일 이름을 입력하시오: ");
        scanf("%s", ipFileName);
    }
}

void generateRandomInput(int vaLength) {
    FILE *inputFile = fopen(ipFileName, "w");
    if (inputFile == NULL) {
        printf("input.in 파일을 열 수 없습니다\n");
        return;
    }
    unsigned long maxValue = pow(2, vaLength) - 1;  // 최대값
    srand(time(NULL));
    for (int i = 0; i < maxInputLength; i++) {
        unsigned long va = rand() % (maxValue + 1);     // 0~최대값 사이 랜덤 숫자 생성
        fprintf(inputFile, "%lu\n", va);
    }
    fclose(inputFile);
}

void setOpFileName() {
    opFileName = malloc(256 * sizeof(char));
    switch(prAlgorithm) {
        case 1:
            strcpy(opFileName, "output.opt");
            break;
        case 2:
            strcpy(opFileName, "output.fifo");
            break;
        case 3:
            strcpy(opFileName, "output.lru");
            break;
        case 4:
            strcpy(opFileName, "output.sc");
            break;
    }
}

void readData() {
    FILE *inputFile = fopen(ipFileName, "r");
    if (inputFile == NULL) {
        printf("파일을 열 수 없습니다\n");
        return;
    }
    // 파일에서 숫자 읽어서 inputList에 저장하기
    inputList = (int *)malloc(maxInputLength * sizeof(int));
    for (int i = 0; i < maxInputLength; i++) {
        fscanf(inputFile, "%d", &inputList[i]);
    }
    fclose(inputFile);
    free(ipFileName);
}
void writeData() {
    FILE *outputFile = fopen(opFileName, "w");
    if (outputFile == NULL) {
        printf("파일을 열 수 없습니다\n");
        return;
    }
    fprintf(outputFile, "%-10s%-14s%-14s%-14s%-14s%-10s\n","No.", "V.A.", "Page No.", "Frame No.", "P.A.", "Page Fault");
    int VA = 0, PageNo = 0, FrameNo = 0, PA = 0;
    char Fault = 'F';
    for (int i = 0; i < maxInputLength; i++) {
        VA = inputList[i];
        PageNo = VA / (1024 * pageSize);
        FrameNo = simulate(PageNo, i);
        int offset = VA % (1024 * pageSize);
        PA = (FrameNo * (1024 * pageSize)) + offset;
        if (faultFlag == true) Fault = 'F';
        else Fault = 'H';
        fprintf(outputFile, "%-10d%-14d%-14d%-14d%-14d%-10c\n", i + 1, VA, PageNo, FrameNo, PA, Fault);
    }
    outputTotalFaults();
    fprintf(outputFile, "Total Number of Page Faults: %s", totalFaults);

    // 동적 메모리 및 파일 포인터 해제
    fclose(outputFile);
    free(inputList);
    free(opFileName);
    if (prAlgorithm == 3) free(lruHead);
    if (prAlgorithm == 4) free(scHead);
}

int simulate(int pageNo, int curPos) {
    // 각 page replacement 알고리즘에 맞는 pageNo에 대한 frameNo 반환
    switch(prAlgorithm) {
        case 1:
            return simulateOptimal(pageNo, curPos);
            break;
        case 2:
            return simulateFIFO(pageNo);
            break;
        case 3:
            return simulateLRU(pageNo);
            break;
        case 4:
            return simulateSecondChance(pageNo);
            break;
    }
    return -1;
}

int optCount() {
    int cnt = 0, maxDist = 0, ret = 0, noRet = 0;  
    // 재사용된 페이지 개수, 재사용 거리 최댓값, 최댓값일 때의 페이지 번호, 재사용되지 않은 페이지 번호
    for (int i = 0; i < frameNumber; i++) {
        if (optDist[i] == -1) {
            noRet = i;
            continue;
        }
        cnt++;
        if (maxDist < optDist[i]) {
            maxDist = optDist[i];
            ret = i;
        }
    }
    if (cnt == frameNumber) return ret;      // 모두 재사용됐을 경우
    if (cnt == frameNumber - 1) return noRet;    // 하나만 재사용되지 않았을 경우
    return -1; // 재사용되지 않은 페이지가 여러 개인 경우
}

int optUpdate(int curPos) {
    for (int i = 0; i < frameNumber; i++) {  // 재사용 거리 초기화
        optDist[i] = -1;
    }
    int PA = 0;
    for (int i = 0; i < frameNumber; i++) {
        for (int j = curPos + 1; j < maxInputLength; j++) {
            PA = inputList[j] / (1024 * pageSize);  // 가상 주소 -> 페이지 번호 변환
            if (pageList[i] == PA) {
                optDist[i] = j;     // 얼마나 멀리 떨어져있는지 거리 저장
                break;
            }
        }
    }
    int tmp = optCount();   // 미래에 재사용되는게 몇 개인지
    if (tmp != -1) return tmp;  // 모두 재사용됐거나 하나만 재사용되지 않았을 경우 리턴

    // 재사용되지 않은 페이지가 여러 개일 경우 (FIFO 적용)
    int minCnt = 1e9;
    int ret = 0;
    for (int i = 0; i < frameNumber; i++) {
        if (optDist[i] == -1 && minCnt > optFifo[i]) {
            minCnt = optFifo[i];
            ret = i;
        }
    }
    return ret;
}

int simulateOptimal(int pageNo, int curPos) {
    faultFlag = true;
    cntFaults++;
    for (int i = 0; i < frameNumber; i++) {
        if (pageList[i] == -1) {    // 빈 프레임이 있을 경우 저장
            pageList[i] = pageNo;
            optFifo[i] = optFifoCnt++;
            return i;
        } else if (pageList[i] == pageNo) {     // 이미 프레임에 존재할 경우
            faultFlag = false;
            cntFaults--;
            optFifo[i] = optFifoCnt++;
            return i;
        }
    }
    int victim = optUpdate(curPos);
    pageList[victim] = pageNo;
    optFifo[victim] = optFifoCnt++;
    return victim;
}

void fifoInsert(int item) {
    if ((q.rear + 1) % 5003 == q.front) {
        return;
    }
    q.rear = (q.rear + 1) % 5003;
    q.items[q.rear] = item;
}
int fifoDelete() {
    if (q.front == q.rear) return -1;
    q.front = (q.front + 1) % 5003;
    return q.items[q.front];
}

int simulateFIFO(int pageNo) {
    faultFlag = true;
    cntFaults++;
    for (int i = 0; i < frameNumber; i++) {
        if (pageList[i] == -1) {    // 빈 프레임 존재
            pageList[i] = pageNo;
            fifoInsert(i);
            return i;
        }
        else if (pageList[i] == pageNo) {   // 이미 프레임에 존재
            faultFlag = false;
            cntFaults--;
            return i;
        }
    }
    int front = fifoDelete();
    pageList[front] = pageNo;
    fifoInsert(front);
    return front;
}

void lruInsert(int data) {
    Stack *newNode = (Stack*)malloc(sizeof(Stack));
    newNode -> data = data;
    newNode -> next = NULL;

    if (lruHead == NULL) {
        lruHead = newNode;
    } else {
        Stack *last = lruHead;
        while (last -> next != NULL) last = last -> next;
        last -> next = newNode;
    }
}

void lruUpdate(int data) {
    Stack *prev = NULL, *cur = lruHead;
    while(cur != NULL) {    //Stack에 data 존재하는지 탐색
        if (cur->data == data) {    //존재한다면
            faultFlag = false;
            if (cur -> next == NULL) return;    //이미 마지막 노드
            break;
        }
        prev = cur;
        cur = cur -> next;
    }
    
    if (cur != NULL) {  //Stack에 데이터 존재
        if (prev != NULL) prev -> next = cur -> next;   //Stack에서 제거
        else lruHead = cur -> next;    //data가 첫 번째 노드인 경우
    }
    lruInsert(data);   //Stack 마지막에 삽입
}

int simulateLRU(int pageNo) {
    faultFlag = true;
    cntFaults++;
    for (int i = 0; i < frameNumber; i++) {
        if (pageList[i] == -1) {
            lruInsert(i);
            pageList[i] = pageNo;
            return i;
        } else if (pageList[i] == pageNo) {
            faultFlag = false;
            cntFaults--;
            lruUpdate(i);
            return i;
        }
    }
    int front = lruHead -> data;
    pageList[front] = pageNo;
    lruInsert(front);
    lruHead = lruHead -> next;
    return front;
}

void scInsert(int data) {
    Circular *newNode = (Circular*)malloc(sizeof(Circular));
    newNode -> data = data;
    newNode -> refBit = false;

    if (scHead == NULL) {
        scHead = newNode;
        newNode -> next = scHead;
    } else {
        Circular *last = scHead;
        while (last -> next != scHead) last = last -> next;
        last -> next = newNode;
        newNode -> next = scHead;
    }
}

void scRefOn(int data) {
    Circular *cur = scHead;
    while (cur != NULL) {
        if (cur -> data == data) {
            cur -> refBit = true;
            return;
        }
        cur = cur -> next;
    }
}

int scUpdate() {
    if (scHead == NULL) return -1;
    Circular *start = vtm;
    while(1) {
        if (vtm == NULL) vtm = scHead;
        if (!(vtm -> refBit)) {
            int victimData = vtm -> data;
            vtm = vtm -> next;
            return victimData;
        }
        vtm -> refBit = false;
        vtm = vtm -> next;
    }
}

int simulateSecondChance(int pageNo) {
    faultFlag = true;
    cntFaults++;
    for (int i = 0; i < frameNumber; i++) {
        if (pageList[i] == -1) {
            scInsert(i);
            pageList[i] = pageNo;
            return i;
        } else if (pageList[i] == pageNo) {
            faultFlag = false;
            cntFaults--;
            scRefOn(i);     // 참조했으니 reference bit = 1로 업데이트
            return i;
        }
    }
    int victim = scUpdate();
    if (victim != -1) pageList[victim] = pageNo;    //교체할 페이지 찾음
    else scInsert(pageNo);  // scHead == NULL일 경우 그냥 삽입
    return victim;
}

void reverse(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char tmp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = tmp;
    }
}
void outputTotalFaults() {
    char tmp[50];
    sprintf(tmp, "%d", cntFaults);
    reverse(tmp);
    int len = strlen(tmp), idx = 0;
    for (int i = 0; i < len; i++) {
        totalFaults[idx++] = tmp[i];
        if (i % 3 == 2 && i != len - 1) {
            totalFaults[idx++] = ',';
        }
    }
    totalFaults[idx] = '\0';
    reverse(totalFaults);
}