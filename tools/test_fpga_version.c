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
        {0x10007, magic, revision, 0x20261007, 0x00174503, 1},
        {0x10008, magic, revision, 0x20261007, 0x00000000, 1},
        {0x10007, magic, revision, 0x20240229, 0x00235959, 1},
        {0x10001, 0x10001, 0x10001, 0x10001, 0x10001, 0}, /* old aliasing bitstream */
        {0x20000, magic, revision, 0x20261007, 0x00174503, 0},
        {0x10002, magic, revision, 0x20261007, 0x00174503, 0}, /* missing key-response protocol */
        {0x10007, 0, revision, 0x20261007, 0x00174503, 0},
        {0x10007, magic, 0x10000, 0x20261007, 0x00174503, 0}, /* predates M17 ALD correction */
        {0x10007, magic, 0x10001, 0x20261007, 0x00174503, 0}, /* predates two-pass scheduling */
        {0x10007, magic, 0x10002, 0x20261007, 0x00174503, 0}, /* missing ISK return path */
        {0x10007, magic, 0x10028, 0x20261007, 0x00174503, 0}, /* unknown later revision */
        {0x10007, magic, 0x10026, 0x20261017, 0x00174503, 0}, /* sampled KS121 reset can lose arithmetic-shift zero status */
        {0x10007, magic, 0x10022, 0x20261007, 0x00174503, 0}, /* storage bridge can lose first-pass pulses or double a store sequence */
        {0x10007, magic, 0x10021, 0x20261007, 0x00174503, 0}, /* held-input single-shots do not expire */
        {0x10007, magic, 0x10020, 0x20261007, 0x00174503, 0}, /* missing AR311 and AP475/AP555 sampled parity paths */
        {0x10007, magic, 0x1001f, 0x20261007, 0x00174503, 0}, /* missing RT701 and AP561/571/581 gates */
        {0x10007, magic, 0x1001e, 0x20261007, 0x00174503, 0}, /* missing AS201 coherent complementary clock pair */
        {0x10007, magic, 0x1001d, 0x20261007, 0x00174503, 0}, /* missing DP091, MA241 and AP591 corrections */
        {0x10007, magic, 0x1001c, 0x20261007, 0x00174503, 0}, /* missing AP321 scan bit32 and zero PSBAR read */
        {0x10007, magic, 0x1001b, 0x20261007, 0x00174503, 0}, /* AP747 omits injected bits 64/65 */
        {0x10007, magic, 0x1001a, 0x20261007, 0x00174503, 0}, /* AP752 uses delayed negative HALF SUM63 instead of positive pin */
        {0x10007, magic, 0x10019, 0x20261007, 0x00174503, 0}, /* delayed ROS register complement creates false RY131 disable pulse */
        {0x10007, magic, 0x10018, 0x20261007, 0x00174503, 0}, /* RY131 WRITE polarity blocks external-register load */
        {0x10007, magic, 0x10017, 0x20261007, 0x00174503, 0}, /* defective RY131 P0 input and ROS36 polarity */
        {0x10007, magic, 0x10016, 0x20261007, 0x00174503, 0}, /* missing RY131 decode inputs and polarity */
        {0x10007, magic, 0x10015, 0x20261007, 0x00174503, 0}, /* missing AP675 AN right-one parity inverter */
        {0x10007, magic, 0x10014, 0x20261007, 0x00174503, 0}, /* inverted CE181 D-source parity */
        {0x10007, magic, 0x10013, 0x20261007, 0x00174503, 0}, /* delayed AP801 complement */
        {0x10007, magic, 0x10012, 0x20261007, 0x00174503, 0}, /* crossed AP635 left-four parity inputs */
        {0x10007, magic, 0x10011, 0x20261007, 0x00174503, 0}, /* missing local serial-adder clock delays */
        {0x10007, magic, 0x10010, 0x20261007, 0x00174503, 0}, /* missing serial-adder parity repairs */
        {0x10007, magic, 0x1000f, 0x20261007, 0x00174503, 0}, /* missing CE181 hold, RT777 parity and coherent ST capture */
        {0x10007, magic, 0x1000e, 0x20261007, 0x00174503, 0}, /* missing logout sequencing and delayed P4 */
        {0x10007, magic, 0x1000d, 0x20261007, 0x00174503, 0}, /* missing PADDA bit-58 hot-one input */
        {0x10007, magic, 0x1000c, 0x20261007, 0x00174503, 0}, /* old DR201 latched word-scan decodes */
        {0x10007, magic, 0x1000b, 0x20261007, 0x00174503, 0}, /* missing DS171-to-DS111 branch input */
        {0x10007, magic, 0x1000a, 0x20261007, 0x00174503, 0}, /* old ROS transcription and field grouping */
        {0x10007, magic, 0x10009, 0x20261007, 0x00174503, 0}, /* old logout delay polarity/timing */
        {0x10007, magic, 0x10008, 0x20261007, 0x00174503, 0}, /* old ROS backup and RT771 parity */
        {0x10007, magic, 0x10007, 0x20261007, 0x00174503, 0}, /* old RX081 reset/scan selection */
        {0x10007, magic, 0x10007, 0x20261007, 0x00174503, 0}, /* old RW/RF parity and AS034 complement */
        {0x10007, magic, 0x10007, 0x20261007, 0x00174503, 0}, /* old aggregate half-sum polarity */
        {0x10007, magic, 0x10004, 0x20261007, 0x00174503, 0}, /* old AP parity checks */
        {0x10007, magic, 0x10003, 0x20261007, 0x00174503, 0}, /* old panel polarities */
        {0x10007, magic, 0x10023, 0x20261007, 0x00174503, 0}, /* no diagnostic state/halt */
        {0x10007, magic, 0x10024, 0x20261007, 0x00174503, 0}, /* no ROSAR breakpoints/step */
        {0x10007, magic, 0x10025, 0x20261007, 0x00174503, 0}, /* no panel P0 counter */
        {0x10006, magic, revision, 0x20261007, 0x00174503, 0}, /* old diagnostic protocol */
        {0x10005, magic, revision, 0x20261007, 0x00174503, 0}, /* no diagnostic registers */
        {0x10004, magic, revision, 0x20261007, 0x00174503, 0}, /* old pressed flags */
        {0x10007, magic, revision, 0, 0, 0},
        {0x10007, magic, revision, 0x20260229, 0x00174503, 0},
        {0x10007, magic, revision, 0x21000229, 0x00174503, 0},
        {0x10007, magic, revision, 0x20260a07, 0x00174503, 0},
        {0x10007, magic, revision, 0x20260431, 0x00174503, 0},
        {0x10007, magic, revision, 0x20261007, 0x00240000, 0},
        {0x10007, magic, revision, 0x20261007, 0x00176000, 0},
        {0x10007, magic, revision, 0x20261007, 0x01000000, 0}
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
