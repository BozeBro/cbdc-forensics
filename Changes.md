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


