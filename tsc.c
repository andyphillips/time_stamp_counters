#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <sys/syscall.h>
#include <fcntl.h>

#include "tsc.h"

struct processor_type_s tsc_processor_types[] = {
     { "Too Old/Unknown",   100000u },
     { "Nehalem",           133330u },
     { "Westmere",          133330u },
     { "Sandybridge",       100000u },
     { "Ivybridge",         100000u },
     { "Haswell",           100000u },
     { "Broadwell",         100000u }, 
     { "Skylake",           100000u },
     { "Xeon Phi",          100000u }
}; 


// -------------------
//
// TSC functions.  
//
// requires a processor that supports the RDTSCP instruction. Or, use LFENCE; RDTSC
// We can extract the socket and cpu we actually ran on from %ECX. 
//  Intel Instruction Set reference. Vol 2B - 4-304. 
//  
//  returns cycles from the expected cpu.
uint64_t rdtscp(uint32_t expected_cpu)
{
        uint32_t lo, hi, cpuid;
#ifdef PARANOID_TSC
        uint32_t core, socket;
#endif     
     __asm__ __volatile__ ("rdtscp" : "=a" (lo), "=d" (hi), "=c" (cpuid)::);
#ifdef PARANOID_TSC     
        socket = (cpuid & 0xfff000)>>12;
        core = cpuid & 0xfff;
        if (core != expected_cpu) return -1;
#endif     
        return (uint64_t)hi << 32 | lo;
}
// 
// return the family and model in a single 32 bit integer. 
// Family 06 = the ones we're interested in. Family 15 = netburst xeons. 
// See;
// https://software.intel.com/en-us/articles/intel-architecture-and-processor-identification-with-cpuid-model-and-family-numbers
// and intel architecture developers manual. 
// 
static __inline__ uint32_t get_intel_family_model()
{
     uint32_t model_register, model, family;
     int eax = 1;
     
     __asm__ __volatile__ ("cpuid" : "=a" (model_register): "a" (eax): "%ebx", "%ecx", "%edx");
     
     model = ((model_register & 0xff) >> 4) | ((model_register & 0xf0000) >> 12);
     family = ((model_register & 0xf00) >> 8) | ((model_register & 0xff00000) >> 16);
     return (family << 16) | model;
}

// used to determine if we have a 133.33 mhz or 100.00 mhz bclk. 
int get_processor_type(uint32_t family_model)
{
     switch (family_model) {
	  // nehalem - Section 35.5 Vol 3c 
     case 0x6001a:
     case 0x6001e:
     case 0x6001f:
     case 0x6002e:
	  return NEHALEM;
	  // westmere - section 35.6
     case 0x60025:
     case 0x6002c:
     case 0x6002f:
	  return WESTMERE;
	  // sandy bridge - section 35.8
     case 0x6002a:
     case 0x6002d:
	  return SANDYBRIDGE;
     case 0x6003a:
     case 0x6003e:
	  return IVYBRIDGE;
     case 0x6003c:
     case 0x6003f:
     case 0x60045:
     case 0x60046:
	  return HASWELL;
     case 0x6003d:
     case 0x60047:
     case 0x6004f:
     case 0x60056:
	  return BROADWELL;
     case 0x6004e:
     case 0x6005e:
	  return SKYLAKE;
     case 0x60057:
	  return PHI;
     default:
	  return TOO_OLD;
     }
     return TOO_OLD;
}

// read msrs - stolen from cpupower helpers
// http://lxr.free-electrons.com/source/tools/power/cpupower/utils/helpers/msr.c#L26
// 
int read_msr(int cpu, unsigned int idx, uint64_t *val)
{
     int fd;
     char msr_file_name[64];
     
     sprintf(msr_file_name, "/dev/cpu/%d/msr", cpu);
     fd = open(msr_file_name, O_RDONLY);
     if (fd < 0) return -1;
     if (lseek(fd, idx, SEEK_CUR) == -1) goto err;
     if (read(fd, val, sizeof *val) != sizeof *val) goto err;
     close(fd);
     return 0;
 err:
     close(fd);
     return -1;
}
// 0xce is the intel MSR_PLATFORM_INFO. 
// SandyBridge or later (IvyBridge, Haswell) is a 100mhz clock. Nehalem/Westmere is a 133.33Mhz baseclock
// See section 35. 
// 
// There are some caveats. This is only supported on recent processor families. 
//  
// Also, if the motherboard manufacturer messes about with the BCLK then the base_clock will be wrong. 
//
uint32_t get_tsc_freq_khz(cpu)
{
     uint64_t platform_info, non_turbo_ratio;
     int processor_type;
     
     processor_type = get_processor_type(get_intel_family_model());
    
#ifdef TSC_VERBOSE
     printf ("Detected processor with family/model of %x\n",get_intel_family_model());
     printf ("This is a %s processor with a base clock of %dkhz\n", tsc_processor_types[processor_type].name, tsc_processor_types[processor_type].base_clock_khz);
#endif  
     if (processor_type == TOO_OLD) {
	  fprintf(stderr,"processor too new, too old, or not detected\n");
	  return -1;
     }
     
    if (read_msr(cpu,0xce, &platform_info) == -1) {
	 fprintf(stderr,"error reading MSR_PLATFORM_INFO\n");
	 return -1;
    }
    non_turbo_ratio = (platform_info & 0xff00) >> 8;
    return  non_turbo_ratio * tsc_processor_types[processor_type].base_clock_khz;
}

// See comment for the math behind this in arch/x86/kernel/tsc.c
// http://lxr.free-electrons.com/source/arch/x86/kernel/tsc.c?v=3.18#L157 
//  
uint32_t get_cycles_to_nsec_scale(unsigned int tsc_frequency_khz)
{
    return (uint32_t)((1000000)<<10 )/(uint32_t)tsc_frequency_khz;
}

uint64_t cycles_to_nsec(uint64_t cycles, uint32_t scale_factor)
{
     return (cycles * scale_factor) >> 10; 
}
//
// TSC Routines
// -------

#ifdef TEST_TSC
pid_t gettid(void) { return syscall(SYS_gettid); }  // not in libc apparently


// This needs to run as root. 
int main (int argc, char *argv[])
{
     cpu_set_t cpuset;
     uint32_t cycles_nsec_scale, tsc_freq_khz;
     uint64_t start_timestamp, end_timestamp, cycles;
     int cpu;
          
     if (argc < 2) {
	  fprintf (stderr,"./tsc <cpuid>\ne.g ./tsc 47\nwill pin to cpu47 and then run a simple timing loop to test tsc and cpu family ident\n");
	  exit (0);
     }
     
     cpu = atoi(argv[1]);
     
     CPU_ZERO(&cpuset);
     CPU_SET(cpu,&cpuset);
     
     // set our cpu affinity. Ideally onto an isolated cpu. 
     if (sched_setaffinity(gettid(),sizeof(cpu_set_t), &cpuset) != 0) {
	  fprintf(stderr,"Thread %d: failed to set affinity to cpu %d. Reason was %s\n",gettid(),cpu,strerror(errno));
	  exit(0);
     }
     
     // Get the MSR's idea of the TSC tick rate in khz 
     tsc_freq_khz = get_tsc_freq_khz(cpu);
     
     // convert this into a scale factor
     cycles_nsec_scale = get_cycles_to_nsec_scale(tsc_freq_khz);
     printf("Invariant TSC runs at %u kHz, scale factor %u\n",tsc_freq_khz, cycles_nsec_scale);
     
     // simple timing exercise to see if we're close to reality
     start_timestamp = rdtscp(cpu);
     usleep(500000);
     end_timestamp = rdtscp(cpu);
     
     // The difference in timestamp cycles, converted to nanoseconds via the scale factor. 
     cycles = (end_timestamp-start_timestamp);
     printf ("Expected to sleep for %u nanos, actually slept for %Lu cycles, %Lu nanos\n", (500000*1000), (unsigned long long)cycles, (unsigned long long)cycles_to_nsec(cycles, cycles_nsec_scale));
     
     return 0;
}
#endif
