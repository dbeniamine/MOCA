# MOCA: A memory analysis tool

MOCA (Memory Organisation Cartography & Analysis) is a tool designed to
provide a fine analysis of an application's memory usage. It mainly consist of
a kernel module and a script which allows you to load the module and monitor a
program.


## Background Knowledge

MOCA is the successor if HeapInfo [1], a Valgrind tool which intercept every
memory access and gives a carthograpy of the memory usage. This new tool is
based on a Linux kernel module, it intercept pagefaults to record memory
access. As pagefaults do not occurs very often, it periodically trigger false page faults
to record more access. This mechanism is inspired by [2]. More details on
Moca's implementation can be find on [3].

No tools is currently provided to visualize Moca traces, still there is a
prototype of visualization using
[framesoc](https://github.com/soctrace-inria/framesoc) [4] and
[ocelotl](https://github.com/soctrace-inria/ocelotl) [5]. The
importer required to visualize Moca traces in framesoc will be available soon
in [framesoc importer repository](https://github.com/soctrace-inria/framesoc.importers).

## Usage

To monitor a program with Moca, you can run the following command as root

    # moca -c "my_command" -a "my_arguments"

### Retrieving data structure informations

It is also possible to retrieve data structure name addresses and size, this
is done using a Pin instrumentation. To do so first install Pin from
[here](https://software.intel.com/en-us/articles/pintool-downloads). On 64bits
systems you will also need to install 32 bits support, on debian it will be
something like `sudo apt-get install libc6-i386`.

One this is done just tell Moca to use pin for structure detections:

    # moca -c "my_command" -a "my_arguments"

At this point `pin` must be in root's path and either pin is installed in
`/opt/pin/` or `PIN_HOME` should point to pin installation directory.

### Log

The command output will be logged in the files <code>Moca-cmd.log</code> and
<code>Moca-cmd.err</code>, these filenames can be modified with the option
<code>-f file</code>.

The output of Moca is logged in the file <code>Moca-output.log</code>, this
can be modified by the parameter <code>-l file</code>.

Finally the trace is in the file named <code>Moca-full-trace.csv</code>.

### Fine tunning

The following parameters allows you to do fine tuning on the
module. By default you shouldn't need to use them.

+ If you encounter performance issues, you can increase the wakeup interval,
the priority (reduce the system noise) or the hashmap numbit parameter.
+ If Moca tells you that a part of the trace have been dropped because there
was not enought space to sotre it, you can increase the number of chunks, the
chunksize or reduce the wakeup interval.  Please note that, as memory is quite
restricted in the kernel, it might be a better idea to play on the wakeup
interval the priority than on the storage related parameters.
    + <code>-w ms</code> Set the wakeup interval for Moca.  
    <code>Default: 40ms</code>
    + <code>-p prio</code> Schedtool priority for the kernel module, the
    program priority will be prio-1. You can increase this parameter to reduce
    the system noise.  
    <code>Default: use the normal process priority</code>
    + <code>-b numbits</code>  Set the number of bits used for the chunk
    hashmaps.  The map size is 2^numbits. the higher numbits is, the less
    collision you should have.  
    <code>Default: 14</code>
    + <code>-S ChunkSize</code>  Set the number of adress that can be stored
    in one chunk. You can also reduce the wakeup interval, and therefore the
    number of adresses needed per chunks.  
    <code>Default: 2*2^14.</code>
    + <code>-C nbChunks </code> Set the number of chunks kept in memory.  
    <code>Default: 20.</code>
    + <code>-F</code> Disable false page faults. If you set this flag, almost
    all memory event will be missed, you really shouldn't use it ..."
    + <code>-u</code> Use ugly false page faults hack, this can reduce MOCA's
    overhead, however you must be sure that your application won't swap or it
    will crash !

### Trace

Traces are saved as one human readable csv file. Each line indicate one memory
accesse described by its virtual and physical addresses (`@Virt` and `@Phy`)
the number of reads and writes (`Nreads` and `Nwrites`) a bitmask indicating
on which CPUs the access have been detected (`CPUMaask`) a start and end
timestamp correspondig to the chunk timestamp (̀`Start` and ̀`End`) and finaly
the internal identifier of the task (thread) responsible for the access
(`TaskId`).

### Visualisation


MOCA output is designed to be imported inside the trace visualisation
framework Framesoc[4], and to be visualised using Ocelotl[5]. This tool uses
an adaptive algorithm to aggregate data. This algorithm returns a set of
results which provides a trade-off between the information loss and the
reduction of the visualisation complexity. The user has then the possibility
to move the cursor between a very precise view trace or something more
aggregated. Moreover it provides the ability to navigate through the trace,
focus on one type of event or another.

To install Framesoc and ocelotl, go to the [official
website](http://soctrace-inria.github.io/framesoc/) and follow the
instruction. Than you can import Moca's trace through Framesoc's Moca importer
and visualise it with Ocelotl.

**Note: ** This visualisation method is still experimental, the importer will
soon be publicly accessible.


## Installation

No installation script is provided yet, however moca script can be run from
anywhere using the parameter -d to give the path to the Moca directory. If
"-n" is not specified in the command, Moca will recompile the module.


## References

[1] David Beniamine. *Cartographier la mémoire virtuelle d'une application de
calcul scientifique. In ComPAS'2013 / RenPar'21* , Grenoble, France, 2013.  
[2] E. H. M. Cruz, M. Diener, and P. O. A. Navaux. *Using the Translation Lookaside
Buffet to Map Threads in Parallel Applications Based on Shared Memory.* In
Parallel Distributed Processing Symposium (IPDPS), 2012 IEEE 26th
International, page 532-543, May 2012.  
[3] D. Beniamine, Y. Corre, D. Dosimont and G. Huard. *Memory Organisation
Cartography & Analysis.* Technical Report RR-8694, Inria. March 2015.  
[4] G. Pagano, D. Dosimont, G. Huard, V. Marangozova-Martin, and J. M.
Vincent.  *Trace Management and Analysis for Embedded Systems.* In Embedded
Multicore Socs (MCSoC), 122, Sept 2013.  
[5] Damien Dosimont, Lucas Mello Schnorr, Guillaume Huard, and Jean-Marc
Vincent. *A Trace Macroscopic Description based on Time Aggregation.* Technical
Report RR-8524, Apr 2014.  Trace visualization; trace analysis; trace
overview; time aggregation; parallel systems; embedded systems; information
theory; scientific computation; multimedia appli- cation; debugging;
optimization.
