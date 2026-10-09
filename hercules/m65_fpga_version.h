#ifndef M65_FPGA_VERSION_H
#define M65_FPGA_VERSION_H

/* BAR0 DWORD indices. Keep revisions in step with fpga_build.vhd. */
#define M65_REG_BUILD_MAGIC 0x1fb
#define M65_REG_BUILD_TIME  0x1fc
#define M65_REG_BUILD_DATE  0x1fd
#define M65_REG_FPGA_VER    0x1fe
#define M65_FPGA_BUILD_MAGIC 0x4d363542U
#define M65_FPGA_INTERFACE_MAJOR 1U
#define M65_FPGA_INTERFACE_MINOR_MIN 3U
#define M65_FPGA_REQUIRED_VERSION 0x00010003U

static inline unsigned int m65_bcd_value(unsigned int packed)
{
    unsigned int result = 0, scale = 1;
    while (packed) {
        unsigned int digit = packed & 15U;
        if (digit > 9U) return ~0U;
        result += digit * scale;
        scale *= 10U;
        packed >>= 4;
    }
    return result;
}

/* No device writes: callers must check this before the first CPU command. */
static inline const char *m65_fpga_compatibility_error(unsigned int interface_version,
    unsigned int magic, unsigned int fpga_version, unsigned int date, unsigned int time)
{
    unsigned int year, month, day, days, hour, minute, second;
    if ((interface_version >> 16) != M65_FPGA_INTERFACE_MAJOR)
        return "unsupported PCIe interface major version";
    if ((interface_version & 65535U) < M65_FPGA_INTERFACE_MINOR_MIN)
        return "bitstream predates ISK key responses (interface 1.3 required)";
    if (magic != M65_FPGA_BUILD_MAGIC)
        return "missing FPGA build metadata signature";
    if (fpga_version != M65_FPGA_REQUIRED_VERSION)
        return "unsupported FPGA revision";
    year = m65_bcd_value(date >> 16);
    month = m65_bcd_value((date >> 8) & 255U);
    day = m65_bcd_value(date & 255U);
    if (year < 2000U || year > 9999U || month < 1U || month > 12U || day < 1U)
        return "invalid or unstamped FPGA build date";
    days = (month == 4U || month == 6U || month == 9U || month == 11U) ? 30U : 31U;
    if (month == 2U) days = 28U + ((year % 4U == 0U) && (year % 100U != 0U || year % 400U == 0U));
    if (day > days) return "invalid FPGA build date";
    hour = m65_bcd_value((time >> 16) & 255U);
    minute = m65_bcd_value((time >> 8) & 255U);
    second = m65_bcd_value(time & 255U);
    if ((time >> 24) != 0U || hour > 23U || minute > 59U || second > 59U)
        return "invalid FPGA build time";
    return 0;
}
#endif
