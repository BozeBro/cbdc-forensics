#include "verbose_server.hxx"
#include "tracer.hxx"

#include <sstream>
#include <string>

namespace nuraft {
   void verbose_server::printer() {
      if (!debug) return; 
      std::stringstream ss; 
      ss << "machine type: " << machine_type << "\n";
      if (shard_id >= 0) {
         ss << "shard id:" << shard_id << "\n";
      }
      ss << "node id: " << node_id << "\n";
      std::string print_msg = ss.str();
      p_in("%s", print_msg.c_str());
   }
   void verbose_server::print_msg(std::string ss) {
      if (!debug) return;
      p_in(ss.c_str());
   }
   verbose_server::verbose_server(context* ctx, const init_options& opt, bool verbose) 
   : raft_server::raft_server(ctx, opt)
   , debug(verbose)
   {}


   ptr<resp_msg> verbose_server::process_req(req_msg& req) {
      printer(); 
      std::stringstream ss; 
      ss << "process the request with last log index " 
         << req.get_last_log_idx() << "\n";
      print_msg(ss.str()); 
      return raft_server::process_req(req);
   }

   ptr< cmd_result< ptr<buffer> > >
        verbose_server::add_srv(const srv_config& srv) {
         printer(); 
         std::stringstream ss; 
         ss << "is node a leader? " << is_leader() 
            << "\nAdding server with id " 
            << srv.get_id() << " to the cluster.\n";
         print_msg(ss.str());
         return raft_server::add_srv(srv); 
        }

   ptr< cmd_result< ptr<buffer> > >
        verbose_server::remove_srv(const int srv_id) {
         printer();
         std::stringstream ss; 
         ss << "is node a leader? " << is_leader() 
            << "\nRemoving server with id " 
            << srv_id << " from the cluster.\n";
         print_msg(ss.str());
         return raft_server::remove_srv(srv_id);
        }
      
   ptr< cmd_result< ptr<buffer> > >
        verbose_server::append_entries(const std::vector< ptr<buffer> >& logs) {
         printer();
         std::stringstream ss;
         ss << "Is node a leader? " << is_leader()
            << "\nAttempting to append and replicate logs\n"
            << "With data: " << logs.data() << "\n";
         print_msg(ss.str());
         return raft_server::append_entries(logs);
        }

   void verbose_server::yield_leadership(bool immediate_yield,
                          int successor_id) {
      printer();
      std::stringstream ss; 
      ss << "Is node a leader? " << is_leader()
         << "\nBecoming a follower\n";
      p_in(ss.str().c_str());
      return raft_server::yield_leadership(immediate_yield, successor_id);
         }

   bool verbose_server::request_leadership() {
      printer();
      std::stringstream ss; 
      ss << "Requesting to become a leader\n"; 
      print_msg(ss.str());
      bool is_success = raft_server::request_leadership(); 
      print_msg(is_success ? "true" : "false");
      return is_success; 
   }
   void verbose_server::restart_election_timer() {
      printer();
      std::stringstream ss; 
      ss << "Restarting the election timer\n";
      print_msg(ss.str());
      raft_server::restart_election_timer();
   }

   void verbose_server::shutdown() {
      printer();
      std::stringstream ss; 
      ss << "Shutting down this server\n"; 
      print_msg(ss.str());
      raft_server::shutdown();  
   }

   void verbose_server::stop_server() {
      printer();
      std::stringstream ss;
      ss << "Stopping background commit thread\n";
      print_msg(ss.str());
      raft_server::stop_server(); 
   }
}  

