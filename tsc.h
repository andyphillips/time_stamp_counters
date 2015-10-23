// tsc.h
#include <sys/types.h>
#include <stdint.h>

//
// check cpu we executed on was the one expected. Turn off for speed.  
#define PARANOID_TSC 1
// 
// Print out processor id information
//#define TSC_VERBOSE 1 
//
// Run as a stand alone program for testing
//#define TEST_TSC 1
//
// Identification for setting the bclk rate
// 
enum {
     TOO_OLD,
     NEHALEM,
     WESTMERE,
     SANDYBRIDGE,
     IVYBRIDGE,
     HASWELL,
     BROADWELL,
     SKYLAKE,
     PHI
}  processor_enum;

struct processor_type_s {
     char *name;
     unsigned int base_clock_khz;
};

extern struct processor_type_s tsc_processor_types[];

// prototypes
/* tsc.c */
uint64_t rdtscp(uint32_t expected_cpu);
int get_processor_type(uint32_t family_model);
int read_msr(int cpu, unsigned int idx, uint64_t *val);
uint32_t get_tsc_freq_khz(int cpu);
uint32_t get_cycles_to_nsec_scale(unsigned int tsc_frequency_khz);
uint64_t cycles_to_nsec(uint64_t cycles, uint32_t scale_factor);
