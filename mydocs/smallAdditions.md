## Table of Contents
 1. Structure change
 2. Changes to Apollo
 3. Changes to NuRaft
### Structure changes
Since we are editing both NuRaft 1.3.0 and Apollo, we place the NuRaft directory into the opencbdc directory, and treat it as one large forensics directory.

We change how the Dockerfile is setup because of this change.

    1. We do not grab the library online.
        * cd directly into it
    2. Rearrange order of docker commands, so everything works the same. 
    3. We always have the project in debug mode rather than configure between both Debug and Production.
new Directory called mydocs which gives documentation over new changes and features to the forensics project. 
### Apollo Changes
    1. Adding new parameters to raft_params and raft_server to handle new flags and new variables
        * (byzantine flags, verbose flags, tracking configured shard count)
Apollo is nearly unchanged in its implementation. It mostly receives flags from configuration file and passes it to NuRaft. 
-- Making the raft_params obtain the flags from the configuration file. 
`./src/uhs/twophase/locking_shard/controller.cpp`

```cpp
params.node_id  = static_cast<int>(m_node_id);
params.cluster_id = static_cast<int>(m_shard_id);
params.verbose = m_opts.m_verbose;
params.is_byzantine = m_opts.m_byzantine;  
params.machine_type = "shard";
```

-- Added verbose and byzantine flags to the default struct
Also added array of nodes with verbose and byzantine flags.
`./src/util/common/config.hpp`
```cpp
static constexpr auto verbose   = "verbose";
static constexpr auto byzantine = "byzantine";
...
/// List of locking shard verbose flag, set to false by default, 
/// ordered by shard ID then node ID. 
bool m_verbose;
/// List of locking shard byzantine flag, set to false by default, 
/// ordered by shard ID then node ID. 
std::string m_byzantine;
```

-- Loading the verbose and byzantine flags in `./src/util/common/config.cpp`. We create a new function that curently reads the byzantine and verbose flags. and returns an options class that contains byzantine, verbose and future flags.
```cpp 
auto load_flags(std::string& machine, size_t cluster_id, size_t node_id, std::string& config_file) 
        -> std::variant<options, std::string>{
        bool verb;
        std::string byz;
        auto opts = options{};
        auto cfg = parser(config_file);
        if (machine == "shard") {
            verb = cfg.get_flag(get_shard_flag_key(verbose, cluster_id, node_id)) == "true";
            byz = cfg.get_flag(get_shard_flag_key(byzantine, cluster_id, node_id));
        }
        else if (machine == "coordinator") {
            verb = cfg.get_flag(get_coordinator_flag_key(verbose, cluster_id, node_id)) == "true";
            byz = cfg.get_flag(get_coordinator_flag_key(byzantine, cluster_id, node_id));
        }
        opts.m_byzantine = byz;
        opts.m_verbose = verb;
        return opts; 
    }
```
### NuRaft changes

In `Nuraft-1.3.0-Debug/include/libnuraft/raft_params.hxx` and `Nuraft-1.3.0-Debug/include/libnuraft/raft_server.hxx`

we add the verbose, is_byzantine, machine_type, node_id, shard_id memebers to the **raft_server** and **raft_params** class
```cpp
/**
 * If True, outputs more debugger messages.
 */
bool verbose; 
/**
 * If True, uses the byzantine class.
 */
bool is_byzantine;
/**
 * The type of role that the raft node represents
 * E.g. Coordinator, raft shard node, 
 */
const std::string machine_type;
/**
 * The index of a raft node in a particular cluster.
 */
int32 node_id;
/**
 * If a raft, shard node, the index of the shard
 */
int32 shard_id; 
```

Add `verbose_server.hxx` implementation in `nuraft.hxx`
Add `verbose_server.cxx` to source files in `CMakeLists.txt`

Have launcher.cxx case on whether verbose output or not.
```cpp
if (params_given.verbose) {
        raft_instance_ = cs_new<verbose_server>(ctx, opt, true);
    } else {
        raft_instance_ = cs_new<raft_server>(ctx, opt);
    }
```

In `controller.cpp` for the raft shard,
include details about node id, shard id, verbose flag, byzantine flag and machine type (shard)
```cpp
params.node_id  = static_cast<int>(m_node_id);
params.shard_id = static_cast<int>(m_shard_id);
params.verbose = m_opts.m_verbose;
params.is_byzantine = m_opts.m_byzantine; 
params.machine_type = "shard";   
```
its "coordinator" when referencing the coordinator node

#### Verbose Server
There is a verbose server which is a standard nuRaft server that outputs more messages to the console. Nice for seeing control flow, and debugging. 
