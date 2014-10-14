#!/bin/bash
child()
{
    sleep 10 &
    echo "child awake"
}
child &
sleep 5
echo "Parent awake"

