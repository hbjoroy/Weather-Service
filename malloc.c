#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void main(void) {
    printf("%lx\n", sizeof(size_t));

    unsigned char *p = (unsigned char *)malloc(0x400);

    size_t *p1 = (size_t *)(p - sizeof(size_t));
    printf("%p\n", p);
    printf("0x%lx\n", *p1);

    for (size_t i = 0; i < 0x400; i++) {
        p[i] = 42;
    }

    unsigned char *p2 = (unsigned char *)malloc(0x400);

    size_t *p3 = (size_t *)(p2 - sizeof(size_t));

    printf("0x%lx\n", *p3);

    free(p);

    free(p2);
    sleep(20);
}