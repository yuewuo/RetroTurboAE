#include "stdio.h"
#include "stdlib.h"

int main() {
    int ret = system("cmake --version");
    printf("ret: %d\n", ret);
    ret = system("unknowncommand233");
    printf("ret: %d\n", ret);
    return 0;
}
