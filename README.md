# HPCBench

A plugin-based performance benchmarking framework focused around Linux.

HPCBench maps out the topology of the processor(s) and allows you to write plugins that
have access to this information and place threads placed on the desired CPU's to
allow for various latency-centric and performance-centric testing.

HPCBench has been successfully used to optimize interrupts and SMT affinity for hardware
which resulted in achieving both 100Gbit/s and 200Gbit/s line speed network rates on older
ring-based Intel chip sets by giving the tools required to optimize the interrupts and make
more efficient use of the CPU's.

## Plugin Exposed API's

* unsigned int hpcb_get_thread_cpu(worker *thread) - Returns the current CPU the requestor thread is running on. Worker thread is not required and can be NULL if called from the thread.
* unsigned int hpcb_set_thread_cpu(worker *thread, unsigned int cpu) - Pins specified thread to the requested CPU.
* uint64_t hpcb_tsccycles(void) - Returns the current cycle count derived from current timestamp counter (rdtscp register). Used to derive cycles between operations by determining the difference of a start and end value.
* uint64_t hpcb_realtime_ms(void) - Returns current clock time in usec granularity. Used to derive time between operations by determining the difference of a start and end value.

## Features Provided

* Provides mutex's for shared locks across threads in plugin or across multiple plugins
* Can run as many plugins at one time as chosen

## Included Plugins

* Example   - Shows an example of coding a plugin
* NOPLoop   - Runs 10^7 & 4096 NOP's to measure instruction rate of each thread.
* NFSTest   -
* PCIeLat   -
* rndmath   -
* memtest   -