#include "../src/bedrock_version.h"
#include <cstdio>

int main() {
    using lumen::supportedBedrock;
    if (!supportedBedrock(1, 26, 51, 1) ||
        supportedBedrock(1, 26, 45, 1) ||
        supportedBedrock(1, 26, 50, 4) ||
        supportedBedrock(1, 26, 51, 0) ||
        supportedBedrock(1, 26, 51, 2) ||
        supportedBedrock(1, 26, 5101, 0) ||
        supportedBedrock(1, 26, 52, 1) ||
        supportedBedrock(1, 27, 51, 1) ||
        supportedBedrock(2, 26, 51, 1) ||
        supportedBedrock(0, 0, 0, 0)) return 1;
    std::puts("BEDROCK_VERSION_OK");
}
