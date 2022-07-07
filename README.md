## Introduction:
The goal is to implement a forensic protocol ontop of the raft implementation. The cbdc created by MIT will server as a case study to show that performance is not inhibited by this forensic protocol built on top.

## Get the code && run the code
```terminal
git clone --recurse-submodules https://github.com/BozeBro/cbdc-forensics
cd cbdc-forensics
git checkout Byzantine
docker compose –file docker-compose-2pc.yml up –build
```
To run wallet commands, open another terminal window and run
```terminal
docker run --network 2pc-network -ti ghcr.io/mit-dci/opencbdc-tx /bin/bash
```
See oldREADME.md for a runthrough of wallet commands
## How to edit the number and configure raft shards
We will look specifically at configuring the number of 2pc raft shard nodes in the 2pc network. Editing other roles and in different architectures follow a similar procedure.

In file, `2pc-compose.cfg`, specify the count via shard_count. Raft shards are numbered from [0, shard_count).

shardi_count specifies the shard where index i `0 <= i < shard_count`.

For each instance of a raft node, we must define a configuration in the `docker-compose-2pc.yml` file. 


### We'll look specifically at the raft node.

shardi_j specifies the node and its shard. i is the shard id and j is the node id. 
For shardi_j, each must be specified an endpoint, raft_endpoint, and readonly_endpoint. 

An example specification appears as the following, 
```yml
shard0:
    build: .
    image: opencbdc-tx
    tty: true
    command: ./build/src/uhs/twophase/locking_shard/locking-shardd 2pc-compose.cfg 0 0
    expose:
      - "6666"
    ports:
      - 6767:6767
    networks:
      - 2pc-network
    healthcheck:
      test: ["CMD-SHELL", "netstat -ltn | grep -c 6666"]
      interval: 30s
      timeout: 10s
      retries: 5
```
You can copy this configuration to create as many instances as you like. Make sure to change the name shard0 to a different name.
the value after expose represents the endpoint in the .cfg file, and ports represent the read_only port. Follow the same format as the example where the number before and after the colon is the same. 
The first argument to the executable in the command arguement is the shard id and the second argument is the node id.
An example second shard could look like this
```yml
shard1:
    build: .
    image: opencbdc-tx
    tty: true
    command: ./build/src/uhs/twophase/locking_shard/locking-shardd 2pc-compose.cfg 0 1
    expose:
      - "7777"
    ports:
      - 1000:1000
    networks:
      - 2pc-network
    healthcheck:
      test: ["CMD-SHELL", "netstat -ltn | grep -c 7777"]
      interval: 30s
      timeout: 10s
      retries: 5
```
Let's suppose we have 1 shard with 2 raft nodes. This is what the corresponding raft nodes would look like in `2pc-compose.cfg` 
```cpp
2pc=1
shard_count=1
shard0_start=0
shard0_end=255
shard0_count=2
shard0_loglevel="INFO"
shard0_0_endpoint="shard0:6666"
shard0_0_raft_endpoint="shard0:6667"
shard0_0_readonly_endpoint="shard0:6767"
shard0_1_endpoint="shard1:7777"
shard0_1_raft_endpoint="shard1:7778"
shard0_1_readonly_endpoint="shard1:1000"
```
Since the original cbdc paper had `raft_endpoint = endpoint + 1`, we follow the same practice.
## New addtions
We can now add verbose and a byzantine flag to the current sytem to 2pc raft shards. 
To get more debug information from raft nodes, run the debug flag, and to customize the nodes to be byzantine, use the byzantine flag.


Continuing on our previous `2pc-compose.fg`, this is configuration for a normal node and a Byzantine verbose node.
dol stands for (Denial of Leadership)

```cpp
2pc=1
shard_count=1
shard0_start=0
shard0_end=255
shard0_count=2
shard0_loglevel="INFO"
shard0_0_endpoint="shard0:6666"
shard0_0_raft_endpoint="shard0:6667"
shard0_0_readonly_endpoint="shard0:6767"
shard0_0_verbose="false"
shard0_1_endpoint="shard1:7777"
shard0_1_raft_endpoint="shard1:7778"
shard0_1_readonly_endpoint="shard1:1000"
shard0_1_verbose="true"
shard0_1_byzantine="dol"
```
verbose and byzantine flags are set to false by default. 

## Running the system and testing Wallet commands
See oldREADME.md. Ignore section that says Get the code.
