
#include "shared_ptr.h"
#include <cstdio>

int main() {
    POC::shared_ptr<int> pa(new int);
    printf("pa: %ld\n", pa.use_count());
    POC::shared_ptr<int> pb = pa;
    printf("pa: %ld, pb: %ld\n", pa.use_count(), pb.use_count());
    return 0;
}
