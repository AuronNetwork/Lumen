#include "../src/bedrock_version.h"
#include <cstdio>

int main() {
    using lumen::supportedBedrock;
    if (!supportedBedrock(1, 26, 50, 4) ||
        supportedBedrock(1, 26, 45, 1) ||
        supportedBedrock(1, 26, 50, 0) ||
        supportedBedrock(1, 26, 50, 5) ||
        supportedBedrock(1, 26, 5004, 0) ||
        supportedBedrock(1, 26, 51, 4) ||
        supportedBedrock(0, 0, 0, 0)) return 1;
    std::puts("BEDROCK_VERSION_OK");
}
