#include "node.h"
#include "context.h"
#include "error.h"
#include <iterator> 
#include <algorithm>

Node::Node(const unsigned n, SimulationContext *c, double b, double l) : 
    number(n), context(c), bw(b), lat(l) 
{}

Node::Node() 
{ throw GeneralException(); }

Node::Node(const Node &rhs) : 
  number(rhs.number), context(rhs.context), bw(rhs.bw), lat(rhs.lat), table(rhs.table) {}

Node & Node::operator=(const Node &rhs) 
{
  return *(new(this)Node(rhs));
}

void Node::SetNumber(const unsigned n) 
{ number=n;}

unsigned Node::GetNumber() const 
{ return number;}

void Node::SetLatency(const double l)
{ lat=l;}

double Node::GetLatency() const 
{ return lat;}

void Node::SetBW(const double b)
{ bw=b;}

double Node::GetBW() const 
{ return bw;}

Node::~Node()
{}

// Implement these functions  to post an event to the event queue in the event simulator
// so that the corresponding node can recieve the ROUTING_MESSAGE_ARRIVAL event at the proper time
void Node::SendToNeighbors(const RoutingMessage *m)
{
  context->SendToNeighbors(this, m);
  //cout << "sent message to all neighbors " << endl;
}

void Node::SendToNeighbor(const Node *n, const RoutingMessage *m)
{
   context->SendToNeighbor(this, n, m);
   //cout << "posted event " << endl;
}

deque<Node*> *Node::GetNeighbors()
{
  return context->GetNeighbors(this);
}

void Node::SetTimeOut(const double timefromnow)
{
  context->TimeOut(this,timefromnow);
}


bool Node::Matches(const Node &rhs) const
{
  return number==rhs.number;
}

Node *Node::FindNeighbor(unsigned number) const{
  //cout << "finding neighbors" << endl;
  deque<Node*> *neighbors = context->GetNeighbors(this);

  //cout << "got neighbors of this node" << endl;
  for(deque<Node*>::iterator it = neighbors->begin(); it != neighbors->end(); it++){
    //cout << "checking neighbors" << endl;
    if((*it)->GetNumber() == number){
      //cout << "successfully found neighbor: " << number << endl;
      Node *copynode = new Node(*(*it));
      return copynode;
    }
  } 
  //cout << "did not find neighbor " << endl;
  return NULL;
}

#if defined(GENERIC)
void Node::LinkHasBeenUpdated(const Link *l)
{
  cerr << *this << " got a link update: "<<*l<<endl;
  //Do Something generic:
  SendToNeighbors(new RoutingMessage);
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  cerr << *this << " got a routing message: "<<*m<<" Ignored "<<endl;
}


void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored"<<endl;
}

Node *Node::GetNextHop(const Node *destination) const
{
  return 0;
}

Table *Node::GetRoutingTable() const
{
  return new Table;
}


ostream & Node::Print(ostream &os) const
{
  os << "Node(number="<<number<<", lat="<<lat<<", bw="<<bw<<")";
  return os;
}

#endif

#if defined(LINKSTATE)


void Node::LinkHasBeenUpdated(const Link *l)
{
  
  cerr << *this<<": Link Update: "<<*l<<endl;
  if (!table){
    table = new Table();
  }
  
  // update our own table
  table->table[l->GetSrc()][l->GetDest()] = l->GetLatency(); 
  seq_map[number]++;
  // flood neighbors
  RoutingMessage *message = new RoutingMessage(table->table[l->GetSrc()], number, seq_map[number]);
  SendToNeighbors(message);
  

  cerr << "Link Update Complete" <<endl; 
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  // cerr << *this << " Routing Message: "<<*m;
  
  if (m->seq_num > seq_map[m->node_id]){
    // cout << "seq_num: " << m->seq_num << endl;
    table->table[m->node_id] = m->neighbor_map;
    SendToNeighbors(m);
    seq_map[m->node_id] = m->seq_num;
  } 
   
}

void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored"<<endl;
}

Node *Node::GetNextHop(const Node *destination) const
{
  vector<unsigned> visited_nodes; 
  map<unsigned, pair<unsigned, double> > dist_map;
  unsigned curr_pos = number;
  // initialize dist_map 
  for( map<unsigned, map<unsigned, double> >::const_iterator it = table->table.begin(); it != table->table.end(); it++){
    dist_map[it->first] = make_pair(it->first, 0); 
  }  
  
  // cout << "initialized dist_map" << endl;
  
  while(find(visited_nodes.begin(), visited_nodes.end(), destination->number) == visited_nodes.end()){
    // update distances for one row
   //  cout << "destination node: " << destination->number << endl;
   //  cout << "updating distance for one row" << endl;
    for( map<unsigned, double>::const_iterator it = table->table[curr_pos].begin(); it != table->table[curr_pos].end(); it++){
      if(find(visited_nodes.begin(), visited_nodes.end(), it->first) == visited_nodes.end()){
        if(dist_map[it->first].second == 0 || dist_map[curr_pos].second + table->table[curr_pos][it->first] < dist_map[it->first].second){
           dist_map[it->first] = make_pair(curr_pos, dist_map[curr_pos].second + table->table[curr_pos][it->first]); 
        }
      }
    }
    
    // cout << "row distance updated" << endl; 
    // find min_distance of the row
    unsigned min_distance = 0;
    for(map<unsigned, pair<unsigned, double> >::iterator it = dist_map.begin(); it != dist_map.end(); it++){
      if(find(visited_nodes.begin(), visited_nodes.end(), it->first) == visited_nodes.end() && it->second.second > 0){
        if(min_distance == 0 || it->second.second < min_distance){
          min_distance = it->second.second;
          curr_pos = it->first; 
        }
      }
    }
    visited_nodes.push_back(curr_pos);
    // cout << *table << endl;
    // cout << "node: " << curr_pos << " pushed back" << endl; 
  }
   
  // traverse back to find the next node
  // cout << "traversing back " << endl;
  unsigned bt_pos = destination -> number; 
  while(1){
    if(dist_map[bt_pos].first== number){
      Node *next_hop = FindNeighbor(bt_pos);
      return next_hop;  
    }
    bt_pos = dist_map[bt_pos].first;  
  }
  
  return 0;
}

Table *Node::GetRoutingTable() const
{
  if(table){
    return table;
  }  
  return NULL;
}


ostream & Node::Print(ostream &os) const
{
  os << "Node(number="<<number<<", lat="<<lat<<", bw="<<bw<<")";
  return os;
}
#endif


#if defined(DISTANCEVECTOR)

void Node::LinkHasBeenUpdated(const Link *l)
{
  cout << "\nlink has been updated \n" << endl;
  if (!table) {
    //cout << "creating new table \n \n" << endl;
    table = new Table();
  }
  // update our table
  map<unsigned, map<unsigned, double> > node_table = table->table; 

  map<unsigned, double> dest_map; 
  if( node_table.find(l->GetDest()) != node_table.end()){
    //cout << "table has link entry" << endl;
    // table does have this entry
    dest_map = node_table[l->GetDest()];
  }
  //cout << "getting latency of link " << endl;
  dest_map[l->GetDest()] = l->GetLatency(); 
  table->UpdateTable(l->GetDest(), dest_map);
  //cout << "updated table with new latency value" << endl;
  
  // send out routing mesages
  RoutingMessage *message = new RoutingMessage(*this);
  //cout << "created routing message to send: " << message << endl;
  //cout << "node of message is: " << message->node << endl;
  //cout << "id of node of message is: " << message->node->GetNumber() << endl;
  //cout << "table of node of message is: " << message->node->GetRoutingTable() << endl;
  //cout << "this node's table is: " << this->table << endl;
  SendToNeighbors(message);
  //cout << "sent routing message to neighbors" << endl;

  cerr << *this<<": Link Update: "<<*l<<endl;
}

void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  cout << "\nprocessing incoming routing message" << endl;
  bool updated = false;
  // Check the id number of the incoming message
  if (!m) {
    return;
  }
  //cout << "message is " << m << endl;
  if (m->node) {
    //cout << "node is not null - node is " << m->node << endl;
  }
  // cout << "table before processing message: " << *table << endl;
  unsigned id = m->node->GetNumber();
  //unsigned id = m->node->GetNumber();
  //cout << "got node id of message: " << id << endl;
  //cout << "this node's id is: " << number << endl;

  // Discover all reachable neighbors in the table.
  Table *t = m->node->GetRoutingTable();
  //cout << "got table of message: " << t << endl;
  set<unsigned> neighbors = t->GetReachableNeighbors();
  // cout << "reachable neighbors: " << endl;
  for(set<unsigned>::iterator it = neighbors.begin(); it != neighbors.end(); it++) {
    // cout << *it << " ";
  }
  // cout << endl;

  // For each reachable neighbor, calculate the minimum distance 
  // to that neighbor, plus the distance of this node to the 
  // sender node. If this distance is less than the current distance
  // from this node to the neighbor or the distance
  // is undefined, update the value to the calculated distance.
  for (set<unsigned>::iterator it = neighbors.begin(); it != neighbors.end(); it++) {
    if ((*it) != this->number) {
      //cout << "calculating min distance " << endl;
      double minDistance = t->GetMinDistance(*it) + table->table[id][id];
      // cout << "min distance is " << minDistance << endl;
      //if (minDistance < 0) {
        //return;
      //}
      //cout << "searching our own distances to check if a shorter path is available" << endl;
      if (table->table[id].find(*it) == table->table[id].end() || minDistance != table->table[id][*it]) {
        table->table[id][*it] = minDistance;
        updated = true;
        //cout << "updated table entry due to shorter path " << endl;
      }
    }
  }
  if (updated) {
    // post the updated table
    RoutingMessage *message = new RoutingMessage(*this);
    //cout << "posting new message of our updated table to everyone " << endl;
    SendToNeighbors(message);
    //cout << "table after processing routing message: " << *table << endl;
    //cout << "sent new routing table to neighbors" << endl;
  }
}

void Node::TimeOut() 
{
  cerr << *this << " got a timeout: ignored"<<endl;
}


Node *Node::GetNextHop(const Node *destination) const
{
  //cout << "getting next hop " << endl;
  map<unsigned, double> curr_map;
  // cout << "source is: " << this->number << " destination is: " << destination->number << endl;
  // cout << "table is: " << *table << endl;
  double min = -1;
  unsigned min_number = 0;
  Node *returned_node;  
  for (map<unsigned, map<unsigned, double> >::iterator it = table->table.begin(); it != table->table.end(); ++it){
    curr_map = it->second;
    if(min == -1 || curr_map[destination->number] < min){
      min = curr_map[destination->number]; 
      min_number = it->first;
    } 
  }
  returned_node = FindNeighbor(min_number);
  //cout << "returning found neighbor " << endl;
  return returned_node; 
}


Table *Node::GetRoutingTable() const
{
  if(table){
    return table;
  } 
  //cout<<"error in GetRoutingTable()" <<endl;
  return NULL;
}

ostream & Node::Print(ostream &os) const
{
  os << "Node(number="<<number<<", lat="<<lat<<", bw="<<bw;
  return os;
}
#endif
