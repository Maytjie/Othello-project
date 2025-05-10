GREEN='\033[1;32m'
NC='\033[0m'

# Stop the current ingenious container if it is running
printf "\n${GREEN}Stopping current container...${NC}\n"
docker stop othello

# Remove the current container
printf "\n${GREEN}Removing current container...${NC}\n"
docker image rmi -f othello

# Build new docker image
printf "\n${GREEN}Building new docker image...${NC}\n"
docker build -t othello .

# Remove old docker image
printf "\n${GREEN}Removing old docker image...${NC}\n"
echo "y" | docker image prune
docker image rmi -f $(docker images -f "dangling=true" -q)

# Create and run a new ingenious-frame container
printf "\n${GREEN}Running Othello...${NC}\n"

docker run -t -v ${PWD}/IngeniousFrame/Logs:/IngeniousFrame/Logs othello

printf "\n${GREEN}Cleaning up...${NC}\n"
docker image rmi -f othello

printf "\n${GREEN}Done.${NC}\n"
