## Table of Contents
  - Current New Flags
  - Run the Code
  - Configuring nodes
  - Crash Nodes

### Flags 
1. verbose - "true" or "false"
  - "false" by default
  - shard0_0_verbose="true"
2. byzantine - "vm" or "normal"
  - "normal" by default
  - shard1_0_byzantine="vm"
### Get the code && run the code
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
See oldREADME.md for wallet commands
### Configuring nodes
To edit configure shard nodes in 2pc architecture, we must edit `docker-compose-2pc.yml` and `2pc-compose.cfg`
Here is an example `2pc-compose.cfg` will relevant shard information. 
```cpp
shard_count=1
shard0_start=0
shard0_end=255
shard0_count=4

shard0_loglevel="INFO"

shard0_0_endpoint="shard00:6666"
shard0_0_raft_endpoint="shard00:6667"
shard0_0_readonly_endpoint="shard00:6767"

shard0_1_endpoint="shard01:6777"
shard0_1_raft_endpoint="shard01:6668"
shard0_1_readonly_endpoint="shard01:7777"

shard0_2_endpoint="shard02:6888"
shard0_2_raft_endpoint="shard02:6889"
shard0_2_readonly_endpoint="shard02:8787"
shard0_2_byzantine="vm"

shard0_3_endpoint="shard03:7888"
shard0_3_raft_endpoint="shard03:7889"
shard0_3_readonly_endpoint="shard03:1111"
```
In the file, we have 1 shard, and that shard has 5 replicated nodes. 
The yml file makes sure that 5 services are started for shards.
Here is a configuration for shard 0, node 2.
```yml
shard02:
    build: .
    image: opencbdc-tx
    tty: true
    command: ./build/src/uhs/twophase/locking_shard/locking-shardd 2pc-compose.cfg 0 2
    expose:
      - "6888"
    ports:
      - 8787:8787
    networks:
      - 2pc-network
    healthcheck:
      test: ["CMD-SHELL", "netstat -ltn | grep -c 6888"]
      interval: 30s
      timeout: 10s
      retries: 5
```

Always start each argument to `shard0_2_x` with "shard02:"


The argument to expose has same number as `shard0_2_endpoint`

The ports has the same number as `shard0_2_readonly_endpoint`

In the command arguement in the yml file, 
```yml
command: ./build/src/uhs/twophase/locking_shard/locking-shardd 2pc-compose.cfg 0 2
```
The third argument designates the shard number, and the fourth argument is the node id.

### Crash Nodes
When running the system, one can see the raft nodes via
```terminal
docker ps
```
For example
```terminal
CONTAINER ID   IMAGE         COMMAND                  CREATED        STATUS                    PORTS                              NAMES
fa92a21f413f   opencbdc-tx   "./build/src/uhs/two…"   27 hours ago   Up 27 hours (healthy)     0.0.0.0:5555->5555/tcp             cbdc-forensics-sentinel0-1
fd44bd8b5416   opencbdc-tx   "./build/src/uhs/two…"   27 hours ago   Up 27 hours (healthy)     1111/tcp                           cbdc-forensics-coordinator00-1
b1edaf7f9d8d   opencbdc-tx   "./build/src/uhs/two…"   27 hours ago   Up 27 hours (unhealthy)   2222/tcp                           cbdc-forensics-coordinator01-1
b504401986ba   opencbdc-tx   "./build/src/uhs/two…"   27 hours ago   Up 27 hours (unhealthy)   6666/tcp, 0.0.0.0:6767->6767/tcp   cbdc-forensics-shard00-1
b867de05a151   opencbdc-tx   "./build/src/uhs/two…"   27 hours ago   Up 27 hours (healthy)     6888/tcp, 0.0.0.0:8787->8787/tcp   cbdc-forensics-shard02-1
3d222c9577c9   opencbdc-tx   "./build/src/uhs/two…"   27 hours ago   Up 27 hours (unhealthy)   0.0.0.0:1111->1111/tcp, 7888/tcp   cbdc-forensics-shard03-1
bffce768658a   opencbdc-tx   "./build/src/uhs/two…"   27 hours ago   Up 27 hours (unhealthy)   6777/tcp, 0.0.0.0:7777->7777/tcp   cbdc-forensics-shard01-1
```

To crash a node, use the command
```terminal
docker stop NAME
```
where NAME is one the names listed under NAMES.

```terminal
docker stop cbdc-forensics-shard03-1
```
would stop the shard 0, node 3.