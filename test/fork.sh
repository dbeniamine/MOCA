#!/bin/bash
child()
{
    sleep 10 &
    # $$ is the pid of the script, we need to use BASHPID here
    echo "child awake pid $BASHPID"
    ps
}
child &
sleep 5
echo "Parent awake pid $$"
ps
