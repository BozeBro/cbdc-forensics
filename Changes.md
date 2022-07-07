### Changes
I will be removing changes that no longer exist 


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
params.verbose = m_opts.m_verbose;
params.is_byzantine = m_opts.m_byzantine; 
params.machine_type = "shard";   
```
its "coordinator" when referencing the coordinator node

The byzantine node (DDoS) contains three fundamental changes to note. 
1. For every request it receives from the leader, it will first process the request, and then initiate a vote. 
```cpp
    if (peers_.size() >= 4) initiate_vote();
    return resp;
```
In the future, I'll add a variable that maintains what the total size should be.
2. When notified that it has sent too many prevote requests, it will ignore reconnection.
```cpp
void byz_server::handle_prevote_resp(resp_msg& resp) {
...
if (pre_vote_.live_ >= election_quorum_size) {
        // pre_vote_.quorum_reject_count_.fetch_add(1);
        p_wn("[PRE-VOTE] rejected by quorum, count %zu",
             pre_vote_.quorum_reject_count_.load());
        if ( pre_vote_.quorum_reject_count_ >=
                 raft_server::raft_limits_.pre_vote_rejection_limit_ ) {
            // p_ft("too many pre-vote rejections, probably this node is not "
            //     "receiving heartbeat from leader. "
            //     "we should re-establish the network connection");
            //raft_server::send_reconnect_request();
        }
... 
}
```
3. When it has been declared the leader, it will ignore the leader election and start the election again.