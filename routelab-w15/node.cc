#include "node.h"
#include "context.h"
#include "error.h"


Node::Node(const unsigned n, SimulationContext *c, double b, double l) : 
    number(n), context(c), bw(b), lat(l) 
{}

Node::Node() 
{ throw GeneralException(); }

Node::Node(const Node &rhs) : 
  number(rhs.number), context(rhs.context), bw(rhs.bw), lat(rhs.lat) {}

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
  deque<Node*> *neighbors = GetNeighbors();
  for (deque<Node*>::iterator it = neighbors->begin(); it != neighbors->end(); it++) {
    SendToNeighbor(*it, m);
  }
}

void Node::SendToNeighbor(const Node *n, const RoutingMessage *m)
{
   Link *l = context->FindMatchingLink(&Link(number, n->number, 0, 0, 0));
   double setTime = context->GetTime() + l->GetLatency();
   EventType type = ROUTING_MESSAGE_ARRIVAL;
   Node *handler_node = FindNeighbor(l->GetDest());
   RoutingMessage message = RoutingMessage(*m);
   Event e = Event(setTime, type, handler_node, &message);
   context -> PostEvent(&e);   
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


#if defined(GENERIC)
void Node::LinkHasBeenUpdated(const Link *l)
{
  cerr << *this << " got a link update: "<<*l<<endl;
  //Do Something generic:
  SendToNeighbors(new RoutingMessage);
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  cerr << *this << " got a routing messagee: "<<*m<<" Ignored "<<endl;
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
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  cerr << *this << " Routing Message: "<<*m;
}

void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored"<<endl;
}

Node *Node::GetNextHop(const Node *destination) const
{
  // WRITE
  return 0;
}

Table *Node::GetRoutingTable() const
{
  // WRITE
  return 0;
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
  // update our table
  map<unsigned, map<unsigned, double> > node_table = table -> table; 

  map<unsigned, double> dest_map; 
  if( node_table.find(l->GetDest()) != node_table.end()){
    // table does have this entry
    dest_map = node_table[l->GetDest()];
  }
  dest_map[l->GetDest()] = l->GetLatency(); 
  table->UpdateTable(l->GetDest(), dest_map);
  
  // send out routing mesages
  RoutingMessage message = RoutingMessage(this);
  SendToNeighbors(&message);

  cerr << *this<<": Link Update: "<<*l<<endl;
}

void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{
  bool updated = false;
  // Check the id number of the incoming message
  unsigned id = m->node->GetNumber();

  // Discover all reachable neighbors in the table.
  Table *t = m->node->GetRoutingTable();
  set<unsigned> neighbors = t->GetReachableNeighbors();

  // For each reachable neighbor, calculate the minimum distance 
  // to that neighbor, plus the distance of this node to the 
  // sender node. If this distance is less than the current distance
  // from this node to the neighbor or the distance
  // is undefined, update the value to the calculated distance.
  for (set<unsigned>::iterator it = neighbors.begin(); it != neighbors.end(); it++) {
    double minDistance = t->GetMinDistance(*it) + table->table[id][id];
    if (minDistance < 0) {
      return;
    }
    if (table->table[id].find(*it) == table->table[id].end() || minDistance < table->table[id][*it]) {
      table->table[id][*it] = minDistance;
      updated = true;
    }
  }
  if (updated) {
    // post the updated table
    RoutingMessage message = RoutingMessage(this);
    SendToNeighbors(&message);
  }
}

void Node::TimeOut() 
{
  cerr << *this << " got a timeout: ignored"<<endl;
}


Node *Node::GetNextHop(const Node *destination) const
{
  map<unsigned, double> curr_map;
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
  return returned_node; 
}

Node *Node::FindNeighbor(unsigned number) const{
  deque<Node*> *neighbors = context->GetNeighbors(this);
  Node* curr_neighbors; 

  for(unsigned int i = 0; i < neighbors->size(); i++){
    curr_neighbors = neighbors->front();
    neighbors->pop_front();
    if(curr_neighbors->number == number){
      return curr_neighbors;
    }
  } 
  return NULL;
}

Table *Node::GetRoutingTable() const
{
  if(table){
    return table;
  } 
  cout<<"error in GetRoutingTable()" <<endl;
  return NULL;
}

ostream & Node::Print(ostream &os) const
{
  os << "Node(number="<<number<<", lat="<<lat<<", bw="<<bw;
  return os;
}
#endif
