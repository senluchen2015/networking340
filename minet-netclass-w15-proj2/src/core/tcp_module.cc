#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include <iostream>
#include "tcpstate.h"

#include "Minet.h"


using std::cout;
using std::endl;
using std::cerr;
using std::string;

void sendSynAck(Connection &, MinetHandle, unsigned int, unsigned int, unsigned int);
//template<class STATE>void addSynAckMapping(Connection &, unsigned int, unsigned int, unsigned int, ConnectionList<STATE> &);

int main(int argc, char *argv[])
{
  MinetHandle mux, sock;

  MinetInit(MINET_TCP_MODULE);

  mux=MinetIsModuleInConfig(MINET_IP_MUX) ? MinetConnect(MINET_IP_MUX) : MINET_NOHANDLE;
  sock=MinetIsModuleInConfig(MINET_SOCK_MODULE) ? MinetAccept(MINET_SOCK_MODULE) : MINET_NOHANDLE;

  if (MinetIsModuleInConfig(MINET_IP_MUX) && mux==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't connect to mux"));
    return -1;
  }

  if (MinetIsModuleInConfig(MINET_SOCK_MODULE) && sock==MINET_NOHANDLE) {
    MinetSendToMonitor(MinetMonitoringEvent("Can't accept from sock module"));
    return -1;
  }

  MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling TCP traffic"));

  MinetEvent event;

  ConnectionList<TCPState> clist;

  while (MinetGetNextEvent(event, 5.0)==0) {
    // if we received an unexpected type of event, print error
    cerr << event << endl;
    if (event.eventtype == MinetEvent::Timeout) {
      cerr << "There was a timeout" << endl;
    }
    if (event.eventtype!=MinetEvent::Dataflow 
        || event.direction!=MinetEvent::IN) {
      MinetSendToMonitor(MinetMonitoringEvent("Unknown event ignored."));
      // if we received a valid event from Minet, do processing
    } else {
      //  Data from the IP layer below  //
      if (event.handle==mux) {
        cerr << " \n started block from mux \n " << endl;
        Packet p;
        MinetReceive(mux,p);
        unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
        cerr << "estimated header len="<<tcphlen<<"\n" << endl;
        p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
        IPHeader iph=p.FindHeader(Headers::IPHeader);
        TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

        cerr << "received packet " << p << endl;

        Connection c;
        // note that this is flipped around because
        // "source" is interepreted as "this machine"
        iph.GetDestIP(c.src);
        iph.GetSourceIP(c.dest);
        iph.GetProtocol(c.protocol);
        tcph.GetDestPort(c.srcport);
        tcph.GetSourcePort(c.destport);

        ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);

        cerr << "TCP Packet: IP Header is "<<iph<<" and ";
        cerr << "TCP Header is "<<tcph << " and ";

        cerr << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID");

        if (cs!=clist.end()) {
          cerr << "connection TCPState state is " << (*cs).state.GetState() << endl;
          if ((*cs).state.GetState() == LISTEN) {
            Buffer &buf = p.GetPayload();
            unsigned char len = 0;
            unsigned char flags = 0;
            unsigned int seq_number = 0;
            size_t buflen = buf.GetSize();
            tcph.GetHeaderLen(len);
            tcph.GetFlags(flags);
            tcph.GetSeqNum(seq_number);
            cerr << "new seq_number is " << seq_number + 1 << endl;
            if (IS_SYN(flags)) {
              cerr << "It's a SYN! Sending SYN ACK" << endl;
              sendSynAck(c, mux, seq_number, 300, 14600);
              //addSynAckMapping(c, seq_number, 300, 14600, clist);
              ConnectionToStateMapping<TCPState> m;
              m.connection=c;
              m.state.SetState(SYN_RCVD);
              m.state.SetLastAcked(seq_number + 1);
              m.state.SetLastSent(300);
              m.state.SetSendRwnd(14600);
              // expire a connection after sending only one SYNACK
              m.state.SetTimerTries(0);
              Time currentTime = Time();
              Time fiveSeconds = Time(5.0);
              Time timeout;
              timeradd(&currentTime, &fiveSeconds, &timeout);
              m.timeout = fiveSeconds;
              m.bTmrActive = true;
              clist.push_back(m);
              cerr << "added new connection to state mapping" << endl;
            }
          }
        }
        Buffer &buf = p.GetPayload();
        unsigned char len = 0;
        unsigned char flags = 0;
        unsigned int seq_number = 0;
        size_t buflen = buf.GetSize();
        tcph.GetHeaderLen(len);
        tcph.GetFlags(flags);

        char sample[buflen+1];
        buf.GetData(sample, buflen, 0);
        sample[buflen] = '\0';
        cerr << " buffer length is " << buf.GetSize();
        cerr << " tcp header len is " << len;
        cerr << " printing data " << sample;

      }
      //  Data from the Sockets layer above  //
      if (event.handle==sock) {
        SockRequestResponse req;
        MinetReceive(sock,req);
        cerr << "req is " << req << endl;
        if (req.type == ACCEPT) {
          cerr << "Socket request type is ACCEPT \n" << endl;
          ConnectionToStateMapping<TCPState> m;
          m.connection=req.connection;
          m.state.SetState(LISTEN);
          // remove any old accept that might be there.
          ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
          if (cs!=clist.end()) {
            clist.erase(cs);
          }
          clist.push_back(m);
          cerr << "added accept socket to list of connections \n" << endl;
        }
        cerr << "Received Socket Request:" << req << endl;
      }
    }
  }
  return 0;
}
/*
void addSynAckMapping(Connection &c, unsigned int req_seq_number, unsigned int res_seq_number, unsigned short win_size, ConnectionList<STATE> &clist) {
  ConnectionToStateMapping<TCPState> m;
  m.connection=c;
  m.state.SetState(SYN_RCVD);
  m.state.SetLastAcked(req_seq_number + 1);
  m.state.SetLastSent(res_seq_number);
  m.state.SetSendRwnd(win_size);
  // expire a connection after sending only one SYNACK
  m.state.SetTimerTries(0);
  Time currentTime = Time();
  Time fiveSeconds = Time(5.0);
  Time timeout;
  timeradd(&currentTime, &fiveSeconds, &timeout);
  m.timeout = timeout;
  m.bTmrActive = true;
  clist.push_back(m);
}
*/
void sendSynAck(Connection &c, MinetHandle mux, unsigned int req_seq_number, unsigned int res_seq_number, unsigned short win_size) {
  unsigned char flags = 0;
  Packet synackpack = Packet();
  IPHeader resiph = IPHeader();
  resiph.SetDestIP(c.dest);
  resiph.SetSourceIP(c.src);
  resiph.SetProtocol(c.protocol);
  resiph.SetTotalLength(IP_HEADER_BASE_LENGTH + 20);
  cerr << "set IP headers " << endl;
  synackpack.PushFrontHeader(resiph);
  cerr << "added IP header to packet" << endl;
  cerr << "src IP: " << c.src << endl;
  cerr << "dest IP: " << c.dest << endl;
  //char buf[len];
  TCPHeader restcph = TCPHeader();
  cerr << "initialized tcp header" << endl;
  restcph.SetSourcePort(c.srcport, synackpack);
  restcph.SetDestPort(c.destport, synackpack);
  cerr << "set TCP ports: destport " << endl;
  cerr << "src port: " << c.srcport << endl;
  cerr << "dest port: " << c.destport << endl;
  restcph.SetSeqNum(res_seq_number, synackpack);
  cerr << "set TCP seqnum" << endl;
  restcph.SetAckNum(req_seq_number + 1, synackpack);
  restcph.SetWinSize(win_size, synackpack);
  cerr << "set TCP headers " << endl;
  SET_SYN(flags);
  SET_ACK(flags);
  restcph.SetFlags(flags, synackpack);
  unsigned char hardcode_len = 5;
  restcph.SetHeaderLen(hardcode_len, synackpack);
  synackpack.PushBackHeader(restcph);
  cerr << "set TCP flags " << endl;
  cerr << "added TCP header to packet" << endl;
  cerr << "Response TCP Packet: IP Header is " << resiph <<" and " << endl;
  cerr << "Response TCP header is " << restcph <<" and " << endl;
  int result = MinetSend(mux, synackpack);
  int secondResult = MinetSend(mux, synackpack);
  for (int i = 0; i < 10; i++) {
    MinetSend(mux, synackpack);
  }
  cerr << "sent packet " << synackpack << endl;
  if (result < 0) {
    cerr << "Minet Send resulted in error " << endl;
  } else {
    cerr << "Minet Send successful " << endl;
  }
  if (secondResult < 0) {
    cerr << "Minet second Send resulted in error " << endl;
  } else {
    cerr << "Minet second Send successful " << endl;
  }

}
