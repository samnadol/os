#include "cpuid.h"

#include "../../drivers/tty.h"
#include "../../lib/string.h"

cpu_info cpuinfo;

bool get_cpu_info()
{
    dprintf("[CPU] Fetching CPUID information\n");

    memset(&cpuinfo, 0, sizeof(cpu_info));
    if (!cpuid_available())
        return false;

    uint32_t eax, ebx, ecx, edx;

    // get vendor
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    memcpy(&cpuinfo.vendor[0], &ebx, 4);
    memcpy(&cpuinfo.vendor[4], &edx, 4);
    memcpy(&cpuinfo.vendor[8], &ecx, 4);

    // get info
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    cpuinfo.edx = edx;

    // get model
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000002));
    memcpy(&cpuinfo.model[0], &eax, 4);
    memcpy(&cpuinfo.model[4], &ebx, 4);
    memcpy(&cpuinfo.model[8], &ecx, 4);
    memcpy(&cpuinfo.model[12], &edx, 4);

    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000003));
    memcpy(&cpuinfo.model[16], &eax, 4);
    memcpy(&cpuinfo.model[20], &ebx, 4);
    memcpy(&cpuinfo.model[24], &ecx, 4);
    memcpy(&cpuinfo.model[28], &edx, 4);

    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000004));
    memcpy(&cpuinfo.model[32], &eax, 4);
    memcpy(&cpuinfo.model[36], &ebx, 4);
    memcpy(&cpuinfo.model[40], &ecx, 4);
    memcpy(&cpuinfo.model[44], &edx, 4);

    return true;
}