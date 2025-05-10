import re
import csv
import json
import asyncio
from common import globals

async def start_lobby(lobby_id, player1, player2, sem, lock):

    await lock.acquire()
    await sem.acquire()
    write_to_config_and_results(lobby_id, player1, player2)

    command = f'java -jar {globals.ADD_OPENS} {globals.FRAMEWORK_JAR[globals.JAVA_VERSION]} create ' + \
        f'-config "{globals.GAME_NAME}.json" -game "{globals.GAME_REFEREE[globals.GAME_NAME]}" ' + \
        f'-lobby "{lobby_id}"'

    globals.logger.debug(f"Starting lobby with command: {command}")
    
    process = await asyncio.create_subprocess_shell(command, 
                        stdout=asyncio.subprocess.PIPE, 
                        stderr=asyncio.subprocess.PIPE)

    # Monitor the output of the lobby process
    async def read_output(stream):
        while True:
            line = await stream.readline()
            if not line :
                break
            print("[start_lobby]", line.decode().strip())


    stdout_task = asyncio.create_task(read_output(process.stdout))
    stderr_task = asyncio.create_task(read_output(process.stderr))

    await asyncio.gather(stdout_task, stderr_task, return_exceptions=True)

def write_to_config_and_results(lobby_id, player1, player2):
    # Write to JSON config file
    globals.CONFIG["player1Path"] = f"players/{player1}"
    globals.CONFIG["player2Path"] = f"players/{player2}"

    with open(f"{globals.GAME_NAME}.json", "w") as file:
        json.dump(globals.CONFIG, file, indent=2)

    # Write Lobby ID to csv
    # Choice to read in results.csv, rather than globally storing it,
    # is for results.csv to act as a live log, and avoid rerunning 
    # the script in the case of crashes.
    data = []
    with open(globals.RESULTS_CSV_PATH, "r") as file:
        reader = csv.reader(file)
        for row in reader:
            data.append(row)

        line_found = False
        for row in data:
            if row[1] == player1 and row[2] == player2:
                row[0] = lobby_id
                line_found = True
                break

    with open(globals.RESULTS_CSV_PATH, "w") as file:
        writer = csv.writer(file)
        writer.writerows(data)


    if line_found:
        globals.logger.debug(f"Found player pair in results.csv, " + \
        f"and updated lobby_id to {lobby_id}")
    else:
        globals.logger.error("Could not find player pair")
        exit(1)

async def connect_players(lobby_id, sem, double_lock):
    player1UI = f'{globals.CONFIG["player1Path"][8:]}-{lobby_id}'
    player2UI = f'{globals.CONFIG["player2Path"][8:]}-{lobby_id}'
    
    player_command = lambda playerUI: f'java -jar {globals.ADD_OPENS} {globals.FRAMEWORK_JAR[globals.JAVA_VERSION]} client -username "{playerUI}" ' + \
        f'-engine "{globals.GAME_PATH[globals.GAME_NAME]}" ' + \
        f' -game "{globals.GAME_REFEREE[globals.GAME_NAME]}" -lobby "{lobby_id}" -hostname localhost -port 61234'

    async def read_output(stream):
        initialised_pattern = r'\d{2}:\d{2}  INFO: Player \(\d\) initialised\.'
        try:
            while True:
                line = await stream.readline()
                if not line or globals.GAME_CRASH:
                    break

                decoded_line = line.decode().strip()

                if "ERROR" in decoded_line:
                    globals.GAME_CRASH = True
                    globals.GAME_CRASH_MESSAGE = decoded_line
                    double_lock.release()
                    break
                
                if re.search(initialised_pattern, decoded_line):
                    double_lock.release()

                    globals.logger.debug("DoubleLock released")
        finally:
            sem.release()
            globals.logger.debug("Semephore released")
                
    players = []
    for player in [player1UI, player2UI]:
        globals.logger.debug(f"Executing subprocess_shell with command: {player_command(player)}")
        process = await asyncio.create_subprocess_shell(player_command(player),
                            stdout=asyncio.subprocess.PIPE,
                            stderr=asyncio.subprocess.PIPE)
        stdout_task = asyncio.create_task(read_output(process.stdout))
        # stderr_task = asyncio.create_task(read_output(process.stderr))
        players.append(stdout_task)
        # players.append(stderr_task)

    await asyncio.gather(*players)

