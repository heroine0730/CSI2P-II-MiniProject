#include <stdio.h>
int main(void){
    int x, y, z;
    scanf("%d%d%d", &x, &y, &z);

    z =(x++) + (y--);
    // x=(--y)*(++z);

    printf("x = %d\n", x);
    printf("y = %d\n", y);
    printf("z = %d\n", z);
}