# battstat
## collect battery statistics

I'd like to collect my own statistics about my XPS 13's battery performance,
and I wanted to write some C code. The result is this program: a daemon that
should regularly write a snapshot of the battery and system state to a file.

It uses a binary format for the log, which is defined as

```
struct snapshot {
	/* The percentage of the battery remaining (between 0 and 100). */
        uint32_t        percentage;

	/* ACPI state of battery. */
        uint32_t        state;

	/* When was the snapshot taken? */
        uint64_t        when;

	/* See below. */
        struct sysinfo  sys;
};
```

The `struct sysinfo` structure is defined in the `sys/sysinfo.h` header. For
modern Linux kernels:

```
struct sysinfo {
        long uptime;             /* Seconds since boot */
        unsigned long loads[3];  /* 1, 5, and 15 minute load averages */
        unsigned long totalram;  /* Total usable main memory size */
        unsigned long freeram;   /* Available memory size */
        unsigned long sharedram; /* Amount of shared memory */
        unsigned long bufferram; /* Memory used by buffers */
        unsigned long totalswap; /* Total swap space size */
        unsigned long freeswap;  /* swap space still available */
        unsigned short procs;    /* Number of current processes */
        unsigned long totalhigh; /* Total high memory size */
        unsigned long freehigh;  /* Available high memory size */
        unsigned int mem_unit;   /* Memory unit size in bytes */
        char _f[20-2*sizeof(long)-sizeof(int)]; /* Padding to 64 bytes */
};
```

A snapshot record is 128 bytes long.


## Usage

```
Usage: ./battstat [-b battery] [-d] [-f statfile] [-h] [-t delay]
        -b battery      Select the battery name to monitor.
        -d              Don't daemonise; run in the foreground.
        -f statfile     Specify the file to write statistics to.
        -h              Print this help message.
        -t delay        Specify the delay in seconds between updates.
```


## Dependencies

* libacpi (`sudo apt-get install libacpi-dev`)
* libpthreads

This only works on Linux.


## License

`battstat` is released under the MIT license. See the LICENSE file for
details.
