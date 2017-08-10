#ifndef CAVS_MIDEND_STREAM_SCHEDULER_H_
#define CAVS_MIDEND_STREAM_SCHEDULER_H_

#include "cavs/midend/node.h"
#include "cavs/midend/edge.h"
#include "cavs/util/stream_event_handle_pool.h"

#include <unordered_map>
#include <list>
#include <vector>

namespace midend {

class StreamScheduler {
 public:
  StreamScheduler(std::list<Node*>* nodes, std::vector<Statement*>* stmts) {
    CHECK(nodes->size() == stmts->size());
    std::unordered_map<Node*, int> node2idx;
    auto iter = nodes->begin();
    for (int i = 0; i < nodes->size(); i++, iter++) {
      CHECK(node2idx.find(*iter) == node2idx.end());
      node2idx[*iter] = i;
      VLOG(V_DEBUG) << i << (*iter)->debug_info();
    }
    std::vector<int> stream_ids(nodes->size(), -1);
    std::vector<int> event_ids(nodes->size(), -1);
    std::vector<std::vector<int>> input_event_ids(nodes->size());
    std::vector<bool> sync_me(nodes->size(), false);
    //0) set the stream of me //initialization
    //1) set the event of me 
    //2) set the stream of father
    //3) set the input event of father
    iter = nodes->begin();
    for (int id = 0; id < nodes->size(); id++, iter++) {
      if ((*iter)->output_size() == 1 && (*iter)->output(0)->dst_size(true) == 1) {
        Edge* edge = (*iter)->output(0);
        //initialization, source node in this scope
        if (stream_ids[id] == -1) {
          stream_ids[id] = StreamEventHandlePool::GenNewStreamID();
        }
        CHECK(node2idx.find(edge->dst(0, true)) != node2idx.end()) << id
            << edge->dst(0, true)->debug_info();
        CHECK(stream_ids[node2idx.at(edge->dst(0, true))] == -1) 
            << id << "\t" << node2idx[edge->dst(0, true)]
            << (*iter)->debug_info() << "\n" << edge->dst(0, true)->debug_info();
        stream_ids[node2idx[edge->dst(0, true)]] = stream_ids[id];
      }else {
        if (stream_ids[id] == -1) {
          stream_ids[id] = StreamEventHandlePool::GenNewStreamID();
        }
        CHECK(event_ids[id] == -1);
        event_ids[id] = StreamEventHandlePool::GenNewEventID();
        
        bool reuse_stream = false;
        for (Edge* edge : (*iter)->output()) {
          for (Node* parent_node : edge->dst(true)) {
            int pid = node2idx[parent_node];
            if (stream_ids[pid] == -1 && !reuse_stream) {
              reuse_stream = true; 
              stream_ids[pid] = stream_ids[id];
            }else {
              CHECK(event_ids[id] != -1);
              input_event_ids[pid].push_back(event_ids[id]); 
            }
          }
          if (edge->dst_size(true) == 0) {
            sync_me[id] = true;
          }
        }
      }
    }
    VLOG(V_DEBUG) << "Streamming info " << stream_ids[0];

    for (int id = 0; id < stmts->size(); id++) {
      CHECK(stream_ids[id] != -1);
      if (stmts->at(id)->type() == Statement::EXPR) {
        ExprStatement* es = dynamic_cast<ExprStatement*>(stmts->at(id));
        es->GetContext()->SetStreamId(stream_ids[id]);
        for (int input_eid : input_event_ids[id]) {
          es->GetContext()->AddInputEventId(input_eid);
        }
        if (sync_me[id])
          es->GetContext()->SetSyncMe();
      }
    }

    iter = nodes->begin();
    for (int id = 0; id < nodes->size(); id++, iter++) {
      VLOG(V_DEBUG) << "Streamming info " << stream_ids[id];
      VLOG(V_DEBUG) << (*iter)->debug_info();
    }
  }
};

} //namespace midend 

#endif
