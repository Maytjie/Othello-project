#!/bin/bash
N=2
MY_COLOUR=2.0
T=1000
make clean && make

#main <colour - 1.0 black, 2.0 white> <port - ignored> <time limit> <player log file>
mpirun -np ${N} player/myplayer ${MY_COLOUR} 61235 ${T} log_player.txt

echo -e "Log files: my_player.log"
cat my_player.log
