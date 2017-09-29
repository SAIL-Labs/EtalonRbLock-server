#!/bin/bash

killall EtalonRbLock-server &>/dev/null
echo
echo $1 $2
echo
exec $1 &>/dev/null
