#include <stdio.h>
int main(void){
    int x, y, z;
    scanf("%d%d%d", &x, &y, &z);

    y=z/10-7*x;
    -y-(+z)%(z+100);
    z =(x++) + (y--);
    x=(--y)*(++z);
    x=z-+-+-+-++y;
        ;       
    x=y=z=3+5;

    printf("x = %d\n", x);
    printf("y = %d\n", y);
    printf("z = %d\n", z);
}