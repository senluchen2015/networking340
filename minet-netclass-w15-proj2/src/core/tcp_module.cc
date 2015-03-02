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
void handleAck(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen, TCPHeader tcph, MinetHandle mux, MinetHandle sock);

Time timeFromNow(double seconds);
void addActiveOpenConnection(ConnectionList<TCPState> &clist, Connection &c, unsigned int seq_number);
void activeOpen(MinetHandle mux, Connection &c, unsigned int seq_number);
void sendAckPack(Connection &c, MinetHandle mux, unsigned int ack_number, unsigned int seq_number, unsigned short win_size, Buffer &buf);
void updateConnectionStateMapping(ConnectionList<TCPState> &clist, Connection &c, unsigned int req_seq_number, unsigned int req_ack_number, unsigned int res_seq_number, unsigned int res_ack_number, size_t datalen, unsigned short rwnd, unsigned int newstate);
void updateReceiveBuffer(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen);
void sendConnectSocketResponse(SockRequestResponse &req, MinetHandle sock);
void sendZeroErrorStatus(MinetHandle sock);
void addAcceptSocketConnection(ConnectionList<TCPState> &clist, SockRequestResponse &req);
void sendNewConnectionToSocket(MinetHandle sock, Connection &c);
IPHeader setIPHeaders(Connection &c, unsigned int tcp_header_size, unsigned int data_length);
void handleSocketStatus(ConnectionList<TCPState> &clist, SockRequestResponse &status);
void sendDataToSocket(ConnectionList<TCPState> &clist, Connection &c, MinetHandle sock);
void sendDataFromSocket(Connection &c, ConnectionList<TCPState> &clist, MinetHandle mux, MinetHandle sock, Buffer &buf);
void sendLastN(ConnectionList<TCPState> &clist, Connection &c, Buffer &to_send, MinetHandle mux);
void updateSendBuffer(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen);
 
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

  while (MinetGetNextEvent(event, 1.0)==0) {
    // if we received an unexpected type of event, print error
    //cerr << event << endl;
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
        cerr << "packet's raw size is: " << p.GetRawSize() << endl;
        unsigned tcphlen=TCPHeader::EstimateTCPHeaderLength(p);
        cerr << "estimated header len="<<tcphlen<<"\n" << endl;
        p.ExtractHeaderFromPayload<TCPHeader>(tcphlen);
        IPHeader iph=p.FindHeader(Headers::IPHeader);
        TCPHeader tcph=p.FindHeader(Headers::TCPHeader);

        cerr << "received packet " << p << endl;

        Connection c;
        // note that this is flipped around because
        // "source" is interepreted as "this machine"
        unsigned char ipLen = 0;
        unsigned short totalLen = 0;
        iph.GetDestIP(c.src);
        iph.GetSourceIP(c.dest);
        iph.GetHeaderLength(ipLen);
        iph.GetProtocol(c.protocol);
        tcph.GetDestPort(c.srcport);
        tcph.GetSourcePort(c.destport);

        ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);

        cerr << "TCP Packet: IP Header is "<<iph<<" and ";
        cerr << "TCP Header is "<<tcph << " and ";

        cerr << "Checksum is " << (tcph.IsCorrectChecksum(p) ? "VALID" : "INVALID");

        Buffer &rawPayload = p.GetPayload();
        unsigned char tcpLen = 0;
        unsigned char flags = 0;
        unsigned int seq_number = 0;
        unsigned short win_size = 0;
        tcph.GetHeaderLen(tcpLen);
        tcph.GetFlags(flags);
        tcph.GetSeqNum(seq_number);
        tcph.GetWinSize(win_size);

        iph.GetTotalLength(totalLen);

        cerr << " \n\n total length of ip packet is " << totalLen << endl;
        unsigned tcpLenBytes = unsigned(tcpLen) * 4;
        unsigned ipLenBytes = unsigned(ipLen) * 4;
        unsigned bytesToExtract = unsigned(totalLen) - tcpLenBytes - ipLenBytes;
        cerr << " total length of ip header is " << ipLenBytes << endl;
        cerr << "extracting bytes: " << bytesToExtract << endl;
        Buffer &buf = rawPayload.ExtractFront(bytesToExtract);
        size_t buflen = buf.GetSize();
        cerr << "new buf is " << buf << endl;

        if (cs!=clist.end()) {
          cerr << "connection TCPState state is " << (*cs).state.GetState() << endl;
          if ((*cs).state.GetState() == LISTEN) {
            cerr << "new seq_number is " << seq_number + 1 << endl;
            if (IS_SYN(flags)) {
              cerr << "It's a SYN! Sending SYN ACK" << endl;
              sendSynAck(c, mux, seq_number, 300, 10000);
              addSynAckMapping(c, seq_number, 300, win_size, clist);
              cerr << "added new connection to state mapping" << endl;
            }
          }
        }
        if (IS_ACK(flags)) {
          cerr << "received an ACK, updating states " << endl;
          handleAck(clist, c, buf, buflen, tcph, mux, sock);
        }

        
        char sample[buflen+1];
        buf.GetData(sample, buflen, 0);
        sample[buflen] = '\0';
        cerr << " buffer length is " << buf.GetSize();
        cerr << " tcp header len is " << tcpLen;
        cerr << " printing data " << sample;

      }
      //  Data from the Sockets layer above  //
      if (event.handle==sock) {
        SockRequestResponse req;
        MinetReceive(sock,req);
        cerr << "req is " << req << endl;
        if (req.type == ACCEPT) {
          cerr << "Socket request type is ACCEPT \n" << endl;
          addAcceptSocketConnection(clist, req);
          // Sending the response back to the socket
          sendZeroErrorStatus(sock);
        }
        if (req.type == CONNECT) {
          cerr << "Socket request type is CONNECT \n" << endl;
          // TODO: remove hardcoded seq number
          unsigned int hardcoded_seq_number = 500;
          activeOpen(mux, req.connection, hardcoded_seq_number);
          addActiveOpenConnection(clist, req.connection, hardcoded_seq_number);
          sendConnectSocketResponse(req, sock);
        }
        if (req.type == FORWARD) {
          sendZeroErrorStatus(sock);
        }
        if (req.type == STATUS) {
          handleSocketStatus(clist, req);
        }
        if (req.type == WRITE) {
          cerr << "sending new data from socket " << endl;
          sendDataFromSocket(req.connection, clist, mux, sock, req.data);
        }
      }
    }
  }
  return 0;
}

void sendDataFromSocket(Connection &c, ConnectionList<TCPState> &clist, MinetHandle mux, MinetHandle sock, Buffer &buf) {
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  if (cs != clist.end()) {
    ConnectionToStateMapping<TCPState> mapping = *cs;
    if (mapping.state.GetState() == ESTABLISHED || mapping.state.GetState() == SEND_DATA) {
      unsigned offsetlastsent = 0;
      size_t bytesize = 0;
      unsigned bytes = buf.GetSize();
      mapping.state.SendPacketPayload(offsetlastsent, bytesize, bytes);
      //unsigned int bytes = MIN_MACRO(TCP_MAXIMUM_SEGMENT_SIZE, buf.GetSize());
      //bytes = MIN_MACRO(bytes, mapping.state.rwnd);
      //Buffer &buf = req.data.ExtractFront(bytes);
      Packet p(buf);
      IPHeader iph = setIPHeaders(c, 20, bytesize);
      p.PushFrontHeader(iph);
      TCPHeader tcph;
      tcph.SetSourcePort(c.srcport,p);
      tcph.SetDestPort(c.destport,p);
      unsigned char hardcode_len = 5;
      tcph.SetHeaderLen(hardcode_len, p);
      unsigned int res_seq_number = mapping.state.GetLastSent();
      tcph.SetSeqNum(res_seq_number, p);
      unsigned int res_ack_number = mapping.state.GetLastRecvd();
      tcph.SetAckNum(res_ack_number, p);
      // TODO: remove hardcoded window size
      tcph.SetWinSize(10000, p);
      unsigned char flags = 0;
      SET_ACK(flags);
      SET_PSH(flags);
      tcph.SetFlags(flags, p);
      p.PushBackHeader(tcph);
      int results = MinetSend(mux,p);
      cerr << "sending packet: " << p << endl;
      cerr << "sending tcp header: " << tcph << endl;
      // giving 0 for req_seq_number, because it is not used
      updateConnectionStateMapping(clist, c, 0, mapping.state.GetLastAcked(), res_seq_number, res_ack_number, bytesize, mapping.state.rwnd, SEND_DATA);
      updateSendBuffer(clist, c, buf, bytesize);
      // now send a status back to the socket
      SockRequestResponse res;
      res.type = STATUS;
      res.connection = c;
      res.bytes = bytesize;
      if (results < 0) {
        res.error = results;
      } else {
        res.error = EOK;
      }
      MinetSend(sock, res);
    }
  }
}

void updateSendBuffer(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen) {
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  if (cs != clist.end()) {
    ConnectionToStateMapping<TCPState> mapping = *cs;
    mapping.state.SendBuffer.AddBack(buf);
    char dataBuf[buflen + 1];
    buf.GetData(dataBuf, buflen, 0);
    dataBuf[buflen] = '\0';
    cerr << "Data added to send buffer: " << dataBuf << endl;
    clist.erase(cs);
    clist.push_front(mapping);
  }
}

void handleSocketStatus(ConnectionList<TCPState> &clist, SockRequestResponse &status) {
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(status.connection);
  if (cs != clist.end()) {
    // delete the number of bytes from the send buffer that the socket received
    // what if multiple packets arrive and we write multiple times before STATUS
    // is received?
    ConnectionToStateMapping<TCPState> mapping = *cs;
    cerr << "removing bytes: " << status.bytes << endl;
    mapping.state.RecvBuffer.ExtractFront(status.bytes);
    clist.erase(cs);
    clist.push_front(mapping);
  }
}

void sendConnectSocketResponse(SockRequestResponse &req, MinetHandle sock) {
  SockRequestResponse res;
  res.type = STATUS;
  res.connection = req.connection;
  res.error = EOK;
  MinetSend(sock, res);
}

void sendZeroErrorStatus(MinetHandle sock) {
  SockRequestResponse res;
  res.type = STATUS;
  res.error = EOK;
  MinetSend(sock, res);
}

void addAcceptSocketConnection(ConnectionList<TCPState> &clist, SockRequestResponse &req) {
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
  m.state = TCPState(seq_number, SYN_SENT, 500);
  // convention assume last sent is the seq number + the size of the buf or +1 in Syn case
  //m.state.SetLastSent(seq_number);
  // TODO: change hardcoded timer tries value
  //m.state.SetTimerTries(500);
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
  sleep(1); 
  int secondResult = MinetSend(mux, activeOpenPacket);
}

void handleAck(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen, TCPHeader tcph, MinetHandle mux, MinetHandle sock) {
  Time currentTime = Time();
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  unsigned int req_seq_number = 0;
  unsigned int req_ack_number = 0;
  unsigned short rwnd = 0;
  tcph.GetSeqNum(req_seq_number);
  tcph.GetAckNum(req_ack_number);
  tcph.GetWinSize(rwnd);
  unsigned char flags = 0;
  tcph.GetFlags(flags);
  cerr << "handling ack" << endl; 
  if (cs != clist.end()) {
    ConnectionToStateMapping<TCPState> mapping = *cs;
    // there is a connection for which to handle ACK
    cerr << " testing state of connection" <<endl;
    switch(mapping.state.stateOfcnx) {
      case SYN_RCVD:
        // change state to established and deactivate timer
          cerr << "In SYN_RCVD" <<endl;
          cerr << "req_ack_number: " << req_ack_number;
        if (req_ack_number == mapping.state.last_sent) {
          cerr << "In SYN_RCVD changing state" <<endl;
          mapping.state.stateOfcnx = ESTABLISHED;
          mapping.state.SetLastAcked(req_ack_number);
          mapping.bTmrActive = false;
          clist.erase(cs);
          clist.push_front(mapping);
          sendNewConnectionToSocket(sock, c);
        }
        break;
      case SEND_DATA:
      {
        cerr << "IN SEND_DATA" << endl;
        // update the ack number of the connection state
        unsigned int res_seq_number = mapping.state.GetLastSent();
        unsigned int res_ack_number = mapping.state.GetLastRecvd();
        updateConnectionStateMapping(clist, c, mapping.state.GetLastRecvd(), req_ack_number, res_seq_number, res_ack_number, 0, rwnd, SEND_DATA);
        cs = clist.FindMatching(c);
        mapping = *cs;
      }
      case ESTABLISHED:
        // check if the segment contains data
        cerr << "In ESTABLISHED \n" << endl;
        if (buflen > 0) {
          //cerr << "Established case with buflen > 0 " << endl;
          // check if the segment # is equal to last received
          cerr << "req_seq_number: "<< req_seq_number << endl;
          cerr << "GetLastRecvd: "<< mapping.state.GetLastRecvd() << endl;
          // if the packet is in order, continue
          if (req_seq_number == mapping.state.GetLastRecvd()) {
            // segment received is directly after last one; send an ACK
            // of seq_number + data length; make sure ack_number is mod 2^32
            cerr << "Established case with seq_number == last_recved" << endl;
            cerr << "buffer contents: " << buf << endl;
            unsigned int res_ack_number = (req_seq_number + buflen) & 0xffffffff;
            cerr << res_ack_number << endl;

            // check that the ack is equal to our last sent (last packet's
            // seq_number + last packet's size)
            cerr << "req_ack_number: " << req_ack_number << endl;
            cerr << "GetLastSent: " << mapping.state.GetLastSent() << endl;;
            if (req_ack_number == mapping.state.GetLastSent()) {
              // seq_number is simply our last sent
              unsigned int res_seq_number = mapping.state.GetLastSent();
              // window size is equal to receive buffer's size
              
              cerr << "sending ACK back in Established" <<endl;
              Buffer emptyBuf = Buffer();
              sendAckPack(c, mux, res_ack_number, res_seq_number, 10000, emptyBuf);
              // TODO: adjust for sending data - right now datalen is 0 due to 
              // not sending data
              updateConnectionStateMapping(clist, c, req_seq_number, req_ack_number, res_seq_number, res_ack_number, 0, rwnd, mapping.state.GetState());
              updateReceiveBuffer(clist, c, buf, buflen);
              sendDataToSocket(clist, c, sock);
            } 
          } else {
            // if out of order, just send back an ACK for the last received
            unsigned int res_seq_number = mapping.state.GetLastSent();
            unsigned int res_ack_number = mapping.state.GetLastRecvd();
            Buffer emptyBuf = Buffer();
            sendAckPack(c, mux, res_ack_number, res_seq_number, 10000, emptyBuf);
          }
        }
        break;
      case SYN_SENT:
        if (IS_SYN(flags)){
          cerr << "IS_SYN flag detected, SYN ACK received" <<endl;
          unsigned int res_ack_number = req_seq_number + 1;
          cerr << "mapping last sent: " << mapping.state.GetLastSent() << endl;
          cerr << "req_ack_number: " << req_ack_number << endl;
          if (req_ack_number == mapping.state.GetLastSent() + 1) {
            cerr << "sending ack in response to synack" << endl;
            // maybe don't add a 1 here?
            unsigned int res_seq_number = req_ack_number;
            Buffer emptyBuf = Buffer();
            sendAckPack(c, mux, res_ack_number, res_seq_number, 10000, emptyBuf);
            sleep(1);
            sendAckPack(c, mux, res_ack_number, res_seq_number, 10000, emptyBuf);
            // don't increment ack number after sending ack pack
            updateConnectionStateMapping(clist, c, req_seq_number, req_ack_number, res_seq_number, res_ack_number, 0, rwnd, ESTABLISHED);
            // using the same response to socket layer as in ACCEPT incoming connection 
            sendNewConnectionToSocket(sock, c);
          }
        }    
        break;
      default:
        break;
    }
  }
}

void sendNewConnectionToSocket(MinetHandle sock, Connection &c) {
  SockRequestResponse res;
  res.connection = c; 
  res.type = WRITE;
  res.bytes = 0;
  res.error = EOK;
  int result = MinetSend(sock, res);
  cerr << "sent new connection to socket: " << res << endl;
  if (result < 0) {
    cerr << "an error occurred during sending socket new connection " << endl;
  } else {
    cerr << "sending new socket connection successful " << endl;
  }
}

IPHeader setIPHeaders(Connection &c, unsigned int tcp_header_size, unsigned int data_length) {
  IPHeader resiph = IPHeader();
  resiph.SetDestIP(c.dest);
  resiph.SetSourceIP(c.src);
  resiph.SetProtocol(c.protocol);
  resiph.SetTotalLength(IP_HEADER_BASE_LENGTH + tcp_header_size + data_length);
  return resiph;
}

void updateReceiveBuffer(ConnectionList<TCPState> &clist, Connection &c, Buffer &buf, size_t buflen) {
  // right now, just replace the RecvBuffer contents of the TCPState
  // with the new buffer and print it out
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  if (cs != clist.end()) {
    ConnectionToStateMapping<TCPState> mapping = *cs;
    mapping.state.RecvBuffer.AddBack(buf);
    char dataBuf[buflen + 1];
    buf.GetData(dataBuf, buflen, 0);
    dataBuf[buflen] = '\0';
    cerr << "Data: " << dataBuf << endl;
    clist.erase(cs);
    clist.push_front(mapping);
  }
}

void sendDataToSocket(ConnectionList<TCPState> &clist, Connection &c, MinetHandle sock) {
  // don't know if we need bytes here or not
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  if (cs != clist.end()) {
    ConnectionToStateMapping<TCPState> mapping = *cs;
    SockRequestResponse write;
    write.connection = c;
    write.data.AddBack(mapping.state.RecvBuffer);
    write.type = WRITE;
    write.bytes = mapping.state.RecvBuffer.GetSize();
    int result = MinetSend(sock, write);
    cerr << "sending write to socket: " << write << endl;
    if (result < 0) {
      cerr << "error sending data" << endl;
    } else {
      cerr << "sending data successful, waiting for status reply" << endl;
    }
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
    mapping.timeout = timeFromNow(5.0);
    mapping.bTmrActive = true;
    clist.erase(cs);
    clist.push_front(mapping);
  }
}

void sendAckPack(Connection &c, MinetHandle mux, unsigned int ack_number, unsigned int seq_number, unsigned short win_size, Buffer &buf) {
  Packet ackpack;
  if (buf.GetSize() > 0) {
    ackpack = Packet(buf);
  } else {
    ackpack = Packet();
  }
  IPHeader resiph = setIPHeaders(c, 20, buf.GetSize());
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
  //int secondResult = MinetSend(mux, ackpack);
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
      //cerr << "handling a timed out connection " << endl;
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
        clist.erase(i);
        mapping.timeout = timeout;
        clist.push_front(mapping);
        switch(mapping.state.stateOfcnx) {
          case SYN_RCVD:
            // server trying to resend SYNACK   
            sendSynAck(mapping.connection, mux, mapping.state.last_recvd , mapping.state.last_sent , 10000);
            cerr << "resending synack " << endl;
            break;
          case SYN_SENT:
            activeOpen(mux, mapping.connection, mapping.state.GetLastSent() - 1);
            cerr << "resending syn " << endl; 
            break;
          case SEND_DATA:
            cerr << "timed out - resending last N packets" << endl;
            sendLastN(clist, mapping.connection, mapping.state.SendBuffer, mux);
            break;
          default:
            break;
        }
      }
      checkForTimedOutConnection(clist, mux);
    }
  }
}

void sendLastN(ConnectionList<TCPState> &clist, Connection &c, Buffer &to_send, MinetHandle mux) {
  ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
  if (cs != clist.end()) {
    ConnectionToStateMapping<TCPState> mapping = *cs;
    unsigned offsetlastsent = 0;
    size_t bytesize = 0;
    unsigned bytes = to_send.GetSize();
    mapping.state.SendPacketPayload(offsetlastsent, bytesize, bytes);
    //unsigned int bytes = MIN_MACRO(TCP_MAXIMUM_SEGMENT_SIZE, to_send.GetSize());
    //bytes = MIN_MACRO(bytes, mapping.state.rwnd);
    Packet p(to_send.ExtractFront(bytesize));
    IPHeader iph = setIPHeaders(mapping.connection, 20, bytesize);
    p.PushFrontHeader(iph);
    TCPHeader tcph;
    tcph.SetSourcePort(mapping.connection.srcport,p);
    tcph.SetDestPort(mapping.connection.destport,p);
    unsigned char hard_coded_headerlen = 5;
    tcph.SetHeaderLen(hard_coded_headerlen, p);
    unsigned int res_seq_number = mapping.state.GetLastAcked() + mapping.state.RecvBuffer.GetSize();
    tcph.SetSeqNum(res_seq_number, p);
    unsigned int res_ack_number = mapping.state.GetLastRecvd();
    tcph.SetAckNum(res_ack_number, p);
    // TODO: remove hardcoded window size
    tcph.SetWinSize(10000, p);
    unsigned char flags = 0;
    SET_ACK(flags);
    SET_PSH(flags);
    tcph.SetFlags(flags, p);
    p.PushBackHeader(tcph);
    int results = MinetSend(mux,p);
    cerr << "resending packet: " << p << endl;
    cerr << "tcp header: " << tcph << endl;
    if (to_send.GetSize() > 0 && bytes > 0) {
      cerr << "recursively calling sendLastN" << endl;
      sendLastN(clist, c, to_send, mux);
    }
  }
}

void addSynAckMapping(Connection &c, unsigned int req_seq_number, unsigned int res_seq_number, unsigned short win_size, ConnectionList<TCPState> &clist) {
  ConnectionToStateMapping<TCPState> m;
  m.connection=c;
  m.state = TCPState(res_seq_number + 1, SYN_RCVD, 500);
  m.state.SetLastRecvd(req_seq_number + 1);
  // set this to the next ACK number we expect
  //m.state.SetLastSent(res_seq_number + 1);
  // TODO: figure out if rwnd is OUR receive window or THEIRS
  // should be THEIRS based on source code
  // turns out it's both
  m.state.SetSendRwnd(win_size);
  // expire a connection after sending only one SYNACK
  //m.state.SetTimerTries(500);
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
  IPHeader resiph = setIPHeaders(c, 20, 0);
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
  //sleep(1);
  //int secondResult = MinetSend(mux, synackpack);
  cerr << "sent packet " << synackpack << endl;
  if (result < 0) {
    cerr << "Minet Send resulted in error " << endl;
  } else {
    cerr << "Minet Send successful " << endl;
  }
  /*if (secondResult < 0) {
    cerr << "Minet second Send resulted in error " << endl;
  } else {
    cerr << "Minet second Send successful " << endl;
  }*/

}
