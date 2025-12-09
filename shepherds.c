#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define DIR1 1
#define DIR2 2
#define MAX_THREADS 20

typedef struct {
    int queue[MAX_THREADS];
    int size;
} DirectionQueue;

typedef struct {
    int direction;
    int id;
    void* crossing_ptr;
} ShepherdData;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    
    // int* queue_array;
    int queue_array[MAX_THREADS];
    int hat_present;
    bool crossing_busy;
    int current_dir;

    DirectionQueue dir1_queue;
    DirectionQueue dir2_queue;
} Crossing;

void print_queue(Crossing* crossing, int direction, int current_id) {
    DirectionQueue* queue;
    const char* dir_name;
    
    if (direction == DIR1) {
        queue = &crossing->dir1_queue;
        dir_name = "DIR1";
    } else {
        queue = &crossing->dir2_queue;
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

void crossing_init(Crossing* crossing) {   // TODO переименовать бридж
    pthread_mutex_init(&crossing->mutex, NULL);
    pthread_cond_init(&crossing->cond, NULL);
    crossing->hat_present = 0;
    crossing->crossing_busy = false;
    crossing->current_dir = 0;
}

void crossing_destroy(Crossing* crossing) {
    pthread_mutex_destroy(&crossing->mutex);
    pthread_cond_destroy(&crossing->cond);
}

void dir1_cross(Crossing* crossing, int shepherd_id) { 
    pthread_mutex_lock(&crossing->mutex);
    queue_add(&crossing->dir1_queue, shepherd_id);
    pthread_mutex_unlock(&crossing->mutex);
    printf("DIR1-%d встает в очередь\n", shepherd_id);  // оповещаем и добавляем в id очередь

    print_queue(crossing, DIR1, shepherd_id); // распечатка очереди

    while (crossing->dir1_queue.queue[0] != shepherd_id){        // в условии сказано, что пастух идет класть шапку, если перед входом никого нет, даже если кто то есть на мосту
        printf("DIR1-%d ждет около моста\n", shepherd_id);
        pthread_cond_wait(&crossing->cond, &crossing->mutex);  // ждет сигнал и захватывает
    }

    printf("DIR1-%d: иду один через мост...\n", shepherd_id);
    usleep(50000);
    if (crossing->hat_present == 0) { // использую две шапки
        crossing->hat_present = 1;
        printf("DIR1-%d Поставил шапку\n", shepherd_id);
    }
    else {
        printf("DIR1-%d Поставил вторую шапку\n", shepherd_id);
        crossing->hat_present = 2;
    }
    usleep(50000);

    while (crossing->crossing_busy) {        // теперь ждем освобождения моста 
        printf("DIR1-%d ждет: на мосту стадо\n", shepherd_id);
        pthread_cond_wait(&crossing->cond, &crossing->mutex);  
    }

    crossing->crossing_busy = true; 
    crossing->current_dir = DIR1;
    queue_remove_front(&crossing->dir1_queue);  // освобождаем место у входа и позволяем другому пастуху положить свою шапку и сами начинаем переход со стадом
    printf("DIR1-%d начинает переход. Мост занят DIR1\n", shepherd_id);
    pthread_mutex_unlock(&crossing->mutex);
    pthread_cond_broadcast(&crossing->cond);

    printf("DIR1-%d: веду стадо через мост...\n", shepherd_id);
    usleep(rand() % 400000 + 200000);
    
    pthread_mutex_lock(&crossing->mutex);
    crossing->crossing_busy = false;
    crossing->current_dir = 0;
    crossing->hat_present = crossing->hat_present - 1;  
    printf("DIR1-%d завершил переход и забирает шапку\n", shepherd_id);
    pthread_mutex_unlock(&crossing->mutex);

    pthread_cond_broadcast(&crossing->cond);
}

void dir2_cross(Crossing* crossing, int shepherd_id) {
    pthread_mutex_lock(&crossing->mutex);         
    queue_add(&crossing->dir2_queue, shepherd_id);
    pthread_mutex_unlock(&crossing->mutex);
    printf("DIR2-%d встает в очередь\n", shepherd_id);  

    print_queue(crossing, DIR2, shepherd_id); // распечатка очереди

    while (crossing->crossing_busy || crossing->hat_present || crossing->dir2_queue.queue[0] != shepherd_id) { // заходят, если нет шапок, мост не занят и они первые в очереди
        if (crossing->dir2_queue.queue[0] == shepherd_id) {
            if (crossing->hat_present)
                printf("DIR2-%d ждет: видит шапку\n", shepherd_id);
            else 
                printf("SOMETHING WENT WRONG"); // т.к. после прохода сдвигаются id
        } else {
            printf("DIR2-%d ждет свою очередь\n", shepherd_id);
        }
        pthread_cond_wait(&crossing->cond, &crossing->mutex);
    }
    
    crossing->crossing_busy = true;
    crossing->current_dir = DIR2;
    pthread_mutex_unlock(&crossing->mutex);
    
    printf("DIR2-%d начинает переход. Мост занят DIR2\n", shepherd_id);
    printf("DIR2-%d: веду стадо через мост...\n", shepherd_id);
    usleep(rand() % 500000 + 300000);
    
    pthread_mutex_lock(&crossing->mutex);
    crossing->crossing_busy = false;
    crossing->current_dir = 0;
    queue_remove_front(&crossing->dir2_queue);
    printf("DIR2-%d завершил переход\n", shepherd_id);
    pthread_mutex_unlock(&crossing->mutex);

    pthread_cond_broadcast(&crossing->cond);
}

void* shepherd_thread(void* arg) {
    ShepherdData* data = (ShepherdData*)arg;
    int direction = data->direction;
    int id = data->id;
    Crossing* crossing = (Crossing*)data->crossing_ptr;
    
    usleep(rand() % 1000000);
    
    if (direction == DIR1) {
        dir1_cross(crossing, id);
    } else {
        dir2_cross(crossing, id);
    }
    
    free(data);
    return NULL;
}

int main() {
    srand(time(NULL));
    
    Crossing crossing;
    crossing_init(&crossing);
    
    DirectionQueue queue1;
    DirectionQueue queue2;
    queue_init(&queue1);
    queue_init(&queue2);
    (&crossing)->dir1_queue = queue1;
    (&crossing)->dir2_queue = queue2;

    int num_shepherds = 10;
    pthread_t threads[num_shepherds];   // id потоков
    ShepherdData* data_array[num_shepherds]; // структуры данных для потоков
    
    printf("Начало моделирования\n\n");
    
    for (int i = 0; i < num_shepherds; i++) {
        ShepherdData* data = malloc(sizeof(ShepherdData));
        data->direction = (rand() % 2 == 0) ? DIR1 : DIR2;
        data->id = i + 1;
        data->crossing_ptr = &crossing;
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
    
    crossing_destroy(&crossing);
    return 0;
}