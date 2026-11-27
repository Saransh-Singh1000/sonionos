#include <stdio.h>
#include <stdlib.h>
#include "usyscall.h"

int main(void) {
    printf("[C App] Hello from standard C in Ring 3 (CPL = %u)!\n", get_current_cpl());
    int *nums = (int *)malloc(3 * sizeof(int));
    nums[0] = 10; nums[1] = 20; nums[2] = 30;
    printf("[C App] malloc sum = %d\n", nums[0] + nums[1] + nums[2]);
    free(nums);
    return 0;
}
