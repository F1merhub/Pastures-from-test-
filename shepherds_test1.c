#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define DIR1 1
#define DIR2 2

typedef struct {
    int direction;
    int id;
    void* bridge_ptr;
} ShepherdData;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    
    bool hat_present;
    bool bridge_busy;
    int current_dir;
} Bridge;

void bridge_init(Bridge* bridge) { 
    pthread_mutex_init(&bridge->mutex, NULL);
    pthread_cond_init(&bridge->cond, NULL);
    bridge->hat_present = false;
    bridge->bridge_busy = false;
    bridge->current_dir = 0;
}

void bridge_destroy(Bridge* bridge) {
    pthread_mutex_destroy(&bridge->mutex);
    pthread_cond_destroy(&bridge->cond);
}

void dir1_cross(Bridge* bridge, int shepherd_id) {
    pthread_mutex_lock(&bridge->mutex);
    
    while (bridge->bridge_busy && bridge->current_dir == DIR2) {        // хотим пройти
        printf("DIR1-%d ждет: на мосту встречное стадо\n", shepherd_id);
        pthread_cond_wait(&bridge->cond, &bridge->mutex);  // ждет сигнал и захватывает
    }
    
    bridge->bridge_busy = true; // проверка в конце ли очереди и нужно ли оставлять шапку
    bridge->current_dir = DIR1;
    printf("DIR1-%d начинает переход. Мост занят DIR1\n", shepherd_id);
    pthread_mutex_unlock(&bridge->mutex);
    
    printf("DIR1-%d: иду один через мост...\n", shepherd_id);
    usleep(rand() % 300000 + 100000);
    
    pthread_mutex_lock(&bridge->mutex);
    bridge->hat_present = true;
    printf("DIR1-%d оставил шапку на противоположном конце\n", shepherd_id);
    pthread_mutex_unlock(&bridge->mutex);
    
    printf("DIR1-%d: возвращаюсь за стадом...\n", shepherd_id);
    usleep(rand() % 200000 + 100000);
    
    printf("DIR1-%d: веду стадо через мост...\n", shepherd_id);
    usleep(rand() % 400000 + 200000);
    
    pthread_mutex_lock(&bridge->mutex);
    bridge->bridge_busy = false;
    bridge->current_dir = 0;
    bridge->hat_present = false;
    printf("DIR1-%d завершил переход и забирает шапку\n", shepherd_id);
    
    pthread_cond_broadcast(&bridge->cond);
    pthread_mutex_unlock(&bridge->mutex);
}

void dir2_cross(Bridge* bridge, int shepherd_id) {
    pthread_mutex_lock(&bridge->mutex);
    
    while (bridge->bridge_busy || bridge->hat_present) {
        if (bridge->hat_present) {
            printf("DIR2-%d ждет: видит шапку\n", shepherd_id);
        } else {
            printf("DIR2-%d ждет: на мосту другое стадо\n", shepherd_id);
        }
        pthread_cond_wait(&bridge->cond, &bridge->mutex);
    }
    
    bridge->bridge_busy = true;
    bridge->current_dir = DIR2;
    printf("DIR2-%d начинает переход. Мост занят DIR2\n", shepherd_id);
    pthread_mutex_unlock(&bridge->mutex);
    
    printf("DIR2-%d: веду стадо через мост...\n", shepherd_id);
    usleep(rand() % 500000 + 300000);
    
    pthread_mutex_lock(&bridge->mutex);
    bridge->bridge_busy = false;
    bridge->current_dir = 0;
    printf("DIR2-%d завершил переход\n", shepherd_id);
    
    pthread_cond_broadcast(&bridge->cond);
    pthread_mutex_unlock(&bridge->mutex);
}

void* shepherd_thread(void* arg) {
    ShepherdData* data = (ShepherdData*)arg;
    int direction = data->direction;
    int id = data->id;
    Bridge* bridge = (Bridge*)data->bridge_ptr;
    
    usleep(rand() % 1000000);
    
    if (direction == DIR1) {
        dir1_cross(bridge, id);
    } else {
        dir2_cross(bridge, id);
    }
    
    free(data);
    return NULL;
}

int main() {
    srand(time(NULL));
    
    Bridge bridge;
    bridge_init(&bridge);
    
    int num_shepherds = 10;
    pthread_t threads[num_shepherds];
    ShepherdData* data_array[num_shepherds];
    
    printf("Начало моделирования\n\n");
    
    for (int i = 0; i < num_shepherds; i++) {
        ShepherdData* data = malloc(sizeof(ShepherdData));
        data->direction = (rand() % 2 == 0) ? DIR1 : DIR2;
        data->id = i + 1;
        data->bridge_ptr = &bridge;
        data_array[i] = data;
        
        if (pthread_create(&threads[i], NULL, shepherd_thread, data) != 0) {
            perror("Ошибка создания потока");
            free(data);
            return 1;
        }
    }
    
    for (int i = 0; i < num_shepherds; i++) {
        pthread_join(threads[i], NULL);
    }
    
    for (int i = 0; i < num_shepherds; i++) {
        free(data_array[i]);
    }
    
    printf("\nВсе стада перешли через мост\n");
    
    bridge_destroy(&bridge);
    return 0;
}