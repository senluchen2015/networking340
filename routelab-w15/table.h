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
class Table {
  // Students should write this class
 public:
  ostream & Print(ostream &os) const;
};
#endif

#if defined(DISTANCEVECTOR)

#include <deque>
#include <unordered_map>

class Table {
 
 public:
  unordered_map<unsigned, unordered_map<unsigned,double>> table; 
  Table(){};
  ~Table(){}; 
  void UpdateTable(unsigned key, unordered_map<unsigned,double>> value){
    table[key] = value;
  } 
  ostream & Print(ostream &os) const;
};
#endif

inline ostream & operator<<(ostream &os, const Table &t) { return t.Print(os);}

#endif
