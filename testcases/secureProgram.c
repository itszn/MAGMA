#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>



struct ThingA {
    int (*func)(const char *);
    char name[16];
};

struct ThingB {
    char desc[20];
};
    

void notUsedAfterFree() {
    struct ThingA* obj1 = NULL;
    struct ThingB* obj2 = NULL;

    char buffer[32];

    while (1) {
        printf("Your objects: %p %p\n",obj1, obj2);
        printf("Menu:\n 1. Create A\n 2. Create B\n 3. Use A\n 4. Delete all\n> ");
        fgets(buffer, 32, stdin);
        int v = atoi(buffer);
        switch (v){
            case 1:
                if (obj1 != NULL) {
                    printf("Hey, %s already exists at %p\n", obj1->name, obj1);
                } else {
                    obj1 = malloc(sizeof(struct ThingA));
                    printf("What name? ");
                    fgets(obj1->name, 16, stdin);
                    obj1->func = puts;
                }
                break;
            case 2:
                if (obj2 != NULL) {
                    printf("Hey, %s already exists at %p\n", obj2->desc, obj2);
                } else {
                    obj2 = malloc(sizeof(struct ThingB));
                    printf("What desc? ");
                    fgets(obj2->desc, 20, stdin);
                }
                break;
            case 3:
                if (obj1 == NULL) {
                    printf("No UAF allowed! Get out of here you hacker\n");
                } else {
                    obj1->func(obj1->name);
                }
                break;
            case 4:
                if (obj1 != NULL) {
                    free(obj1);
                    obj1 = NULL;
                }
                if (obj2 != NULL) {
                    free(obj2);
                    obj2 = NULL;
                }
                break;
            default:
                return;
        }
    }
}





int main() {
    notUsedAfterFree();
}
