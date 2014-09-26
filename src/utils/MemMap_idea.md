Fork
if Pid==0
    wait parent signal (sig usr handler ?)
    cmd= schedtool max prio + affinity cmd, args
    execve cmd args
else
    insmod(pid)
    signal child
    waitpid(pid)
    gather_trace
