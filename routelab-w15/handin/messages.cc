#include "messages.h"


#if defined(GENERIC)
ostream &RoutingMessage::Print(ostream &os) const
{
  os << "RoutingMessage()";
  return os;
}
#endif


#if defined(LINKSTATE)

ostream &RoutingMessage::Print(ostream &os) const
{
  return os;
}

RoutingMessage::RoutingMessage(map<unsigned, double>n_map, unsigned id, unsigned s_num )
{
  neighbor_map = n_map;
  node_id = id;
  seq_num = s_num;
}


RoutingMessage::RoutingMessage(const RoutingMessage &rhs)
{}

#endif


#if defined(DISTANCEVECTOR)

ostream &RoutingMessage::Print(ostream &os) const
{
  return os;
}

RoutingMessage::RoutingMessage(const Node &n)
{
  node = new Node(n);
}


RoutingMessage::RoutingMessage(const RoutingMessage &rhs)
{}

#endif

