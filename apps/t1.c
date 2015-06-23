#include <stdio.h>

char *fred[] = { "Test","one","two","three" };


main() {
    int i=0;

    for(i=0;i<4;i++) {
        printf("%s\n",fred[i]);
    }

}
