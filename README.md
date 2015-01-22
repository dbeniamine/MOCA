# MOCA: A memory analysis tool

MOCA (Memory Organisation Cartography & Analysis) is a tool designed to
provide a fine analysis of an application's memory usage. It mainly consist of
a kernel module and a script which allows you to load the module and monitor a
program.


## Background Knowledge

**TODO**

+ Page
+ Pagefault
+ Phy vs virt
+ HI
+ Chunks
+ Probes
+ False page faults (ref SPCD) + mprotect
+ System Noise


## Usage

To monitor a program with Moca, you can run the following command as root

    moca -c "my_command" -a "my_arguments"


### Log

The command output will be logged in the files <code>Moca-cmd.log</code> and
<code>Moca-cmd.err</code>, these filenames can be modified with the option
<code>-f file</code>.

The output of Moca is logged in the file <code>Moca-output.log</code>, this
can be modified by the parameter <code>-l file</code>.

Finally the traces are in the files named <code>Moca-taskX</code>.

### Fine tunning


The following parameters allows you to do fine tuning on the module. By
default you shouldn't need to use them.

+ If you encounter performance issues, you can increase the wakeup interval,
the priority (reduce the system noise) or the hashmap numbit parameter.

+ If Moca tells you that a part of the trace have been dropped because there
was not enought space to sotre it, you can increase the number of chunks, the
chunksize or reduce the wakeup interval. Please note that, as memory is quite
restricted in the kernel, it might be a better idea to play on the wakeup
interval the priority than on the storage related parameters.

To do so, the following parameters are available:

    -w ms           Set the wakeup interval for Moca to interval ms
                    Default: 50ms"
    -p prio         Schedtool priority for the kernel module, the program
                    priority will be prio-1. You can increase this parameter
                    to reduce the system noise. Default: $prio"
    -b numbits      Set the number of bits used for the chunk hashmaps.
                    The map size is 2^numbits. the higher numbits is, the
                    less collision you should have. Default: 14"
    -S ChunkSize    Set the number of adress that can be stored in one
                    chunk. You can also reduce the wakeup interval, and
                    therefore the number of adresses needed per chunks.
                    Default: 2*2^14."
    -C nbChunks     Set the number of chunks kept in memory. Default: 20."



### Trace

By default Moca traces are saved in files named Moca-taskX, there is on file
per task (process or thread), X is the ID of the task (starting at 0, by ordre
of creation).
These are plain text easily parsable files, they can be imported and
visualised into framesoc[3] (see next section).

All files starts with a Line giving the internalID and the system processID:

    Task internalId ProcessId

The second is always the beginning of a chunk:

    Chunk id N start end cpumask

These lines contains the identifier of the chunk inside the task, the number N
of access in this chunk, the timestamp of the beginning and end of the chunk
and a bitmask telling which processors have accessed to this chunk.

Each Chunk line is followed by N access lines:

    Access @Virt @Phy countread countwrite cpumas

Each access lines correspond to one page, it gives the virtual and physical
address of the page, the number of read and writes observed and a cpumask
telling which processors are responsible of these access.


### Visualisation

**TODO**
+ Framesoc[3]
+ ocelotl[4]

## Installation

**TODO**

## Limitations

+ mprotect

## References

[1] HI
[2] SPCD
[3] Framesoc
[4] Ocelotl
