#ifndef _table
#define _table


#include <iostream>

using namespace std;

#if defined(GENERIC)
class Table {
  // Students should write this class

 public:
  ostream & Print(ostream &os) const;
};
#endif


#if defined(LINKSTATE)
#include <deque>
#include <map>
#include <set>

class Table {
  // Students should write this class
  public:
  // outside map will be all the node in the topology
  // inside map will be the node's neighbor
  // double will be the latency

  map<unsigned, map<unsigned,double> > table; 
  Table(){}
  ~Table(){}
   void UpdateTable(unsigned key, map<unsigned,double> &value){
    table[key] = value;
    cout << "updated table value: " << value.find(key)->second << endl;
    cout << "updated table key: " << key << endl;
  } 

  ostream & Print(ostream &os) const;
};
#endif

#if defined(DISTANCEVECTOR)

#include <deque>
#include <map>
#include <set>

class Table {
 
 public:
  // based off of the routing table from the Wikipedia
  // distance vector algorithm. The outer map's keys are
  // neighbors (distance via), and the inner map's keys
  // are the distances (distance to, via neighbor X)
  map<unsigned, map<unsigned,double> > table; 

  Table(){}
  ~Table(){} 
  void UpdateTable(unsigned key, map<unsigned,double> &value){
    table[key] = value;
    cout << "updated table value: " << value.find(key)->second << endl;
    cout << "updated table key: " << key << endl;
  } 

  set<unsigned> GetReachableNeighbors() {
    set<unsigned> neighbors;
    for (map<unsigned, map<unsigned,double> >::iterator it = table.begin(); it != table.end(); it++) {
      for (map<unsigned,double>::iterator innerit = it->second.begin(); innerit != it->second.end(); innerit++) {
        neighbors.insert(innerit->first);
      }
    }
    return neighbors;
  }

  double GetMinDistance(unsigned destId) {
    double minDistance = -1;
    for (map<unsigned, map<unsigned,double> >::iterator it = table.begin(); it != table.end(); it++) {
      if ((it->second.find(destId) != it->second.end() && it->second.find(destId)->second < minDistance) || minDistance < 0) {
        minDistance = it->second.find(destId)->second;
      }
    }
    return minDistance;
  }

  ostream & Print(ostream &os) const;
};
#endif

inline ostream & operator<<(ostream &os, const Table &t) { return t.Print(os);}

#endif
