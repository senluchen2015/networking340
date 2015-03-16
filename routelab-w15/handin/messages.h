#ifndef _messages
#define _messages

#include <iostream>

#include "node.h"
#include "link.h"

#if defined(GENERIC)
struct RoutingMessage {
 public:
  ostream & Print(ostream &os) const;
};
#endif

#if defined(LINKSTATE)
struct RoutingMessage {
  map<unsigned, double> neighbor_map;
  unsigned node_id;
  unsigned seq_num;

  RoutingMessage();
  RoutingMessage(const RoutingMessage &rhs);
  RoutingMessage &operator=(const RoutingMessage &rhs);

  RoutingMessage(map<unsigned, double>n_map, unsigned id, unsigned s_num);
  ostream & Print(ostream &os) const;
};
#endif

#if defined(DISTANCEVECTOR)
struct RoutingMessage {
  Node *node; 
  RoutingMessage(const Node &node);
  RoutingMessage(const RoutingMessage &rhs);
  RoutingMessage &operator=(const RoutingMessage &rhs);

  ostream & Print(ostream &os) const;
};
#endif


inline ostream & operator<<(ostream &os, const RoutingMessage &m) { return m.Print(os);}

#endif
