#ifndef _VERBOSE_SERVER_HXX_
#define _VERBOSE_SERVER_HXX_

#include "raft_server.hxx"
namespace nuraft {
class verbose_server : public raft_server {
    using raft_server::raft_server; 
    ptr<raft_server> node; 
    public:
        void initialize(ptr<raft_server> raft) {
            node = raft; 
        }
        /**
     * Process Raft request.
     *
     * @param req Request.
     * @return Response.
     */
    virtual ptr<resp_msg> process_req(req_msg& req);

    /**
     * Check if this server is ready to serve operation.
     *
     * @return `true` if it is ready.
     */
    bool is_initialized() const { return initialized_; }

    /**
     * Check if this server is catching up the current leader
     * to join the cluster.
     *
     * @return `true` if it is in catch-up mode.
     */
    bool is_catching_up() const { return catching_up_; }

    /**
     * Check if this server is receiving snapshot from leader.
     *
     * @return `true` if it is receiving snapshot.
     */
    bool is_receiving_snapshot() const { return receiving_snapshot_; }

    /**
     * Add a new server to the current cluster.
     * Only leader will accept this operation.
     * Note that this is an asynchronous task so that needs more network
     * communications. Returning this function does not guarantee
     * adding the server.
     *
     * @param srv Configuration of server to add.
     * @return `get_accepted()` will be true on success.
     */
    ptr< cmd_result< ptr<buffer> > >
        add_srv(const srv_config& srv);

    /**
     * Remove a server from the current cluster.
     * Only leader will accept this operation.
     * The same as `add_srv`, this is also an asynchronous task.
     *
     * @param srv_id ID of server to remove.
     * @return `get_accepted()` will be true on success.
     */
    ptr< cmd_result< ptr<buffer> > >
        remove_srv(const int srv_id);

    /**
     * Append and replicate the given logs.
     * Only leader will accept this operation.
     *
     * @param logs Set of logs to replicate.
     * @return
     *     In blocking mode, it will be blocked during replication, and
     *     return `cmd_result` instance which contains the commit results from
     *     the state machine.
     *     In async mode, this function will return immediately, and the
     *     commit results will be set to returned `cmd_result` instance later.
     */
    ptr< cmd_result< ptr<buffer> > >
        append_entries(const std::vector< ptr<buffer> >& logs);

    /**
     * Update the priority of given server.
     * Only leader will accept this operation.
     *
     * @param srv_id ID of server to update priority.
     * @param new_priority
     *     Priority value, greater than or equal to 0.
     *     If priority is set to 0, this server will never be a leader.
     */
    void set_priority(const int srv_id, const int new_priority);

    /**
     * Broadcast the priority change of given server to all peers.
     * This function should be used only when there is no live leader
     * and leader election is blocked by priorities of live followers.
     * In that case, we are not able to change priority by using
     * normal `set_priority` operation.
     *
     * @param srv_id ID of server to update priority.
     * @param new_priority New priority.
     */
    void broadcast_priority_change(const int srv_id,
                                   const int new_priority);

    /**
     * Yield current leadership and becomes a follower. Only a leader
     * will accept this operation.
     *
     * If given `immediate_yield` flag is `true`, it will become a
     * follower immediately. The subsequent leader election will be
     * totally random so that there is always a chance that this
     * server becomes the next leader again.
     *
     * Otherwise, this server will pause write operations first, wait
     * until the successor (except for this server) finishes the
     * catch-up of the latest log, and then resign. In such a case,
     * the next leader will be much more predictable.
     *
     * Users can designate the successor. If not given, this API will
     * automatically choose the highest priority server as a successor.
     *
     * @param immediate_yield If `true`, yield immediately.
     * @param successor_id The server ID of the successor.
     *                     If `-1`, the successor will be chosen
     *                     automatically.
     */
    void yield_leadership(bool immediate_yield = false,
                          int successor_id = -1);

    /**
     * Send a request to the current leader to yield its leadership,
     * and become the next leader.
     *
     * @return `true` on success. But it does not guarantee to become
     *         the next leader due to various failures.
     */
    bool request_leadership();

    /**
     * Start the election timer on this server, if this server is a follower.
     * It will allow the election timer permanently, if it was disabled
     * by state manager.
     */
    void restart_election_timer();

    /**
     * Set custom context to Raft cluster config.
     *
     * @param ctx Custom context.
     */
    void set_user_ctx(const std::string& ctx);

    /**
     * Get custom context from the current cluster config.
     *
     * @return Custom context.
     */
    std::string get_user_ctx() const;

    /**
     * Get ID of this server.
     *
     * @return Server ID.
     */
    int32 get_id() const
    { return id_; }

    /**
     * Get the current term of this server.
     *
     * @return Term.
     */
    ulong get_term() const
    { return state_->get_term(); }

    /**
     * Get the term of given log index number.
     *
     * @param log_idx Log index number
     * @return Term of given log.
     */
    ulong get_log_term(ulong log_idx) const
    { return log_store_->term_at(log_idx); }

    /**
     * Get the term of the last log.
     *
     * @return Term of the last log.
     */
    ulong get_last_log_term() const
    { return log_store_->term_at(get_last_log_idx()); }

    /**
     * Get the last log index number.
     *
     * @return Last log index number.
     */
    ulong get_last_log_idx() const
    { return log_store_->next_slot() - 1; }

    /**
     * Get the last committed log index number of state machine.
     *
     * @return Last committed log index number of state machine.
     */
    ulong get_committed_log_idx() const
    { return sm_commit_index_.load(); }

    /**
     * Get the target log index number we are required to commit.
     *
     * @return Target committed log index number.
     */
    ulong get_target_committed_log_idx() const
    { return quick_commit_index_.load(); }

    /**
     * Get the leader's last committed log index number.
     *
     * @return The leader's last committed log index number.
     */
    ulong get_leader_committed_log_idx() const
    { return is_leader() ? get_committed_log_idx() : leader_commit_index_.load(); }

    /**
     * Calculate the log index to be committed
     * from current peers' matched indexes.
     *
     * @return Expected committed log index.
     */
    ulong get_expected_committed_log_idx();

    /**
     * Get the current Raft cluster config.
     *
     * @return Cluster config.
     */
    ptr<cluster_config> get_config() const;

    /**
     * Get log store instance.
     *
     * @return Log store instance.
     */
    ptr<log_store> get_log_store() const { return log_store_; }

    /**
     * Get data center ID of the given server.
     *
     * @param srv_id Server ID.
     * @return -1 if given server ID does not exist.
     *          0 if data center ID was not assigned.
     */
    int32 get_dc_id(int32 srv_id) const;

    /**
     * Get auxiliary context stored in the server config.
     *
     * @param srv_id Server ID.
     * @return Auxiliary context.
     */
    std::string get_aux(int32 srv_id) const ;

    /**
     * Get the ID of current leader.
     *
     * @return Leader ID
     *         -1 if there is no live leader.
     */
    int32 get_leader() const {
        // We should handle the case when `role_` is already
        // updated, but `leader_` value is stale.
        if ( leader_ == id_ &&
             role_ != srv_role::leader ) return -1;
        return leader_;
    }

    /**
     * Check if this server is leader.
     *
     * @return `true` if it is leader.
     */
    bool is_leader() const {
        if ( leader_ == id_ &&
             role_ == srv_role::leader ) return true;
        return false;
    }

    /**
     * Check if there is live leader in the current cluster.
     *
     * @return `true` if live leader exists.
     */
    bool is_leader_alive() const {
        if ( leader_ == -1 || !hb_alive_ ) return false;
        return true;
    }

    /**
     * Get the configuration of given server.
     *
     * @param srv_id Server ID.
     * @return Server configuration.
     */
    ptr<srv_config> get_srv_config(int32 srv_id) const;

    /**
     * Get the configuration of all servers.
     *
     * @param[out] configs_out Set of server configurations.
     */
    void get_srv_config_all(std::vector< ptr<srv_config> >& configs_out) const;

    /**
     * Peer info structure.
     */
    struct peer_info {
        peer_info()
            : id_(-1)
            , last_log_idx_(0)
            , last_succ_resp_us_(0)
            {}

        /**
         * Peer ID.
         */
        int32 id_;

        /**
         * The last log index that the peer has, from this server's point of view.
         */
        ulong last_log_idx_;

        /**
         * The elapsed time since the last successful response from this peer,
         * in microsecond.
         */
        ulong last_succ_resp_us_;
    };

    /**
     * Get the peer info of the given ID. Only leader will return peer info.
     *
     * @param srv_id Server ID.
     * @return Peer info.
     */
    peer_info get_peer_info(int32 srv_id) const;

    /**
     * Get the info of all peers. Only leader will return peer info.
     *
     * @return Vector of peer info.
     */
    std::vector<peer_info> get_peer_info_all() const;

    /**
     * Shut down server instance.
     */
    void shutdown();

    /**
     *  Start internal background threads, initialize election
     */
    void start_server(bool skip_initial_election_timeout);

    /**
     * Stop background commit thread.
     */
    void stop_server();

    /**
     * Send reconnect request to leader.
     * Leader will re-establish the connection to this server in a few seconds.
     * Only follower will accept this operation.
     */
    void send_reconnect_request();

    /**
     * Update Raft parameters.
     *
     * @param new_params Parameters to set.
     */
    void update_params(const raft_params& new_params);

    /**
     * Get the current Raft parameters.
     * Returned instance is the clone of the original one,
     * so that user can modify its contents.
     *
     * @return Clone of Raft parameters.
     */
    raft_params get_current_params() const;

    /**
     * Get the counter number of given stat name.
     *
     * @param name Stat name to retrieve.
     * @return Counter value.
     */
    static uint64_t get_stat_counter(const std::string& name);

    /**
     * Get the gauge number of given stat name.
     *
     * @param name Stat name to retrieve.
     * @return Gauge value.
     */
    static int64_t get_stat_gauge(const std::string& name);

    /**
     * Get the histogram of given stat name.
     *
     * @param name Stat name to retrieve.
     * @param[out] histogram_out
     *     Histogram as a map. Key is the upper bound of a bucket, and
     *     value is the counter of that bucket.
     * @return `true` on success.
     *         `false` if stat does not exist, or is not histogram type.
     */
    static bool get_stat_histogram(const std::string& name,
                                   std::map<double, uint64_t>& histogram_out);

    /**
     * Reset given stat to zero.
     *
     * @param name Stat name to reset.
     */
    static void reset_stat(const std::string& name);

    /**
     * Reset all existing stats to zero.
     */
    static void reset_all_stats();

    /**
     * Apply a log entry containing configuration change, while Raft
     * server is not running.
     * This API is only for recovery purpose, and user should
     * make sure that when Raft server starts, the last committed
     * index should be equal to or bigger than the index number of
     * the last configuration log entry applied.
     *
     * @param le Log entry containing configuration change.
     * @param s_mgr State manager instance.
     * @param err_msg Will contain a message if error happens.
     * @return `true` on success.
     */
    static bool apply_config_log_entry(ptr<log_entry>& le,
                                       ptr<state_mgr>& s_mgr,
                                       std::string& err_msg);

    /**
     * Get the current Raft limit values.
     *
     * @return Raft limit values.
     */
    static limits get_raft_limits();

    /**
     * Update the Raft limits with given values.
     *
     * @param new_limits New values to set.
     */
    static void set_raft_limits(const limits& new_limits);

    /**
     * Invoke internal callback function given by user,
     * with given type and parameters.
     *
     * @param type Callback event type.
     * @param param Parameters.
     * @return cb_func::ReturnCode.
     */
    CbReturnCode invoke_callback(cb_func::Type type,
                                 cb_func::Param* param);

    /**
     * Set a custom callback function for increasing term.
     */
    void set_inc_term_func(srv_state::inc_term_func func);
}; 
}

#endif // _VERBOSE_SERVER_HXX_