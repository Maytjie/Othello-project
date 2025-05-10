# Othello Local Referee

## Description

The Othello local referee serves as a method of testing your Multi-Process Othello Game Algorothm locally
without the need of a `IngeniousFrame.jar` file.

## How to

To use the local referee, first, copy your own `my_player.c` functions into the `src/my_player.c` file within this directory.
There are comments indicating where the normal run_master functionality must go. The run_worker function can be directly copied
from your code.
All other self-implemented functions by you should be copied into the `src/my_player.c` file as (along with their declarations).

### Manually

1.  Run the `make` command in the root of this directory to compile the source code.
2.  To run the local referee run a command of this form: `mpirun -np N player/myplayer <0 | 1> 61235 T log_player.txt`. Where:
    N is the number of processes to use. 0 or 1 is the colour and starting position of your player. T is the time limit in seconds per move.

### Bash Script

To run the local referee autmatically, run `./runlocal.sh`. This will compile and run with the variables set within the bash script.
You may modify the variables within the bash script at your own will. They function as decribed in the manual case above.

### Output Files

Output for your player is piped to `log_player.txt` and similar output is generated for the random opponent.
