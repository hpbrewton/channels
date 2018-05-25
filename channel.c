#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

typedef struct _chan {
    void *buf;
    int start;
    int offset;
    int size;
    int isclosed;
    int elemsize;
       
    pthread_mutex_t mu;
    pthread_cond_t send_cv;
    pthread_cond_t recv_cv;
} chan;

int end(chan *ch) {return (((ch->start + ch->offset) % ch->size) * ch->elemsize);}
int start(chan *ch) {return (ch->start * ch->elemsize);}
int full(chan *ch) {return ch->offset==ch->size;}
int empty(chan *ch) {return (ch->offset == 0);}

// API adapted from libthread API
chan *makechan(int elemsize, int bufsize);
int chsend(chan *c, void *v);
int chrecv(chan *c, void *v);

chan *
makechan(int elemsize, int bufsize) {
    chan *defer = malloc(sizeof(chan));
    defer->start = 0;
    defer->offset  = 0;
    defer->size  = bufsize;
    defer->elemsize = elemsize;
    defer->buf   = malloc(elemsize*bufsize);

    pthread_mutex_init(&(defer->mu), NULL);
    pthread_mutex_init(&(defer->mu), NULL);
    pthread_cond_init(&(defer->send_cv), NULL);
    pthread_cond_init(&(defer->recv_cv), NULL);

    return defer;
}

void
freechan(chan *c) {
    free(c->buf);
    free(c);
}

int 
chsend(chan *c, void *v) {
    pthread_mutex_lock(&(c->mu));
    while (full(c)) {
        pthread_cond_wait(&(c->send_cv), &(c->mu));
    }
    memcpy(&(((char*)c->buf)[end(c)]), v, c->elemsize);
    c->offset++;
    pthread_cond_signal(&(c->recv_cv));
    pthread_mutex_unlock(&(c->mu));
    return 0;
}

int 
chrecv(chan *c, void *v) {
    pthread_mutex_lock(&(c->mu));
    while (empty(c)) {
        pthread_cond_wait(&(c->recv_cv), &(c->mu));
    }
    memcpy(v, &(((char*)c->buf)[start(c)]), c->elemsize);
    c->start++;
    c->start = c->start % c->size; 
    c->offset--;
    pthread_cond_signal(&(c->send_cv));
    pthread_mutex_unlock(&(c->mu));
    return 0;
}

// TESTS

void 
check_timeout(void * (*func)(void *), unsigned int time) {
    pthread_t thread;
    int finished = 0;
    pthread_create(&thread, NULL, func, (void *) &finished);
    sleep(time);
    assert(finished == 0);
    pthread_cancel(thread);
}

void
simple_linear() {
    chan *c1 = makechan(sizeof(int), 32);
    for (int i = 1; i < 10; ++i) {
        chsend(c1, (void *) &i);
    }
    int place;
    for (int i = 0; i < 9; ++i) {
        chrecv(c1, (void *) &place);
        assert(place == i + 1); 
    }
    
}

void *
simple_linear2(void * finished_vptr) {
    chan *c1 = makechan(sizeof(int), 5);
    for (int i = 1; i < 10; ++i) {
        chsend(c1, (void *) &i);
    }
    int place;
    for (int i = 0; i < 9; ++i) {
        chrecv(c1, (void *) &place);
        assert(place == i + 1); 
    }

    int *finished = (int *) finished_vptr;
    *finished = 1;
    return finished_vptr;
}

int items = 100;

void *
producer(void *c_vptr) {
    chan *c = (chan *) c_vptr;
    for (int n = items; n >= 0; --n) { 
        chsend(c, &n);
    }
    return NULL;
}

void *
consumer(void *c_vptr) {
    chan *c = (chan *) c_vptr;
    int val = 1;
    for (; val != 0;) {
        chrecv(c, &val);
        printf("%d\n", val);
    }
    return NULL;
}

void
producerconsumer(int numproducers, int numconsumers) {
    chan *c1 = makechan(sizeof(int), 11);
    pthread_t producers [numproducers];
    pthread_t consumers [numconsumers];
    for (int i = 0; i < numproducers; ++i)
        pthread_create(&producers[i], NULL, producer, (void *) c1);
    for (int i = 0; i < numconsumers; ++i)
        pthread_create(&consumers[i], NULL, consumer, (void *) c1);

    for (int i = 0; i < numconsumers; ++i)
        pthread_join(consumers[i], NULL);
}

int
main(int argc, char *argv[]) {
    if (argc == 0) {
        printf ("usage:     <numproducers>, <numconsumers>, number of elements to write per producer.\n");
    }
    items = atoi(argv[3]);
    producerconsumer(atoi(argv[1]), atoi(argv[2]));  
}
