### Changes
I will be removing changes that no longer exist 


-- Making the raft_params obtain the flags from the configuration file. 
`./src/uhs/twophase/locking_shard/controller.cpp`

```cpp
params.node_id  = m_node_id; 
params.shard_id = m_shard_id;
params.verbose  = m_opts.m_verbose; 
params.is_byzantine = m_opts.m_byzantine; 
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
std::vector<std::vector<bool>> m_verbose;
/// List of locking shard byzantine flag, set to false by default, 
/// ordered by shard ID then node ID. 
std::vector<std::vector<bool>> m_byzantine;
```

-- Loading the verbose and byzantine flags in `./src/util/common/config.cpp`. We edit the `read_shard_endpoints` function since it contains the shard id and the node id of each raft node in a shard. It also edits the code very little. The functionality of the code is not changed from the original. 
New additions to the function
```cpp 
    opts.m_verbose.resize(shard_count); 
    opts.m_byzantine.resize(shard_count); 
    // ...
    // Within the inner for loop
    // Adding custome Byzantine flags underneath. 
    const auto verb_key 
        = get_shard_flag_key(verbose, i, j);
    bool verb     = cfg.get_flag(verb_key);
    opts.m_verbose[i].emplace_back(verb); 

    const auto byz_key 
        = get_shard_flag_key(byzantine, i, j);
    bool byz     = cfg.get_flag(byz_key);
    opts.m_byzantine[i].emplace_back(byz); 
```

-- We preinstall the NuRaft library installing NuRaft at runtime. The Dockerfile and scripts/configure.sh are changed to reflect this fact. 
In `Nuraft-1.3.0-Debug/include/libnuraft/raft_params.hxx` and `Nuraft-1.3.0-Debug/include/libnuraft/raft_server.hxx`

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
params.verbose  = m_opts.m_verbose[m_shard_id][m_node_id];
params.is_byzantine = m_opts.m_byzantine[m_shard_id][m_node_id];  
params.machine_type = "shard";     
```