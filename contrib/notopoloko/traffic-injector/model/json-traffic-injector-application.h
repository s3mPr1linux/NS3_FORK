#ifndef JSON_TRAFFIC_INJECTOR_APPLICATION_H
#define JSON_TRAFFIC_INJECTOR_APPLICATION_H

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"


class JsonTrafficInjectorApplication : public ns3::Application
{
public:

    JsonTrafficInjectorApplication ();
  ~JsonTrafficInjectorApplication() override ;

  void Setup (const ns3::Address &address,
              uint16_t port,
              std::vector<uint32_t> packetSizes,
              std::vector<float> timeToSend,
              bool tcp,
              uint16_t portsToSpreadTraffic);
  void Start();
private:
  void StartApplication () override;
  void StopApplication () override ;
  void ReceivePacket (ns3::Ptr<ns3::Socket> socket);



  void ScheduleTx ();
  void SendPacket (uint64_t packetSize);

  std::vector<float>    m_timeToSend;
  std::vector<ns3::Ptr<ns3::Socket>> m_socket;
  ns3::Address          m_peer;
  uint16_t              m_peer_port;
  std::vector<uint32_t> m_packetSizes;
  uint16_t              m_currentPacketSize;
  uint32_t              m_nPackets;
  ns3::EventId          m_sendEvent;
  bool                  m_running;
  uint32_t              m_packetsSent;
  uint32_t              m_recvBack;
  uint64_t              m_bytesRecvBack;
  uint8_t               m_currentTime;
  bool                  m_tcp;
  uint16_t              m_portsToSpreadTraffic;
  uint16_t              m_currentPort;
};

#endif //JSON_TRAFFIC_INJECTOR_APPLICATION_H