from common import globals
import asyncio
from pathlib import Path
import os
import re
import csv

async def get_server_process():
    """
    Starts the server and captures its output.
    """
    Path("./IngeniousFrame").mkdir(parents=True, exist_ok=True)

    assert(os.path.exists("./IngeniousFrame"))
    globals.logger.debug("\"IngeniousFrame\" directory exists.")

    command = f"java -jar {globals.ADD_OPENS} {globals.FRAMEWORK_JAR.get(globals.JAVA_VERSION)} server"
    globals.logger.debug(f"Launching server with command {command}")
    process = await asyncio.create_subprocess_shell(
        command,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE
    )
    
    async def read_output(stream, csv_file=globals.RESULTS_CSV_PATH):
        match_count = 0
        while True:
            line = await stream.readline()
            if not line or match_count >= globals.MATCHES_TO_PLAY or globals.GAME_CRASH:
                break
            decoded_line = line.decode().strip()
            print(decoded_line)
            if "ERROR" in decoded_line:
                globals.GAME_CRASH = True
                globals.GAME_CRASH_MESSAGE = decoded_line
                for _ in range(60):
                    line = await stream.readline()
                    decoded_line = line.decode().strip()
                    print(decoded_line)
                break

            if "Game has terminated!" in decoded_line:
                match_count += 1

                # Read the next three lines
                scores_line = await stream.readline()
                player1_line = await stream.readline()
                player2_line = await stream.readline()
                print(scores_line.decode().strip())
                print(player1_line.decode().strip())
                print(player2_line.decode().strip())

                # Extract player names, UUIDs, and chip counts using regular expressions
                player1_match = re.search(globals.GAME_PATTERN[globals.GAME_NAME], player1_line.decode())
                player2_match = re.search(globals.GAME_PATTERN[globals.GAME_NAME], player2_line.decode())

                if player1_match and player2_match:
                    player1_name = player1_match.group(1)
                    player1_uuid = player1_match.group(2)
                    player1_chips = float(player1_match.group(3))
                    player2_name = player2_match.group(1)
                    player2_uuid = player2_match.group(2)
                    player2_chips = float(player2_match.group(3))

                    # Update the CSV file
                    with open(csv_file, 'r') as file:
                        reader = csv.reader(file)
                        rows = list(reader)

                        # Find the row corresponding to the player names
                        for row in rows:
                            if row[1] == player1_name and row[2] == player2_name:
                                # Append the chip counts to the existing values
                                if row[3]:
                                    row[5] = str(player1_chips)
                                else:
                                    row[3] = str(player1_chips)
                                if row[4]:
                                    row[6] = str(player2_chips)
                                else:
                                    row[4] = str(player2_chips)
                                break
                            elif row[1] == player2_name and row[2] == player1_name:
                                if row[3]:
                                    row[5] = str(player2_chips)
                                else:
                                    row[3] = str(player2_chips)
                                if row[4]:
                                    row[6] = str(player1_chips)
                                else:
                                    row[4] = str(player1_chips)
                                break

                    with open(csv_file, 'w', newline='') as file:
                        writer = csv.writer(file)
                        writer.writerows(rows)

                    globals.logger.debug(f"Updated CSV - Player 1: {player1_name}, Player 2: {player2_name}")

    stdout_task = asyncio.create_task(read_output(process.stdout))
    # stderr_task = asyncio.create_task(read_output(process.stderr))

    await asyncio.gather(stdout_task, return_exceptions=True)
 
    return process # Not doing anything atm.

