#include "logs.hxx"
#include "req_msg.hxx"
#include <vector>
#include <iostream>
namespace nuraft{
    void log_server::set_state(activity act, req_msg& req) {
        switch (act) {
            // prevote
            case reject_prevote: {
                std::cout << "Rejecting prevote from " << req.get_src() << '\n';
                return;
            }
            case accept_prevote: {
                std::cout << "Accepting prevote from " << req.get_src() << '\n';
                return;
            }
            case accept_vote: {
                std::cout << "Accepting vote from " << req.get_src() << '\n';
            }
            case reject_vote: {
                std::cout << "Rejecting vote from " << req.get_src() << '\n';
                return; 
            }
            default:
                break;
        }
    }
    void log_server::set_state(activity act) {
        switch (act)
        {
        case be_leader: {
            std::cout << "Becoming Leader\n";
            return;
        }
        case be_follower: {
            std::cout << "Becoming Follower\n";
            return;
        }
        case req_prevote: {
                std::cout << "Sending PreVote to peers\n";
                return; 
            }
        case req_vote: {
                std::cout << "Sending Vote to peers\n";
                return;
            }
        case null_state:
        case ignore_votes:
        case pre_commit_log:
        default:
                break;
        }
    }
        void log_server::set_state(activity act, ulong idx) {
            switch (act)
            {
            case commit_log:
                std::cout << "Committing log at index " << idx << '\n';
                break;
            default:
                break;
            }
        }

}