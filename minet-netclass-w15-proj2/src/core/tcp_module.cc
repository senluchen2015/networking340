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

void sendSynAck(Connection, MinetHandle, unsigned int, unsigned int, unsigned short);
void addSynAckMapping(Connection &c, unsigned int req_seq_number, unsigned int res_seq_number, unsigned short win_size, ConnectionList<TCPState> &clist);
void checkForTimedOutConnection(ConnectionList<TCPState> &clist, MinetHandle mux);
void handleAck(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen, TCPHeader tcph, MinetHandle mux);
Time timeFromNow(double seconds);
void addActiveOpenConnection(ConnectionList<TCPState> &clist, Connection &c, unsigned int seq_number);
void activeOpen(MinetHandle mux, Connection &c, unsigned int seq_number);
void sendAckPack(Connection &c, MinetHandle mux, unsigned int ack_number, unsigned int seq_number, unsigned short win_size);
void updateConnectionStateMapping(ConnectionList<TCPState> &clist, Connection &c, unsigned int req_seq_number, unsigned int req_ack_number, unsigned int res_seq_number, unsigned int res_ack_number, size_t datalen, unsigned short rwnd, unsigned int newstate);
void receiveData(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen);
IPHeader setIPHeaders(Connection &c);
 
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
      //cerr << "There was a timeout" << endl;
      checkForTimedOutConnection(clist, mux);
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

        Buffer &buf = p.GetPayload();
        unsigned char len = 0;
        unsigned char flags = 0;
        unsigned int seq_number = 0;
        size_t buflen = buf.GetSize();
        tcph.GetHeaderLen(len);
        tcph.GetFlags(flags);
        tcph.GetSeqNum(seq_number);

        if (cs!=clist.end()) {
          cerr << "connection TCPState state is " << (*cs).state.GetState() << endl;
          if ((*cs).state.GetState() == LISTEN) {
            cerr << "new seq_number is " << seq_number + 1 << endl;
            if (IS_SYN(flags)) {
              cerr << "It's a SYN! Sending SYN ACK" << endl;
              sendSynAck(c, mux, seq_number, 300, 14600);
              addSynAckMapping(c, seq_number, 300, 14600, clist);
              cerr << "added new connection to state mapping" << endl;
            }
          }
        }
        if (IS_ACK(flags)) {
          cerr << "received an ACK, updating states " << endl;
          handleAck(clist, c, buf, buflen, tcph, mux);
        }

        
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
        if (req.type == CONNECT) {
          cerr << "Socket request type is CONNECT \n" << endl;
          // TODO: remove hardcoded seq number
          unsigned int hardcoded_seq_number = 500;
          activeOpen(mux, req.connection, hardcoded_seq_number);
          addActiveOpenConnection(clist, req.connection, hardcoded_seq_number);
        }
      }
    }
  }
  return 0;
}

Time timeFromNow(double seconds) {
  Time currentTime = Time();
  Time nSeconds = Time(seconds);
  Time timeout;
  timeradd(&currentTime, &nSeconds, &timeout);
  return timeout;
}

void addActiveOpenConnection(ConnectionList<TCPState> &clist, Connection &c, unsigned int seq_number) {
  ConnectionToStateMapping<TCPState> m;
  m.connection = c;
  m.state.SetState(SYN_SENT);
  m.state.SetLastSent(seq_number);
  // TODO: change hardcoded timer tries value
  m.state.SetTimerTries(3);
  m.bTmrActive = true;
  m.timeout = timeFromNow(5.0);
  clist.push_front(m);
}

void activeOpen(MinetHandle mux, Connection &c, unsigned int seq_number) {
  unsigned char flags = 0;
  Packet activeOpenPacket = Packet();
  IPHeader activeOpenIph = IPHeader();
  activeOpenIph.SetDestIP(c.dest);
  activeOpenIph.SetSourceIP(c.src);
  activeOpenIph.SetProtocol(c.protocol);
  activeOpenIph.SetTotalLength(IP_HEADER_BASE_LENGTH + 20);
  cerr << "set IP headers " << endl;
  activeOpenPacket.PushFrontHeader(activeOpenIph);
  cerr << "added IP header to packet" << endl;
  cerr << "src IP: " << c.src << endl;
  cerr << "dest IP: " << c.dest << endl;
  //char buf[len];
  TCPHeader activeOpenTcph = TCPHeader();
  cerr << "initialized tcp header" << endl;
  activeOpenTcph.SetSourcePort(c.srcport, activeOpenPacket);
  activeOpenTcph.SetDestPort(c.destport, activeOpenPacket);
  cerr << "set TCP ports: destport " << endl;
  cerr << "src port: " << c.srcport << endl;
  cerr << "dest port: " << c.destport << endl;
  activeOpenTcph.SetSeqNum(seq_number, activeOpenPacket);
  cerr << "set TCP seqnum" << seq_number << " + 1" << endl;
  activeOpenTcph.SetAckNum(0, activeOpenPacket);
  // TODO: remove hardcoded win size
  activeOpenTcph.SetWinSize(10000, activeOpenPacket);
  cerr << "set TCP headers " << endl;
  SET_SYN(flags);
  activeOpenTcph.SetFlags(flags, activeOpenPacket);
  // TODO: change hardcoded length to reflect whether or not there are options
  unsigned char hardcode_len = 5;
  activeOpenTcph.SetHeaderLen(hardcode_len, activeOpenPacket);
  activeOpenPacket.PushBackHeader(activeOpenTcph);
  cerr << "set TCP flags " << endl;
  cerr << "added TCP header to packet" << endl;
  cerr << "Response TCP Packet: IP Header is " << activeOpenIph <<" and " << endl;
  cerr << "Response TCP header is " << activeOpenTcph <<" and " << endl;
  int result = MinetSend(mux, activeOpenPacket);
  int secondResult = MinetSend(mux, activeOpenPacket);
}

void handleAck(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen, TCPHeader tcph, MinetHandle mux) {
  Time currentTime = Time();
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  unsigned int req_seq_number = 0;
  unsigned int req_ack_number = 0;
  tcph.GetSeqNum(req_seq_number);
  tcph.GetAckNum(req_ack_number);
  if (cs != clist.end()) {
    ConnectionToStateMapping<TCPState> mapping = *cs;
    // there is a connection for which to handle ACK
    switch(mapping.state.stateOfcnx) {
      case SYN_RCVD:
        // change state to established and deactivate timer
        if (req_ack_number == mapping.state.last_sent + 1) {
          mapping.state.stateOfcnx = ESTABLISHED;
          mapping.state.SetLastAcked(req_ack_number);
          mapping.bTmrActive = false;
          clist.erase(cs);
          clist.push_front(mapping);
        }
        break;
      case ESTABLISHED:
        // check if the segment contains data
        if (buflen > 0) {
          // check if the segment # is equal to last received
          if (req_seq_number == mapping.state.GetLastRecvd()) {
            // segment received is directly after last one; send an ACK
            // of seq_number + data length; make sure ack_number is mod 2^32
            unsigned int res_ack_number = (req_seq_number + buflen) & 0xffffffff;
            // check that the ack is equal to our last sent (last packet's
            // seq_number + last packet's size)
            if (req_ack_number == mapping.state.GetLastSent()) {
              // seq_number is simply our last sent
              unsigned int res_seq_number = mapping.state.GetLastSent();
              // window size is equal to receive buffer's size
              sendAckPack(c, mux, res_ack_number, res_seq_number, mapping.state.RecvBuffer.GetSize() - 1);
              // TODO: adjust for sending data - right now datalen is 1 due to 
              // not sending data
              updateConnectionStateMapping(clist, c, req_seq_number, req_ack_number, res_seq_number, res_ack_number, 1, mapping.state.GetRwnd(), ESTABLISHED);
              receiveData(clist, c, buf, buflen);
            }
          }
        }
        break;
      default:
        break;
    }
  }
}

IPHeader setIPHeaders(Connection &c) {
  IPHeader resiph = IPHeader();
  resiph.SetDestIP(c.dest);
  resiph.SetSourceIP(c.src);
  resiph.SetProtocol(c.protocol);
  resiph.SetTotalLength(IP_HEADER_BASE_LENGTH + 20);
  return resiph;
}

void receiveData(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen) {
  // right now, just replace the RecvBuffer contents of the TCPState
  // with the new buffer and print it out
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  if (cs != clist.end()) {
    ConnectionToStateMapping<TCPState> mapping = *cs;
    mapping.state.RecvBuffer.Clear();
    mapping.state.RecvBuffer.AddFront(buf);
    clist.erase(cs);
    clist.push_front(mapping);
  }
}

void updateConnectionStateMapping(ConnectionList<TCPState> &clist, Connection &c, unsigned int req_seq_number, unsigned int req_ack_number, unsigned int res_seq_number, unsigned int res_ack_number, size_t datalen, unsigned short rwnd, unsigned int newstate) {
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  if (cs != clist.end()) {
    ConnectionToStateMapping<TCPState> mapping = *cs;
    mapping.state.SetState(newstate);
    mapping.state.SetLastRecvd(res_ack_number);
    mapping.state.SetLastSent(res_seq_number + datalen);
    mapping.state.SetLastAcked(req_ack_number);
    mapping.state.SetSendRwnd(rwnd);
    clist.erase(cs);
    clist.push_front(mapping);
  }
}

void sendAckPack(Connection &c, MinetHandle mux, unsigned int ack_number, unsigned int seq_number, unsigned short win_size) {
  Packet ackpack = Packet();
  IPHeader resiph = setIPHeaders(c);
  ackpack.PushFrontHeader(resiph);
  TCPHeader restcph = TCPHeader();
  restcph.SetSourcePort(c.srcport, ackpack);
  restcph.SetDestPort(c.destport, ackpack);
  restcph.SetSeqNum(seq_number, ackpack);
  restcph.SetAckNum(ack_number, ackpack);
  restcph.SetWinSize(win_size, ackpack);
  unsigned char flags = 0;
  SET_ACK(flags);
  restcph.SetFlags(flags, ackpack);
  unsigned char hardcode_len = 5;
  restcph.SetHeaderLen(hardcode_len, ackpack);
  ackpack.PushBackHeader(restcph);
  cerr << "Response TCP header is " << restcph <<" and " << endl;
  int result = MinetSend(mux, ackpack);
  int secondResult = MinetSend(mux, ackpack);
}

void checkForTimedOutConnection(ConnectionList<TCPState> &clist, MinetHandle mux) {
  Time currentTime = Time();
  ConnectionList<TCPState>::iterator i = clist.FindEarliest();
  if (clist.empty()) {
    cerr << "clist is empty " << endl;
  }
  //cerr << "size of clist is " << clist.size();
  if (i != clist.end()) {
    //cerr << "found earliest time" << endl;
    ConnectionToStateMapping<TCPState> mapping = *i;
    if (mapping.timeout <= currentTime) {
      cerr << "handling a timed out connection " << endl;
      // handle timed out connection
      if (mapping.state.tmrTries == 0) {
        cerr << "deleting timed out connection " << endl;
        // delete this connection from the list
        clist.erase(i);
      } else {
        mapping.state.tmrTries--;
        Time fiveSeconds = Time(5.0);
        Time timeout;
        timeradd(&currentTime, &fiveSeconds, &timeout);
        switch(mapping.state.stateOfcnx) {
          case SYN_RCVD:
            // server trying to resend SYNACK
            clist.erase(i);
            mapping.timeout = timeout;
            clist.push_front(mapping);
            sendSynAck(mapping.connection, mux, mapping.state.last_recvd, mapping.state.last_sent, mapping.state.rwnd);
            cerr << "resending synack " << endl;
            break;
          default:
            break;
        }
      }
      checkForTimedOutConnection(clist, mux);
    }
  }
}

void addSynAckMapping(Connection &c, unsigned int req_seq_number, unsigned int res_seq_number, unsigned short win_size, ConnectionList<TCPState> &clist) {
  ConnectionToStateMapping<TCPState> m;
  m.connection=c;
  m.state.SetState(SYN_RCVD);
  m.state.SetLastRecvd(req_seq_number);
  // set this to the next ACK number we expect
  m.state.SetLastSent(res_seq_number + 1);
  // TODO: figure out if rwnd is OUR receive window or THEIRS
  // should be THEIRS based on source code
  m.state.SetSendRwnd(win_size);
  // expire a connection after sending only one SYNACK
  m.state.SetTimerTries(3);
  Time currentTime = Time();
  Time fiveSeconds = Time(5.0);
  Time timeout;
  timeradd(&currentTime, &fiveSeconds, &timeout);
  cerr << "new timeout time is " << timeout << endl;
  m.timeout = timeout;
  m.bTmrActive = true;
  clist.push_front(m);
}

void sendSynAck(Connection c, MinetHandle mux, unsigned int req_seq_number, unsigned int res_seq_number, unsigned short win_size) {
  unsigned char flags = 0;
  Packet synackpack = Packet();
  IPHeader resiph = setIPHeaders(c);
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
  cerr << "set TCP seqnum" << req_seq_number << " + 1" << endl;
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
  /*for (int i = 0; i < 10; i++) {
    MinetSend(mux, synackpack);
  }*/
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
