#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define DIR1 1
#define DIR2 2
#define MAX_THREADS 30

typedef struct {
    int queue[MAX_THREADS];
    int size;
} DirectionQueue;

typedef struct {
    int direction;
    int id;
    void* bridge_ptr;
} ShepherdData;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    
    // int* queue_array;
    int queue_array[MAX_THREADS];
    int hat_present;
    bool bridge_busy;
    int current_dir;

    DirectionQueue dir1_queue;
    DirectionQueue dir2_queue;
} Bridge;

void print_queue(Bridge* bridge, int direction, int current_id) {
    DirectionQueue* queue;
    const char* dir_name;
    
    if (direction == DIR1) {
        queue = &bridge->dir1_queue;
        dir_name = "DIR1";
    } else {
        queue = &bridge->dir2_queue;
        dir_name = "DIR2";
    }
    
    printf("%s-%d: Очередь %s[%d]: ", dir_name, current_id, dir_name, queue->size);
    
    if (queue->size == 0) {
        printf("пуста");
    } else {
        for (int i = 0; i < queue->size; i++) {
            if (queue->queue[i] == current_id) {
                printf("[%d]* ", queue->queue[i]);  // * отмечает текущий поток
            } else {
                printf("%d ", queue->queue[i]);
            }
        }
        
        // Показываем, кто первый
        if (queue->size > 0) {
            printf(" (первый: %d)", queue->queue[0]);
        }
    }
    
    printf("\n");
}


void queue_init(DirectionQueue* q) {
    q->size = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        q->queue[i] = -1;
    }
}

void queue_add(DirectionQueue* q, int thread_id) {
    if (q->size < MAX_THREADS) {
        q->queue[q->size] = thread_id;
        q->size++;
    }
}

void queue_remove_front(DirectionQueue* q) {
    if (q->size <= 0) {
        return;  
    }
    
    for (int i = 0; i < q->size - 1; i++) {
        q->queue[i] = q->queue[i + 1];
    }
    
    q->queue[q->size - 1] = -1;
    q->size--;
}



void bridge_init(Bridge* bridge) {   // TODO переименовать бридж
    pthread_mutex_init(&bridge->mutex, NULL);
    pthread_cond_init(&bridge->cond, NULL);
    bridge->hat_present = 0;
    bridge->bridge_busy = false;
    bridge->current_dir = 0;
}

void bridge_destroy(Bridge* bridge) {
    pthread_mutex_destroy(&bridge->mutex);
    pthread_cond_destroy(&bridge->cond);
}

void dir1_cross(Bridge* bridge, int shepherd_id) { 

    pthread_mutex_lock(&bridge->mutex);
    queue_add(&bridge->dir1_queue, shepherd_id);
    pthread_mutex_unlock(&bridge->mutex);
    printf("DIR1-%d встает в очередь\n", shepherd_id);  // оповещаем и добавляем в id очередь

    print_queue(bridge, DIR1, shepherd_id); // распечатка очереди

    while (bridge->dir1_queue.queue[0] != shepherd_id){        // в условии сказано, что пастух идет класть шапку, если перед входом никого нет, даже если кто то есть на мосту
        printf("DIR1-%d ждет около моста\n", shepherd_id);
        pthread_cond_wait(&bridge->cond, &bridge->mutex);  // ждет сигнал и захватывает
    }

    printf("DIR1-%d: иду один через мост...\n", shepherd_id);
    usleep(50000);
    if (bridge->hat_present == 0) { // использую две шапки
        bridge->hat_present = 1;
        printf("DIR1-%d Поставил шапку\n", shepherd_id);
    }
    else {
        printf("DIR1-%d Поставил вторую шапку\n", shepherd_id);
        bridge->hat_present = 2;
    }
    usleep(50000);

    while (bridge->bridge_busy) {        // теперь ждем освобождения моста 
        printf("DIR1-%d ждет: на мосту стадо\n", shepherd_id);
        pthread_cond_wait(&bridge->cond, &bridge->mutex);  
    }

    bridge->bridge_busy = true; 
    bridge->current_dir = DIR1;
    queue_remove_front(&bridge->dir1_queue);  // освобождаем место у входа и позволяем другому пастуху положить свою шапку и сами начинаем переход со стадом
    printf("DIR1-%d начинает переход. Мост занят DIR1\n", shepherd_id);
    pthread_mutex_unlock(&bridge->mutex);
    pthread_cond_broadcast(&bridge->cond);

    printf("DIR1-%d: веду стадо через мост...\n", shepherd_id);
    usleep(rand() % 400000 + 200000);
    
    pthread_mutex_lock(&bridge->mutex);
    bridge->bridge_busy = false;
    bridge->current_dir = 0;
    bridge->hat_present = bridge->hat_present - 1;  
    printf("DIR1-%d завершил переход и забирает шапку\n", shepherd_id);
    pthread_mutex_unlock(&bridge->mutex);

    pthread_cond_broadcast(&bridge->cond);

}
void dir2_cross(Bridge* bridge, int shepherd_id) {

    pthread_mutex_lock(&bridge->mutex);         
    queue_add(&bridge->dir2_queue, shepherd_id);
    pthread_mutex_unlock(&bridge->mutex);
    printf("DIR2-%d встает в очередь\n", shepherd_id);  

    print_queue(bridge, DIR2, shepherd_id); // распечатка очереди

    while (bridge->bridge_busy || bridge->hat_present || bridge->dir2_queue.queue[0] != shepherd_id) { // заходят, если нет шапок, мост не занят и они первые в очереди
        if (bridge->dir2_queue.queue[0] == shepherd_id) {
            if (bridge->hat_present)
                printf("DIR2-%d ждет: видит шапку\n", shepherd_id);
            else 
                printf("SOMETHING WENT WRONG"); // т.к. после прохода сдвигаются id
        } else {
            printf("DIR2-%d ждет свою очередь\n", shepherd_id);
        }
        pthread_cond_wait(&bridge->cond, &bridge->mutex);
    }
    
    bridge->bridge_busy = true;
    bridge->current_dir = DIR2;
    pthread_mutex_unlock(&bridge->mutex);
    
    printf("DIR2-%d начинает переход. Мост занят DIR2\n", shepherd_id);
    printf("DIR2-%d: веду стадо через мост...\n", shepherd_id);
    usleep(rand() % 500000 + 300000);
    
    pthread_mutex_lock(&bridge->mutex);
    bridge->bridge_busy = false;
    bridge->current_dir = 0;
    queue_remove_front(&bridge->dir2_queue);
    printf("DIR2-%d завершил переход\n", shepherd_id);
    pthread_mutex_unlock(&bridge->mutex);

    pthread_cond_broadcast(&bridge->cond);
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
    
    DirectionQueue queue1;
    DirectionQueue queue2;
    queue_init(&queue1);
    queue_init(&queue2);
    (&bridge)->dir1_queue = queue1;
    (&bridge)->dir2_queue = queue2;

    int num_shepherds = 10;
    pthread_t threads[num_shepherds];   // id потоков
    ShepherdData* data_array[num_shepherds]; // структуры данных для потоков
    
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