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
        {0x10005, magic, revision, 0x20261007, 0x00174503, 1},
        {0x10006, magic, revision, 0x20261007, 0x00000000, 1},
        {0x10005, magic, revision, 0x20240229, 0x00235959, 1},
        {0x10001, 0x10001, 0x10001, 0x10001, 0x10001, 0}, /* old aliasing bitstream */
        {0x20000, magic, revision, 0x20261007, 0x00174503, 0},
        {0x10002, magic, revision, 0x20261007, 0x00174503, 0}, /* missing key-response protocol */
        {0x10005, 0, revision, 0x20261007, 0x00174503, 0},
        {0x10005, magic, 0x10000, 0x20261007, 0x00174503, 0}, /* predates M17 ALD correction */
        {0x10005, magic, 0x10001, 0x20261007, 0x00174503, 0}, /* predates two-pass scheduling */
        {0x10005, magic, 0x10002, 0x20261007, 0x00174503, 0}, /* missing ISK return path */
        {0x10005, magic, 0x1000c, 0x20261007, 0x00174503, 0}, /* unknown later revision */
        {0x10005, magic, 0x1000a, 0x20261007, 0x00174503, 0}, /* old ROS transcription and field grouping */
        {0x10005, magic, 0x10009, 0x20261007, 0x00174503, 0}, /* old logout delay polarity/timing */
        {0x10005, magic, 0x10008, 0x20261007, 0x00174503, 0}, /* old ROS backup and RT771 parity */
        {0x10005, magic, 0x10007, 0x20261007, 0x00174503, 0}, /* old RX081 reset/scan selection */
        {0x10005, magic, 0x10006, 0x20261007, 0x00174503, 0}, /* old RW/RF parity and AS034 complement */
        {0x10005, magic, 0x10005, 0x20261007, 0x00174503, 0}, /* old aggregate half-sum polarity */
        {0x10005, magic, 0x10004, 0x20261007, 0x00174503, 0}, /* old AP parity checks */
        {0x10005, magic, 0x10003, 0x20261007, 0x00174503, 0}, /* old panel polarities */
        {0x10004, magic, revision, 0x20261007, 0x00174503, 0}, /* old pressed flags */
        {0x10005, magic, revision, 0, 0, 0},
        {0x10005, magic, revision, 0x20260229, 0x00174503, 0},
        {0x10005, magic, revision, 0x21000229, 0x00174503, 0},
        {0x10005, magic, revision, 0x20260a07, 0x00174503, 0},
        {0x10005, magic, revision, 0x20260431, 0x00174503, 0},
        {0x10005, magic, revision, 0x20261007, 0x00240000, 0},
        {0x10005, magic, revision, 0x20261007, 0x00176000, 0},
        {0x10005, magic, revision, 0x20261007, 0x01000000, 0}
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
