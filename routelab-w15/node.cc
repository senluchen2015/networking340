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
}

void Node::SendToNeighbor(const Node *n, const RoutingMessage *m)
{

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
void Node::PostEvent(Link *l){
 
   double setTime = context->GetTime() + l->lat;
   EventType type = ROUTING_MESSAGE_ARRIVAL;
   Node *handler_node = FindNeighbor(l->dest);
   RoutingMessage *message = RoutingMessage(this);
   Event *e = Event(setTime, type, handler_node, mesages);
   context -> PostEvent(e);  
}

void Node::LinkHasBeenUpdated(const Link *l)
{
  // update our table
  unordered_map<unsigned, double> dest_map = table[l->dest];
  dest_map[l->dest] = l->lat; 
  table.UpdateTable(l->dest, dest_map);
  
  // send out routing mesages
  deque<Links*> out_going_links = context -> GetOutgoingLinks(this);
  for (int i = 0; i < out_going_links.size(); i++){
    PostEvent(out_going_links[i]);
  }  

  cerr << *this<<": Link Update: "<<*l<<endl;
}


void Node::ProcessIncomingRoutingMessage(const RoutingMessage *m)
{

}

void Node::TimeOut()
{
  cerr << *this << " got a timeout: ignored"<<endl;
}


Node *Node::GetNextHop(const Node *destination) const
{
  unordered_map<unsigned, double> curr_map;
  double min = -1;
  int min_number = 0;
  Node *returned_node;  
  for (auto it = table.begin(); it != table.end(); ++it){
    curr_map = it->second;
    if(min == -1 || curr_map[destination->number] < min){
      min = curr_map[destination->number]; 
      min_number = it->first;
    } 
  }
  returned_node = FindNeighbor(min_number);
  return returned_node; 
}

Node *Node::FindNeighbor(unsigned number){
  deque<Node*> neighbors = GetNeighbors();
  Node* curr_neighbors; 

  for(int i = 0; i < neighbors.size(); i++){
    curr_neighbors = neighbors[i];
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
  else{
    cout<<"error in GetRoutingTable()" <<endl;
  }
}

ostream & Node::Print(ostream &os) const
{
  os << "Node(number="<<number<<", lat="<<lat<<", bw="<<bw;
  return os;
}
#endif
