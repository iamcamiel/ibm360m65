#include <stdio.h>
#include "../hercules/m65_fpga_version.h"

int main(void)
{
    const unsigned int magic = M65_FPGA_BUILD_MAGIC;
    const unsigned int revision = M65_FPGA_REQUIRED_VERSION;
    const struct {
        unsigned int interface_version, magic, revision, date, time;
        int accepted;
    } cases[] = {
        {0x10002, magic, revision, 0x20261007, 0x00174503, 1},
        {0x10003, magic, revision, 0x20261007, 0x00000000, 1},
        {0x10002, magic, revision, 0x20240229, 0x00235959, 1},
        {0x10001, 0x10001, 0x10001, 0x10001, 0x10001, 0}, /* old aliasing bitstream */
        {0x20000, magic, revision, 0x20261007, 0x00174503, 0},
        {0x10002, 0, revision, 0x20261007, 0x00174503, 0},
        {0x10002, magic, 0x10000, 0x20261007, 0x00174503, 0}, /* predates M17 ALD correction */
        {0x10002, magic, 0x10001, 0x20261007, 0x00174503, 0}, /* predates 100 MHz two-pass CPU scheduling */
        {0x10002, magic, 0x10003, 0x20261007, 0x00174503, 0}, /* unknown later revision */
        {0x10002, magic, revision, 0, 0, 0},
        {0x10002, magic, revision, 0x20260229, 0x00174503, 0},
        {0x10002, magic, revision, 0x21000229, 0x00174503, 0},
        {0x10002, magic, revision, 0x20260a07, 0x00174503, 0},
        {0x10002, magic, revision, 0x20260431, 0x00174503, 0},
        {0x10002, magic, revision, 0x20261007, 0x00240000, 0},
        {0x10002, magic, revision, 0x20261007, 0x00176000, 0},
        {0x10002, magic, revision, 0x20261007, 0x01000000, 0}
    };
    unsigned int i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        const char *error = m65_fpga_compatibility_error(cases[i].interface_version,
            cases[i].magic, cases[i].revision, cases[i].date, cases[i].time);
        if ((!error) != cases[i].accepted) {
            fprintf(stderr, "case %u failed: %s\n", i, error ? error : "unexpected acceptance");
            return 1;
        }
    }
    printf("FPGA compatibility: %u cases passed\n", i);
    return 0;
}
