#ifndef __PERCPU_H__
#define __PERCPU_H__

#define DEFINE_PER_CPU(type, name) typeof(type) name[NUM_CPUS]
#define DECLARE_PER_CPU(type, name) extern typeof(type) name[NUM_CPUS]

#define per_cpu(var, cpu) var[cpu]

#endif
