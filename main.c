#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <pigpio.h>
#include "postfix.h"
#include "findroot.h"

#define NUM_THREADS 3
#define ROWS 7
#define COLS 4
#define MAX 100

int rowPins[ROWS] = {17, 18, 27, 22, 23, 24, 25};
int colPins[COLS] = {8, 7, 1, 4};

char keymap[ROWS][COLS] = {
    {'1', '2', '3', '+'},
    {'4', '5', '6', '-'},
    {'7', '8', '9', '*'},
    {'0', '.', '/', '^'},
    {'(', ')', 'x', 'E'},
    {'B', '\0', '\0', '\0'},
    {'\0', '\0', '\0', '\0'}
};
//(x^14-3*x^12+7*x^9)-(5*x^8+2*x^6)+(4*x^5-11*x^3+6*x^2)-(20*x-50)
int found = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER; // Thêm biến điều kiện
float best_result = 0.0;

typedef struct {
    Token *postfix;
    float result;
} ThreadData;

char scanKeypad() {
    for (int r = 0; r < ROWS; r++) {
        gpioWrite(rowPins[r], PI_LOW);
        for (int c = 0; c < COLS; c++) {
            if (gpioRead(colPins[c]) == PI_LOW) {
                while (gpioRead(colPins[c]) == PI_LOW) time_sleep(0.01);
                gpioWrite(rowPins[r], PI_HIGH);
                return keymap[r][c];
            }
        }
        gpioWrite(rowPins[r], PI_HIGH);
    }
    return '\0';
}

void *findrootNewton(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    data->result = newtonRaphson(data->postfix);

    pthread_mutex_lock(&mutex);
    if (!found && !isnan(data->result)) {
        float fx = evaluatePostfix(data->postfix, data->result);
        if (fabs(fx) < 1e-4) {
            best_result = data->result;
            found = 1;
            printf("Newton-Raphson tìm được nghiệm: %f\n", best_result);
            pthread_cond_signal(&cond); // Báo hiệu khi tìm được nghiệm
        }
    }
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}

void *findrootBisection(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    data->result = bisectionMethod(data->postfix);

    pthread_mutex_lock(&mutex);
    if (!found && !isnan(data->result)) {
        float fx = evaluatePostfix(data->postfix, data->result);
        if (fabs(fx) < 1e-4) {
            best_result = data->result;
            found = 1;
            printf("Bisection tìm được nghiệm: %f\n", best_result);
            pthread_cond_signal(&cond);
        }
    }
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}

void *findrootSecant(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    data->result = secantMethod(data->postfix);

    pthread_mutex_lock(&mutex);
    if (!found && !isnan(data->result)) {
        float fx = evaluatePostfix(data->postfix, data->result);
        if (fabs(fx) < 1e-4) {
            best_result = data->result;
            found = 1;
            printf("Secant tìm được nghiệm: %f\n", best_result);
            pthread_cond_signal(&cond);
        }
    }
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}

int main() {
    struct timespec start, end;
    Token *output;
    char str[MAX];
    int idx = 0;
    char ch;

    if (gpioInitialise() < 0) {
        printf("Lỗi: Không thể khởi tạo pigpio!\n");
        return 1;
    }

    for (int i = 0; i < ROWS; i++) {
        gpioSetMode(rowPins[i], PI_OUTPUT);
        gpioWrite(rowPins[i], PI_HIGH);
    }
    for (int i = 0; i < COLS; i++) {
        gpioSetMode(colPins[i], PI_INPUT);
        gpioSetPullUpDown(colPins[i], PI_PUD_UP);
    }

    printf("Nhập biểu thức bằng bàn phím 7x4 (ấn 'E' để xác nhận, 'B' để xóa ký tự cuối):\n");
    printf("Hiện tại: ");
    str[0] = '\0';

    while (1) {
        ch = scanKeypad();
        if (ch == '\0') continue;
        if (ch == 'E') {
            str[idx] = '\0';
            printf("\n");
            break;
        } else if (ch == 'B') {
            if (idx > 0) {
                idx--;
                str[idx] = '\0';
                printf("\rHiện tại: %s  ", str);
                fflush(stdout);
            }
        } else if (idx < MAX - 1) {
            str[idx++] = ch;
            str[idx] = '\0';
            printf("\rHiện tại: %s", str);
            fflush(stdout);
        }
    }

    output = infixToPostfix(str);
    if (output != NULL) {
        printTokens(output);

        pthread_t threads[NUM_THREADS];
        ThreadData threadData[NUM_THREADS];

        clock_gettime(CLOCK_MONOTONIC, &start);

        threadData[0].postfix = output;
        pthread_create(&threads[0], NULL, findrootNewton, (void *)&threadData[0]);
        threadData[1].postfix = output;
        pthread_create(&threads[1], NULL, findrootBisection, (void *)&threadData[1]);
        threadData[2].postfix = output;
        pthread_create(&threads[2], NULL, findrootSecant, (void *)&threadData[2]);

        // Chờ nghiệm đầu tiên với condition variable
        pthread_mutex_lock(&mutex);
        while (!found) {
            pthread_cond_wait(&cond, &mutex); // Chờ tín hiệu từ luồng
        }
        pthread_mutex_unlock(&mutex);

        // In kết quả ngay khi có nghiệm
        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        printf("Thời gian tìm nghiệm: %f giây\n", elapsed);

        float fx = evaluatePostfix(output, best_result);
        printf("Kết quả với nghiệm %.4f là: %.4f\n", best_result, fx);
        if (fabs(fx) > 1e-4) {
            printf("Cảnh báo: Giá trị tại nghiệm không đủ gần 0 (có thể không phải nghiệm chính xác)!\n");
        }

        // Hủy các luồng còn lại
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_cancel(threads[i]);
        }

        free(output);
    } else {
        printf("Lỗi khi chuyển đổi biểu thức!\n");
    }

    gpioTerminate();
    return 0;
}
