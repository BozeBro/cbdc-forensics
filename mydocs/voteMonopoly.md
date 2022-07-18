## Implementation
Here we discuss how we actually implement NuRaft to have this Vote Monopoly (VM) Node.

### Key changes
    1. Deciding when to vote.
    2. Byzantine node receives quorum.
### Deciding when to vote
VM starts an election, whenever it notices a leader is alive. 
```cpp
ptr<resp_msg> byz_server::process_req(req_msg& req) {
    ...
    // Start only after all nodes are added.
    // TODO: work with actaul crashed nodes.
    // valid : only intiate vote when req is from leader. Via ping, append_entries
    // maybe priority change request
    if (peers_.size() >= peer_size - 1 && get_leader() == req.get_src()) {
        byz_server::initiate_vote();
    }
    return resp;
}
```
process_req handles all incoming requests to a server. Therefore, detect when it is a request from a leader. Then we know that a leader exists, and we will initiate a leader election. 
We pause Byzantine action until all nodes are initially added. Otherwise, nodes crash. This crash behavior is unintended, and is more likely of a implementation casuse than a Byzantine cause.

### Receiving quorum

When VM receives the quorum to become leader, it will simply act like it has not received quorum, and initiate another election. Again, VM lies about timing out and receiving prevotes. The process is restarted.

```cpp
void byz_server::handle_vote_resp(resp_msg& resp) {
... 
 if (votes_granted_ >= election_quorum_size) {
        election_completed_ = true;
        initiate_vote();
        //restart_election_timer();
        // p_in("  === LEADER (term %zu) ===\n", state_->get_term());
    }
    ...
}
```
Note, we change `restart_election_timer()` to `initiate_vote()` in this function so its like we timeout again and we initiate voting.