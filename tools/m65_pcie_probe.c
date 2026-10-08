#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../hercules/m65_fpga_version.h"

/* Inspect metadata by default. --read performs only aligned 32-bit BAR reads. */
static unsigned long attribute(const char *device, const char *name) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", device, name);
    FILE *f = fopen(path, "r");
    unsigned long value = ~0UL;
    if (f) {
        char text[64];
        if (fgets(text, sizeof text, f)) value = strtoul(text, NULL, 0);
        fclose(f);
    }
    return value;
}

static int read_registers(const char *resource) {
    int fd = open(resource, O_RDONLY | O_SYNC);
    if (fd < 0) { perror(resource); return 1; }
    struct stat st;
    if (fstat(fd, &st) || st.st_size < 2048) {
        fprintf(stderr, "BAR0 must expose at least 2048 bytes\n"); close(fd); return 1;
    }
    void *map = mmap(NULL, 2048, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) { perror("mmap BAR0"); return 1; }
    volatile const uint32_t *regs = map;
    uint32_t id = le32toh(regs[0]);
    printf("ID 0x%08x (expected 0x03602065)\n", id);
    if (id != 0x03602065) {
        fprintf(stderr, "Unexpected register signature; stopping\n");
        munmap(map, 2048); return 1;
    }
    uint32_t version = le32toh(regs[0x1ff]);
    uint32_t revision = le32toh(regs[M65_REG_FPGA_VER]);
    uint32_t date = le32toh(regs[M65_REG_BUILD_DATE]);
    uint32_t time = le32toh(regs[M65_REG_BUILD_TIME]);
    const char *error = m65_fpga_compatibility_error(version,
        le32toh(regs[M65_REG_BUILD_MAGIC]), revision, date, time);
    printf("Interface %u.%u; FPGA %u.%u; build %08x %06x UTC\n",
        version >> 16, version & 65535, revision >> 16, revision & 65535, date, time);
    if (error) fprintf(stderr, "M65 FPGA rejected: %s\n", error);
    munmap(map, 2048);
    return error ? 1 : 0;
}

int main(int argc, char **argv) {
    const char *root = "/sys/bus/pci/devices";
    int read_bar = 0;
    if (argc == 3 && !strcmp(argv[1], "--fixture")) return read_registers(argv[2]);
    if (argc == 2 && !strcmp(argv[1], "--read")) read_bar = 1;
    else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--read | --fixture FILE]\n", argv[0]); return 2;
    }
    DIR *dir = opendir(root);
    if (!dir) { perror(root); return 1; }
    struct dirent *entry;
    int count = 0, found = 0, result = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        char device[512], path[1024];
        snprintf(device, sizeof device, "%s/%s", root, entry->d_name);
        unsigned long vendor = attribute(device, "vendor"), product = attribute(device, "device");
        printf("%s %04lx:%04lx\n", entry->d_name, vendor, product);
        count++;
        if (vendor != 0x0360 || product != 0x2065) continue;
        found++;
        printf("  M65 endpoint; enable=%lu\n", attribute(device, "enable"));
        snprintf(path, sizeof path, "%s/resource", device);
        FILE *f = fopen(path, "r");
        unsigned long long start, end, flags;
        if (!f || fscanf(f, "%llx %llx %llx", &start, &end, &flags) != 3) {
            if (f) fclose(f);
            fprintf(stderr, "Cannot read BAR0 metadata\n"); result = 1; continue;
        }
        fclose(f);
        printf("  BAR0 0x%llx..0x%llx flags=0x%llx\n", start, end, flags);
        if (read_bar) {
            snprintf(path, sizeof path, "%s/config", device);
            int fd = open(path, O_RDONLY);
            unsigned char command[2];
            int valid = fd >= 0 && pread(fd, command, 2, 4) == 2;
            if (fd >= 0) close(fd);
            if (!valid || !(command[0] & 2) || !(flags & 0x200) || end < start || end - start < 2047) {
                fprintf(stderr, "BAR0 is not assigned/enabled memory; no register reads performed\n");
                result = 1; continue;
            }
            snprintf(path, sizeof path, "%s/resource0", device);
            result |= read_registers(path);
        }
    }
    closedir(dir);
    if (!count) puts("No PCIe devices enumerated.");
    if (!found) puts("M65 FPGA endpoint (0360:2065) not present.");
    return found ? result : 3;
}
