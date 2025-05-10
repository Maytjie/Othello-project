import subprocess
import glob
import os
import shutil
import csv
from common import globals

def load_players():
    # Get list list of all players with & without paths
    directories = [globals.BASE_DIRECTORY + dir + "/" \
               for dir in os.listdir(globals.BASE_DIRECTORY) \
               if os.path.isdir(globals.BASE_DIRECTORY + dir) and dir.endswith("_player")]
    player_names = [dir.split("/")[-2] for dir in directories]
    globals.NUM_PLAYERS = len(player_names)

    globals.logger.debug(f"Found {globals.NUM_PLAYERS} players")
    globals.logger.debug(f"Player Names: {player_names}")

    if len(player_names) < 2:
        globals.logger.error("Need at least two players")
        return False

    # Check for duplicate player player names
    if len(player_names) != len(set(player_names)):
        globals.logger.error("Found duplicate player names")
        return False

    for player_name in player_names:
        _remove_binary_files(player_name)
   
    for i in range(globals.NUM_PLAYERS):
        globals.player_to_path[player_names[i]] = directories[i]
    
    globals.logger.debug(f"Player to path mapped")

    return True

def gen_round_robin_pairs():
    # Get the existing player names
    existing_player_names = list(globals.player_to_path.keys())

    # Get the list of binary files in the players/ directory
    players_dir = os.path.join(globals.BASE_DIRECTORY, "players")
    binary_files = [f for f in os.listdir(players_dir) if os.path.isfile(os.path.join(players_dir, f)) and not f.startswith('.')]

    # Extract the player names from the binary file names
    new_player_names = [os.path.splitext(f)[0] for f in binary_files]

    # Combine the existing and new player names
    player_names = existing_player_names + [name for name in new_player_names if name not in existing_player_names]

    # Generate round robin pairs
    pairs = []
    for i in range(len(player_names)):
        for j in range(i+1, len(player_names)):
            pairs.append((player_names[i], player_names[j]))

    globals.round_robin_pairs = pairs
    globals.logger.debug(f"Round Robin Pairs: {pairs}")
    globals.MATCHES_TO_PLAY = len(pairs) * int(globals.CONFIG["numMatches"])

    write_round_robin_pairs()

def write_round_robin_pairs():
    with open(globals.RESULTS_CSV_PATH, "a") as file:
        writer = csv.DictWriter(file, fieldnames=globals.RESULTS_HEADER)
        for pair in globals.round_robin_pairs:
            player1, player2 = pair
            row = {
                "Player 1": player1,
                "Player 2": player2,
                "Winner": "DNF"
            }
            writer.writerow(row)

    globals.logger.debug(f"Wrote round robin pairs to {globals.RESULTS_CSV_PATH}")
    globals.logger.info(f"Round Robin Pairs: {len(globals.round_robin_pairs)}")

def find_incomplete_matches():
    incomplete_matches = []
    incomplete_player_names = set()  # Use a set to avoid duplicates

    with open(globals.RESULTS_CSV_PATH, mode='r') as file:
        reader = csv.DictReader(file)
        for row in reader:
            # Check if any chip count fields are empty
            if not row['Player 1 Chips Game 1'] or not row['Player 2 Chips Game 1'] or \
               not row['Player 1 Chips Game 2'] or not row['Player 2 Chips Game 2']:
                player1 = row['Player 1']
                player2 = row['Player 2']
                incomplete_matches.append((player1, player2))
                incomplete_player_names.add(player1)
                incomplete_player_names.add(player2)

    # Now find directories for these players
    for player_name in incomplete_player_names:
        player_dir = globals.BASE_DIRECTORY + player_name + "/"
        print(f"Player dir {player_dir}")
        if os.path.isdir(player_dir):
            globals.player_to_path[player_name] = player_dir
        else:
            globals.logger.error(f"Directory for player {player_name} not found.")
    
    globals.round_robin_pairs = incomplete_matches
    globals.NUM_PLAYERS = len(incomplete_player_names)
    globals.MATCHES_TO_PLAY = 2 * len(globals.round_robin_pairs)
    globals.logger.debug(f"Picked up {len(globals.round_robin_pairs)} remaining pairs: {globals.round_robin_pairs}")
    globals.logger.debug(f"Updated player paths: {globals.player_to_path}")

def compile_and_move_players():
    """
    Compiles and moves all players to the players directory.
    Looks for directories ending in "_player" and compiles them.
    """
    compiler = "mpicc"
    cflags = "-O2 -g -Wall -Wno-variadic-macros -pedantic -DDEBUG"
    ldflags = "-g"
    ldlibs = ""
    new_dir = "players"

    os.makedirs("players", exist_ok=True)

    # Get all directory names ending in "_player"
    player_names = [name for name in os.listdir() if name.endswith("_player")]

    for player in player_names:
        os.makedirs(f"{player}/obj", exist_ok=True)
        player_source_file = f"{player}/src/{player}.c"
        executable_file = f"{player}/obj/{player}"
        comms_source = f"{player}/src/comms.c"

        # Compile the player
        globals.logger.info(f"Compiling '{player}'")
        compile_command = f"{compiler} {cflags} {ldflags} {ldlibs} -o {executable_file} {player_source_file} {comms_source}"

        # Execute the command and capture stdout and stderr
        result = subprocess.run(compile_command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        # Print the stdout and stderr
        print(result.stdout)
        print(result.stderr)

        if result.returncode != 0:
            globals.logger.error(f"Compilation failed for `{player}`. Exiting now.")
            return False  # Need all players to compile

        # Move the executable to the players directory
        shutil.move(executable_file, os.path.join(new_dir, executable_file.split('/')[-1]))

        # Clean up .dSYM directory
        dsym_directory = f"{executable_file}.dSYM"
        if os.path.isdir(dsym_directory):
            shutil.rmtree(dsym_directory)

    globals.logger.info(f"Compiled {len(player_names)} players - {player_names}")
    return True

def _remove_binary_files(player_name):
    # Get the path to the players directory
    players_dir = os.path.join(globals.BASE_DIRECTORY, "players")

    # Get the list of binary files with the same name as the player
    binary_files = glob.glob(os.path.join(players_dir, f"*{player_name}*"))

    # Remove the binary files
    for file_path in binary_files:
        try:
            os.remove(file_path)
            print(f"Removed file: {file_path}")
        except OSError as e:
            print(f"Error removing file: {file_path}. {e.strerror}")

