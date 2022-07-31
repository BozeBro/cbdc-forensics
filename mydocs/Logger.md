## Simple Logger

We create a Log wrapper over raft node functionality to provide human readable log messages.

Log inherits from the raft class such that we can replace initiation of a raft node with our log wrapper.

The wrapper logs certain activities to the counter. They are:

1.  null_state
2.  req_prevote
3.  reject_prevote
4.  accept_prevote
5.  req_vote
6.  accept_vote
7.  reject_vote
8.  pre_commit_log
9.  commit_log
10. be_leader
11. be_follower
12. ignore_votes

For activities that are interacting with a request, they will include the term number and the src of the request.
Null state represents the initial state of a byzantine node. The wrapper will do nothing upon seeing this activity.

ignore_votes reprsents vote monopoly byzantine behavior. A node receives a quorum of votes but chooses not to become leader.

See the implementation in [logs.hxx](/NuRaft-1.3.0-Debug/include/libnuraft/logs.hxx) and [log.cxx](/NuRaft-1.3.0-Debug/src/logs.cxx)
