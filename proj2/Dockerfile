FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive \
    DEBCONF_NONINTERACTIVE_SEEN=true

RUN apt-get update && \
    apt-get upgrade -y && \
    apt update

RUN apt-get install -y python3 python3-pip python-is-python3 mpich curl vim
RUN apt-get install -y lsof openjdk-21-jdk

# Create IF directory and copy everything
RUN mkdir IngeniousFrame
WORKDIR /IngeniousFrame
COPY . .

ENTRYPOINT ["python"]
CMD ["play.py", "Othello"]
