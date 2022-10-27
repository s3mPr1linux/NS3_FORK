//
// Created by gabriel on 29/09/22.
//

#include "E2AP.h"
#include "ORAN-message-types.h"
#include "ORAN-indication-types.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("E2AP");

void
E2AP::HandlePayload (std::string src_endpoint, std::string dest_endpoint, Json payload)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (payload.contains ("TYPE"), "Payload from " + src_endpoint + " addressed to " + dest_endpoint
                                            + "does not contain a message type.");
  ORAN_MESSAGE_TYPES msg_type = payload.find ("TYPE").value ();

  // Check if we are not receiving invalid payloads
  if (m_endpointRoot == "/E2Node/0")
    {
      // E2T can't receive messages that should have originated on the RIC (himself)
      if (msg_type < RIC_SUBSCRIPTION_RESPONSE)
        {
          std::stringstream ss;
          ss << "E2T received message addressed to himself. Type=" << oran_msg_str.at (msg_type);
          NS_ASSERT(ss.str ().c_str ());
        }
    }
  else
    {
      // E2 nodes can't receive messages that should have originated on E2 Nodes (himselves)
      if (RIC_SUBSCRIPTION_RESPONSE <= msg_type && msg_type <= E2_NODE_CONFIGURATION_UPDATE)
        {
          std::stringstream ss;
          ss << m_endpointRoot << " received message addressed to himself. Type=" << oran_msg_str[msg_type];
          NS_ASSERT(ss.str ().c_str ());
        }
    }

  NS_LOG_FUNCTION("Handling payload with type " + oran_msg_str.at (msg_type) + " : " + to_string (payload));

  // Handle all O-RAN messages
  switch (msg_type)
    {
      // O-RAN WG3 E2AP v2.02 8.2.1.2
      // RIC initiated
      case RIC_SUBSCRIPTION_REQUEST:
        {
          NS_LOG_FUNCTION(
              m_endpointRoot + " subscribing " + src_endpoint + " to " + to_string (payload["RAN Function"]));

          if (payload["RIC Subscription Details"]["RIC Event Trigger Format"] != EVENT_TRIGGER_PERIODIC)
            {
              NS_ABORT_MSG ("Inexistent RIC event trigger format");
            }

          // Fetch data from request
          uint32_t period = payload["RIC Subscription Details"]["RIC Event Trigger Definition"]["Period"];
          std::string periodic_endpoint_to_report = payload["RAN Function"];

          auto it = m_endpointPeriodicityAndBuffer.find (periodic_endpoint_to_report);

          // Setup event loop
          if (it != m_endpointPeriodicityAndBuffer.end ())
            {
              NS_LOG_FUNCTION(m_endpointRoot + " failed to subscribe " + src_endpoint + " to "
                              + to_string (payload["RAN Function"]));
              Json RIC_SUBSCRIPTION_FAILURE_MESSAGE;
              RIC_SUBSCRIPTION_FAILURE_MESSAGE["DEST_ENDPOINT"] = src_endpoint;
              RIC_SUBSCRIPTION_FAILURE_MESSAGE["PAYLOAD"]["TYPE"] = RIC_SUBSCRIPTION_FAILURE;
              RIC_SUBSCRIPTION_FAILURE_MESSAGE["PAYLOAD"]["RAN Function"] = periodic_endpoint_to_report;
              SendPayload (RIC_SUBSCRIPTION_FAILURE_MESSAGE);
              return;
            }

          // Register event loop to send payload
          EventId event = Simulator::Schedule (MilliSeconds (period), &E2AP::PeriodicReport, this, src_endpoint, period, periodic_endpoint_to_report);
          struct PeriodicReportStruct entry{period, event, src_endpoint, {}, {}};
          m_endpointPeriodicityAndBuffer.emplace (periodic_endpoint_to_report, entry);

          //todo: handle actions
          // payload["RIC Subscription Details"]["Sequence of Actions"];

          NS_LOG_FUNCTION(
              m_endpointRoot + " subscribed " + src_endpoint + " to " + to_string (payload["RAN Function"]));
          Json RIC_SUBSCRIPTION_RESPONSE_MESSAGE;
          RIC_SUBSCRIPTION_RESPONSE_MESSAGE["DEST_ENDPOINT"] = src_endpoint;
          RIC_SUBSCRIPTION_RESPONSE_MESSAGE["PAYLOAD"]["TYPE"] = RIC_SUBSCRIPTION_RESPONSE;
          RIC_SUBSCRIPTION_RESPONSE_MESSAGE["PAYLOAD"]["RAN Function"] = periodic_endpoint_to_report;
          SendPayload (RIC_SUBSCRIPTION_RESPONSE_MESSAGE);

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.1.2
      // E2 initiated
      case RIC_SUBSCRIPTION_RESPONSE:
        {
          NS_LOG_FUNCTION(dest_endpoint + " was successfully subscribed to " + to_string (payload["RAN Function"]));

          // Store registered endpoint on the RIC
          sSubscribeToEndpoint (payload["RAN Function"], dest_endpoint);
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.1.3
      // E2 initiated
      case RIC_SUBSCRIPTION_FAILURE:
        {
          NS_LOG_FUNCTION(dest_endpoint + " failed to subscribed to " + to_string (payload["RAN Function"]));
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.2.2
      // RIC initiated
      case RIC_SUBSCRIPTION_DELETE_REQUEST:
        {
          std::string endpointToUnsubscribe = payload["RAN Function"];
          NS_LOG_FUNCTION(m_endpointRoot + " unsubscribing " + src_endpoint + " from " + endpointToUnsubscribe);
          auto endpointIt = m_endpointPeriodicityAndBuffer.find (endpointToUnsubscribe);
          if (endpointIt == m_endpointPeriodicityAndBuffer.end ())
            {
              // Failure, send failure message
              NS_LOG_FUNCTION(m_endpointRoot + " doesn't have a subscription to " + endpointToUnsubscribe);
              Json RIC_SUBSCRIPTION_DELETE_FAILURE_MESSAGE;
              RIC_SUBSCRIPTION_DELETE_FAILURE_MESSAGE["DEST_ENDPOINT"] = src_endpoint;
              RIC_SUBSCRIPTION_DELETE_FAILURE_MESSAGE["PAYLOAD"]["TYPE"] = RIC_SUBSCRIPTION_DELETE_FAILURE;
              RIC_SUBSCRIPTION_DELETE_FAILURE_MESSAGE["PAYLOAD"]["RAN Function"] = endpointToUnsubscribe;
              SendPayload (RIC_SUBSCRIPTION_DELETE_FAILURE_MESSAGE);
            }
          else
            {
              NS_LOG_FUNCTION(m_endpointRoot + " have the subscription to " + endpointToUnsubscribe);

              // Cancel periodic event
              Simulator::Cancel (endpointIt->second.eventId);

              // Remove entry
              m_endpointPeriodicityAndBuffer.erase (endpointIt);

              NS_LOG_FUNCTION(m_endpointRoot + " removed the subscription to " + endpointToUnsubscribe);

              // Successful, send response message
              Json RIC_SUBSCRIPTION_DELETE_RESPONSE_MESSAGE;
              RIC_SUBSCRIPTION_DELETE_RESPONSE_MESSAGE["DEST_ENDPOINT"] = src_endpoint;
              RIC_SUBSCRIPTION_DELETE_RESPONSE_MESSAGE["PAYLOAD"]["TYPE"] = RIC_SUBSCRIPTION_DELETE_RESPONSE;
              RIC_SUBSCRIPTION_DELETE_RESPONSE_MESSAGE["PAYLOAD"]["RAN Function"] = endpointToUnsubscribe;
              SendPayload (RIC_SUBSCRIPTION_DELETE_RESPONSE_MESSAGE);
            }
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.2.2
      // E2 initiated
      case RIC_SUBSCRIPTION_DELETE_RESPONSE:
        {
          NS_LOG_FUNCTION(dest_endpoint + " was successfully unsubscribed to " + to_string (payload["RAN Function"]));

          // Remove registry of subscribed endpoint from the RIC
          sUnsubscribeToEndpoint (payload["RAN Function"], dest_endpoint);
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.2.3
      // E2 initiated
      case RIC_SUBSCRIPTION_DELETE_FAILURE:
        {
          NS_LOG_FUNCTION(dest_endpoint + " was not unsubscribed from " + to_string (payload["RAN Function"]));
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.2A.2
      // E2 initiated
      case RIC_SUBSCRIPTION_DELETE_REQUIRED:
        {
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.3.2
      // E2 initiated
      case RIC_INDICATION:
        {
          NS_LOG_FUNCTION (m_endpointRoot + " parsing RIC INDICATION message: " + to_string (payload));
          // Indications are complicated, handle them in a dedicated function
          HandleIndicationPayload (src_endpoint, dest_endpoint, payload);
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.4.2
      // RIC initiated
      case RIC_CONTROL_REQUEST:
        {
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.4.2
      // E2 initiated
      case RIC_CONTROL_ACKNOWLEDGE:
        {
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.2.4.3
      // E2 initiated
      case RIC_CONTROL_FAILURE:
        {
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.1.2
      // E2 initiated
      case E2_SETUP_REQUEST:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.1.2
      // RIC initiated
      case E2_SETUP_RESPONSE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.1.3
      // RIC initiated
      case E2_SETUP_FAILURE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.2.2
      // RIC or E2 initiated
      case RESET_REQUEST:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.2.3
      // RIC or E2 initiated
      case RESET_RESPONSE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.3.2
      // RIC or E2 initiated
      case ERROR_INDICATION:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.4.2
      // RIC initiated
      case RIC_SERVICE_QUERY:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.4.2
      // E2 initiated
      case RIC_SERVICE_UPDATE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.4.2
      // RIC initiated
      case RIC_SERVICE_UPDATE_ACKNOWLEDGE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.4.3
      // RIC initiated
      case RIC_SERVICE_UPDATE_FAILURE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.5.2
      // E2 initiated
      case E2_NODE_CONFIGURATION_UPDATE:
        {
          bool successful = true;
          bool temp = true;
          std::vector<std::string> failed_addition_list{};
          std::vector<std::vector<std::string>> failed_update_list{};
          std::vector<std::string> failed_removal_list{};

          if (payload.contains ("COMPONENT_CONFIGURATION_ADDITION_LIST"))
            {
              for (std::string new_endpoint: payload.find ("COMPONENT_CONFIGURATION_ADDITION_LIST").value ())
                {
                  temp = true & sRegisterEndpoint (src_endpoint, new_endpoint);
                  if (!temp)
                    {
                      failed_addition_list.push_back (new_endpoint);
                    }
                  successful &= temp;
                }
            }
          if (payload.contains ("COMPONENT_CONFIGURATION_UPDATE_LIST"))
            {
              for (std::vector<std::string> endpoints: payload.find ("COMPONENT_CONFIGURATION_UPDATE_LIST").value ())
                {
                  temp = true & sUpdateEndpoint (src_endpoint, endpoints[0], endpoints[1]);
                  if (!temp)
                    {
                      failed_update_list.push_back (endpoints);
                    }
                  successful &= temp;
                }
            }
          if (payload.contains ("COMPONENT_CONFIGURATION_REMOVAL_LIST"))
            {
              for (std::string old_endpoint: payload.find ("COMPONENT_CONFIGURATION_REMOVAL_LIST").value ())
                {
                  temp = true & sRemoveEndpoint (src_endpoint, old_endpoint);
                  if (!temp)
                    {
                      failed_removal_list.push_back ({src_endpoint});
                    }
                  successful &= temp;
                }
            }
          if (successful)
            {
              Json E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE_MESSAGE;
              E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE_MESSAGE["DEST_ENDPOINT"] = src_endpoint;
              E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE_MESSAGE["PAYLOAD"]["TYPE"] = E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE;
              if (payload.contains ("COMPONENT_CONFIGURATION_ADDITION_LIST"))
                {
                  E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE_MESSAGE["PAYLOAD"]["COMPONENT_CONFIGURATION_ADDITION_LIST"] = payload.find ("COMPONENT_CONFIGURATION_ADDITION_LIST").value ();
                }
              if (payload.contains ("COMPONENT_CONFIGURATION_UPDATE_LIST"))
                {
                  E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE_MESSAGE["PAYLOAD"]["COMPONENT_CONFIGURATION_UPDATE_LIST"] = payload.find ("COMPONENT_CONFIGURATION_UPDATE_LIST").value ();
                }
              if (payload.contains ("COMPONENT_CONFIGURATION_REMOVAL_LIST"))
                {
                  E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE_MESSAGE["PAYLOAD"]["COMPONENT_CONFIGURATION_REMOVAL_LIST"] = payload.find ("COMPONENT_CONFIGURATION_REMOVAL_LIST").value ();
                }
              SendPayload (E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE_MESSAGE);
            }
          else
            {
              Json E2_NODE_CONFIGURATION_UPDATE_FAILURE_MESSAGE;
              E2_NODE_CONFIGURATION_UPDATE_FAILURE_MESSAGE["DEST_ENDPOINT"] = src_endpoint;
              E2_NODE_CONFIGURATION_UPDATE_FAILURE_MESSAGE["PAYLOAD"]["TYPE"] = E2_NODE_CONFIGURATION_UPDATE_FAILURE;
              if (payload.contains ("COMPONENT_CONFIGURATION_ADDITION_LIST"))
                {
                  E2_NODE_CONFIGURATION_UPDATE_FAILURE_MESSAGE["PAYLOAD"]["COMPONENT_CONFIGURATION_ADDITION_LIST"] = failed_addition_list;
                }
              if (payload.contains ("COMPONENT_CONFIGURATION_UPDATE_LIST"))
                {
                  E2_NODE_CONFIGURATION_UPDATE_FAILURE_MESSAGE["PAYLOAD"]["COMPONENT_CONFIGURATION_UPDATE_LIST"] = failed_update_list;
                }
              if (payload.contains ("COMPONENT_CONFIGURATION_REMOVAL_LIST"))
                {
                  E2_NODE_CONFIGURATION_UPDATE_FAILURE_MESSAGE["PAYLOAD"]["COMPONENT_CONFIGURATION_REMOVAL_LIST"] = failed_removal_list;
                }
              SendPayload (E2_NODE_CONFIGURATION_UPDATE_FAILURE_MESSAGE);
            }
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.5.2
      // RIC initiated
      case E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE:
        {
          //todo: handle successful case
          NS_LOG_FUNCTION ("Received E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE from RIC: " + to_string (payload));
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.5.3
      // RIC initiated
      case E2_NODE_CONFIGURATION_UPDATE_FAILURE:
        {
          //todo: handle failure case
          NS_LOG_FUNCTION ("Received E2_NODE_CONFIGURATION_UPDATE_FAILURE from RIC: " + to_string (payload));
        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.6.2
      // RIC initiated
      case E2_CONNECTION_UPDATE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.6.2
      // E2 initiated
      case E2_CONNECTION_UPDATE_ACKNOWLEDGE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.6.3
      // E2 initiated
      case E2_CONNECTION_UPDATE_FAILURE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.7.2
      // RIC or E2 initiated
      case E2_REMOVAL_REQUEST:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.7.2
      // RIC or E2 initiated
      case E2_REMOVAL_RESPONSE:
        {

        }
      break;
      // O-RAN WG3 E2AP v2.02 8.3.7.3
      // RIC or E2 initiated
      case E2_REMOVAL_FAILURE:
        {

        }
      break;
      default:
        std::cout << this->m_endpointRoot << " failed to handle payload with RPC=" << payload["RPC"] << std::endl;
    }
}

void
E2AP::SendPayload (Json payload)
{
  // Add source endpoint
  payload["SRC_ENDPOINT"] = m_endpointRoot;

  // Replace destination endpoint with the corresponding root endpoint
  if (payload.contains ("DEST_ENDPOINT"))
    {
      payload["DEST_ENDPOINT"] = getEndpointRoot (payload["DEST_ENDPOINT"]);
    }

  if (m_endpointRoot != "/E2Node/0")
    {
      NS_LOG_FUNCTION (
          "Sending the payload of type " + oran_msg_str.at (payload.at ("PAYLOAD").at ("TYPE")) + " to the RIC: "
          + to_string (payload));
      m_socket->SendTo (encodeJsonToPacket (payload), 0, m_node0Address);
    }
  else
    {
      // If we are on the RIC, we can handle things locally if the payload is addressed to us
      if (payload["DEST_ENDPOINT"] == "/E2Node/0")
        {
          NS_LOG_FUNCTION ("RIC handling the payload of type " +
                           oran_msg_str.at (payload.at ("PAYLOAD").at ("TYPE")) +
                           " locally: " + to_string (payload));
          HandlePayload (payload["SRC_ENDPOINT"], m_endpointRoot, payload.at ("PAYLOAD"));
        }
        // Or we need to forward to the correct node
      else
        {
          NS_LOG_FUNCTION ("RIC forwarding the payload of type " +
                           oran_msg_str.at (payload.at ("PAYLOAD").at ("TYPE")) +
                           " to " + to_string (payload["DEST_ENDPOINT"]) + ": " + to_string (payload));
          m_socket->SendTo (encodeJsonToPacket (payload), 0, getAddressFromEndpointRoot (payload["DEST_ENDPOINT"]));
        }
    }
}

void
E2AP::RegisterEndpoint (std::string endpoint)
{
  NS_LOG_FUNCTION (this);
  Json E2_NODE_CONFIGURATION_UPDATE_MESSAGE;
  E2_NODE_CONFIGURATION_UPDATE_MESSAGE["DEST_ENDPOINT"] = "/E2Node/0";
  E2_NODE_CONFIGURATION_UPDATE_MESSAGE["PAYLOAD"]["TYPE"] = E2_NODE_CONFIGURATION_UPDATE;
  E2_NODE_CONFIGURATION_UPDATE_MESSAGE["PAYLOAD"]["COMPONENT_CONFIGURATION_ADDITION_LIST"] = std::vector<std::string>{
      endpoint};
  SendPayload (E2_NODE_CONFIGURATION_UPDATE_MESSAGE);
}

void
E2AP::UpdateEndpoint (std::string old_endpoint, std::string new_endpoint)
{
  NS_LOG_FUNCTION (this);
  Json E2_NODE_CONFIGURATION_UPDATE_MESSAGE;
  E2_NODE_CONFIGURATION_UPDATE_MESSAGE["DEST_ENDPOINT"] = "/E2Node/0";
  E2_NODE_CONFIGURATION_UPDATE_MESSAGE["PAYLOAD"]["TYPE"] = E2_NODE_CONFIGURATION_UPDATE;
  E2_NODE_CONFIGURATION_UPDATE_MESSAGE["PAYLOAD"]["COMPONENT_CONFIGURATION_UPDATE_LIST"] = std::vector<std::vector<std::string>>{
      {old_endpoint, new_endpoint}};
  SendPayload (E2_NODE_CONFIGURATION_UPDATE_MESSAGE);
}

void
E2AP::RemoveEndpoint (std::string endpoint)
{
  NS_LOG_FUNCTION (this);
  Json E2_NODE_CONFIGURATION_UPDATE_MESSAGE;
  E2_NODE_CONFIGURATION_UPDATE_MESSAGE["DEST_ENDPOINT"] = "/E2Node/0";
  E2_NODE_CONFIGURATION_UPDATE_MESSAGE["PAYLOAD"]["TYPE"] = E2_NODE_CONFIGURATION_UPDATE;
  E2_NODE_CONFIGURATION_UPDATE_MESSAGE["PAYLOAD"]["COMPONENT_CONFIGURATION_REMOVAL_LIST"] = std::vector<std::string>{
      endpoint};
  SendPayload (E2_NODE_CONFIGURATION_UPDATE_MESSAGE);
}

void
E2AP::SubscribeToEndpoint (std::string endpoint)
{
  SubscribeToEndpointPeriodic (endpoint, 1000);
}

void
E2AP::SubscribeToEndpointPeriodic (std::string endpoint, uint32_t periodicity_ms)
{
  NS_LOG_FUNCTION (m_endpointRoot + " subscribing to endpoint " + endpoint);
  Json RIC_SUBSCRIPTION_REQUEST_MESSAGE;
  RIC_SUBSCRIPTION_REQUEST_MESSAGE["DEST_ENDPOINT"] = endpoint;
  RIC_SUBSCRIPTION_REQUEST_MESSAGE["PAYLOAD"]["TYPE"] = RIC_SUBSCRIPTION_REQUEST;
  RIC_SUBSCRIPTION_REQUEST_MESSAGE["PAYLOAD"]["RAN Function"] = endpoint;
  RIC_SUBSCRIPTION_REQUEST_MESSAGE["PAYLOAD"]["RIC Subscription Details"];
  RIC_SUBSCRIPTION_REQUEST_MESSAGE["PAYLOAD"]["RIC Subscription Details"]["RIC Event Trigger Format"] = EVENT_TRIGGER_PERIODIC;
  RIC_SUBSCRIPTION_REQUEST_MESSAGE["PAYLOAD"]["RIC Subscription Details"]["RIC Event Trigger Definition"]["Period"] = periodicity_ms;
  RIC_SUBSCRIPTION_REQUEST_MESSAGE["PAYLOAD"]["RIC Subscription Details"]["Sequence of Actions"];//todo
  SendPayload (RIC_SUBSCRIPTION_REQUEST_MESSAGE);
}

void
E2AP::UnsubscribeToEndpoint (std::string endpoint)
{
  NS_LOG_FUNCTION (m_endpointRoot + " unsubscribing to endpoint " + endpoint);
  Json RIC_SUBSCRIPTION_DELETE_REQUEST_MESSAGE;
  RIC_SUBSCRIPTION_DELETE_REQUEST_MESSAGE["DEST_ENDPOINT"] = endpoint;
  RIC_SUBSCRIPTION_DELETE_REQUEST_MESSAGE["PAYLOAD"]["TYPE"] = RIC_SUBSCRIPTION_DELETE_REQUEST;
  RIC_SUBSCRIPTION_DELETE_REQUEST_MESSAGE["PAYLOAD"]["RAN Function"] = endpoint;
  SendPayload (RIC_SUBSCRIPTION_DELETE_REQUEST_MESSAGE);
}

void
E2AP::PeriodicReport (std::string subscriber_endpoint, uint32_t period_ms, std::string subscribed_endpoint)
{
  NS_LOG_FUNCTION(this << subscriber_endpoint << period_ms << subscribed_endpoint);

  auto it = m_endpointPeriodicityAndBuffer.find (subscribed_endpoint);
  NS_ASSERT_MSG (
      it != m_endpointPeriodicityAndBuffer.end (),
      "Endpoint " + subscribed_endpoint + " to report periodically was not found");

  // Send report
  Json RIC_INDICATION_MESSAGE;
  RIC_INDICATION_MESSAGE["DEST_ENDPOINT"] = subscriber_endpoint;
  RIC_INDICATION_MESSAGE["PAYLOAD"]["TYPE"] = RIC_INDICATION;
  RIC_INDICATION_MESSAGE["PAYLOAD"]["COLLECTION START TIME"] = it->second.collectionStartTime.ToString ();
  RIC_INDICATION_MESSAGE["PAYLOAD"]["MESSAGE"];
  RIC_INDICATION_MESSAGE["PAYLOAD"]["MESSAGE"]["TYPE"] = KPM_INDICATION_FORMAT_1; //todo: complement format fields, this is super non-conformant
  RIC_INDICATION_MESSAGE["PAYLOAD"]["MESSAGE"]["RAN FUNCTION"] = subscribed_endpoint;
  RIC_INDICATION_MESSAGE["PAYLOAD"]["MESSAGE"]["MEASUREMENTS"] = it->second.measurements;
  SendPayload (RIC_INDICATION_MESSAGE);

  // Reschedule report event
  EventId event = Simulator::Schedule (MilliSeconds (period_ms), &E2AP::PeriodicReport, this, subscriber_endpoint, period_ms, subscribed_endpoint);
  it->second.eventId = event;
  it->second.measurements.clear ();
  it->second.collectionStartTime = SystemWallClockTimestamp ();
}

void
E2AP::PublishToSubEndpointSubscribers (std::string endpoint, Json json)
{
  PublishToEndpointSubscribers (m_endpointRoot + endpoint, json);
}

void
E2AP::PublishToEndpointSubscribers (std::string complete_endpoint, Json json)
{
  NS_LOG_FUNCTION(this << complete_endpoint << to_string (json));

  // Do not push report if endpoint is not registered
  auto endpointIt = m_endpointToSubscribers.find (complete_endpoint);
  if (endpointIt == m_endpointToSubscribers.end ())
    {
      NS_LOG_FUNCTION(this << "Endpoint not subscribed:" << complete_endpoint);
      return;
    }

  // Published content is sent by the periodic reporting in E2Nodes
  std::string kpm = getSubEndpoint (m_endpointRoot, complete_endpoint); // Remove endpointRoot from full KPM endpoint

  if (!json.contains ("MEASUREMENTS"))
    {
      NS_ABORT_MSG("No measurements saved in json: " + to_string (json));
    }

  // If there is no entry for the KPM measurements, create an entry
  auto kpmIt = m_kpmToEndpointStorage.find (kpm);
  if (kpmIt == m_kpmToEndpointStorage.end ())
    {
      m_kpmToEndpointStorage.emplace (
          kpm,
          std::map<std::string, std::deque<PeriodicMeasurementStruct>>{});
      kpmIt = m_kpmToEndpointStorage.find (kpm);
    }

  // Find the measuring E2Node. If it doesn't exist, create an entry for it.
  auto measuringE2NodeIt = kpmIt->second.find (complete_endpoint);
  if (measuringE2NodeIt == kpmIt->second.end ())
    {
      kpmIt->second.emplace (complete_endpoint, std::deque<PeriodicMeasurementStruct>{});
      measuringE2NodeIt = kpmIt->second.find (complete_endpoint);
    }
  auto periodicMeasurement = PeriodicMeasurementStruct{SystemWallClockTimestamp ().ToString (), {json}};
  measuringE2NodeIt->second.push_front (periodicMeasurement);
}

// O-RAN WG3 E2SM KPM v2.00.03 7.3.2
// Trigger timer: only for REPORT, not INSERT/POLICY
// ["periodic"] = KPM interval


// O-RAN WG3 E2SM RC v01.02 7.3.1
std::vector<std::tuple<enum RIC_EVENT_TRIGGER_DEFINITION_STYLES, std::string, std::pair<uint8_t, uint8_t>>>
    RIC_EVENT_TRIGGER_DEFINITION_SUPPORTED_RIC_SERVICE_STYLE
    {
        {MESSAGE_EVENT,              "REPORT", {1, 1}},
        {MESSAGE_EVENT,              "POLICY", {1, 7}},
        {CALL_PROCESS_BREAKPOINT,    "REPORT", {2, 2}},
        {CALL_PROCESS_BREAKPOINT,    "INSERT", {1, 7}},
        {CALL_PROCESS_BREAKPOINT,    "POLICY", {1, 8}},
        {E2_NODE_INFORMATION_CHANGE, "REPORT", {3, 3}},
        {UE_INFORMATION_CHANGE,      "REPORT", {4, 4}},
        {ON_DEMAND,                  "REPORT", {5, 5}},
    };

// O-RAN WG3 E2SM KPM v2.00.03 7.4.1
// REPORT services
enum REPORT_KPM_SERVICES {
    E2_NODE_MEASUREMENT = 1,
    E2_NODE_MEASUREMENT_FOR_SINGLE_UE,
    E2_NODE_CONDITIONAL_MEASUREMENT_FOR_UES,
    E2_NODE_CONDITIONAL_MEASUREMENT_SET_FOR_UES,
    E2_NODE_MEASUREMENT_FOR_MULTIPLE_UES
};

// REPORTed measurements
// 5G https://www.etsi.org/deliver/etsi_TS/128500_128599/128552/16.10.00_60/ts_128552v161000p.pdf
// 4G https://www.etsi.org/deliver/etsi_TS/132400_132499/132425/14.01.00_60/ts_132425v140100p.pdf

void
E2AP::RegisterDefaultEndpoints ()
{
  NS_LOG_FUNCTION(this);
  for (auto &kpmEndpoints: m_defaultEndpointsKpmTypeAndUnit)
    {
      if (std::get<2> (kpmEndpoints.second) == IMPLEMENTED)
        {
          RegisterEndpoint ("/KPM/" + kpmEndpoints.first);
        }
    }
}

void
E2AP::SubscribeToDefaultEndpoints (const E2AP &e2NodeToSubscribeTo)
{
  NS_LOG_FUNCTION(this);
  for (auto &kpmEndpoints: m_defaultEndpointsKpmTypeAndUnit)
    {
      if (std::get<2> (kpmEndpoints.second) == IMPLEMENTED)
        {
          SubscribeToEndpoint (e2NodeToSubscribeTo.m_endpointRoot + "/KPM/" + kpmEndpoints.first);
        }
    }
}

void
E2AP::HandleIndicationPayload (std::string& src_endpoint, std::string& dest_endpoint, Json& payload)
{
  NS_LOG_FUNCTION_NOARGS();

  RIC_INDICATION_HEADER indicationHeader;
  NS_ASSERT(payload["MESSAGE"].contains ("HEADER"));
  from_json (payload["MESSAGE"]["HEADER"], indicationHeader);

  switch (indicationHeader.format)
    {
      case RIC_INDICATION_HEADER_FORMAT_1:
        {
          // REPORT services
          uint16_t eventTriggerConditionId = indicationHeader.contents.format_1.eventTriggerConditionID;
          std::cout << eventTriggerConditionId << std::endl;
        }
      break;
      case RIC_INDICATION_HEADER_FORMAT_2:
        {
          // INSERT services
          uint16_t rnti = indicationHeader.contents.format_2.RNTI;
          switch (indicationHeader.contents.format_2.RICInsertStyleType)
            {
              case RIC_INSERT_SERVICE_STYLES::RADIO_BEARER_CONTROL_REQUEST::VALUE:
                {
                  switch (indicationHeader.contents.format_2.InsertIndicationID)
                    {
                      case RIC_INSERT_SERVICE_STYLES::RADIO_BEARER_CONTROL_REQUEST::DRB_QOS_CONFIGURATION_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_BEARER_CONTROL_REQUEST::QOS_FLOW_MAPPING_CONFIGURATION_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_BEARER_CONTROL_REQUEST::LOGICAL_CHANNEL_CONFIGURATION_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_BEARER_CONTROL_REQUEST::RADIO_ADMISSION_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_BEARER_CONTROL_REQUEST::DRB_TERMINATION_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_BEARER_CONTROL_REQUEST::DRB_SPLIT_RATION_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_BEARER_CONTROL_REQUEST::PDCP_DUPLICATION_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      default:
                        NS_ABORT_MSG("Unknown Radio Bearer Control Request Indication ID");
                    }
                }
              break;
              case RIC_INSERT_SERVICE_STYLES::RADIO_RESOURCE_ALLOCATION_CONTROL_REQUEST::VALUE:
                {
                  switch (indicationHeader.contents.format_2.InsertIndicationID)
                    {
                      case RIC_INSERT_SERVICE_STYLES::RADIO_RESOURCE_ALLOCATION_CONTROL_REQUEST::DRX_PARAMETER_CONFIGURATION_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_RESOURCE_ALLOCATION_CONTROL_REQUEST::SR_PERIODICITY_CONFIGURATION_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_RESOURCE_ALLOCATION_CONTROL_REQUEST::SPS_PARAMETERS_CONFIGURATION_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_RESOURCE_ALLOCATION_CONTROL_REQUEST::CONFIGURED_GRANT_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_RESOURCE_ALLOCATION_CONTROL_REQUEST::CQI_TABLE_CONFIGURATION_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_RESOURCE_ALLOCATION_CONTROL_REQUEST::SLICE_LEVEL_PRB_QUOTA_REQUEST::VALUE:
                        {

                        }
                      break;
                      default:
                        NS_ABORT_MSG("Unknown Radio Resource Allocation Control Request Indication ID");
                    }
                }
              break;
              case RIC_INSERT_SERVICE_STYLES::CONNECTED_MODE_MOBILITY_CONTROL_REQUEST::VALUE:
                {
                  switch (indicationHeader.contents.format_2.InsertIndicationID)
                    {
                      case RIC_INSERT_SERVICE_STYLES::CONNECTED_MODE_MOBILITY_CONTROL_REQUEST::HANDOVER_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::CONNECTED_MODE_MOBILITY_CONTROL_REQUEST::CONDITIONAL_HANDOVER_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::CONNECTED_MODE_MOBILITY_CONTROL_REQUEST::DUAL_ACTIVE_PROTOCOL_STACK_HANDOVER_CONTROL_REQUEST::VALUE:
                        {

                        }
                        break;
                      default:
                        NS_ABORT_MSG("Unknown Radio Resource Allocation Control Request Indication ID");
                    }
                }
              break;
              case RIC_INSERT_SERVICE_STYLES::RADIO_ACCESS_CONTROL_REQUEST::VALUE:
                {
                  switch (indicationHeader.contents.format_2.InsertIndicationID)
                    {
                      case RIC_INSERT_SERVICE_STYLES::RADIO_ACCESS_CONTROL_REQUEST::UE_ADMISSION_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_ACCESS_CONTROL_REQUEST::RACH_BACKOFF_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_ACCESS_CONTROL_REQUEST::ACCESS_BARRING_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_ACCESS_CONTROL_REQUEST::RRC_CONNECTION_RELEASE::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::RADIO_ACCESS_CONTROL_REQUEST::RRC_CONNECTION_REJECT::VALUE:
                        {

                        }
                      break;
                      default:
                        NS_ABORT_MSG("Unknown Radio Resource Allocation Control Request Indication ID");
                    }
                }
              break;
              case RIC_INSERT_SERVICE_STYLES::DUAL_CONNECTIVITY_CONTROL_REQUEST::VALUE:
                {
                  switch (indicationHeader.contents.format_2.InsertIndicationID)
                    {
                      case RIC_INSERT_SERVICE_STYLES::DUAL_CONNECTIVITY_CONTROL_REQUEST::DC_SECONDARY_NODE_ADDITION_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::DUAL_CONNECTIVITY_CONTROL_REQUEST::DC_SECONDARY_NODE_MODIFICATION_AND_RELEASE_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::DUAL_CONNECTIVITY_CONTROL_REQUEST::DC_PSCELL_CHANGE_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::DUAL_CONNECTIVITY_CONTROL_REQUEST::DC_SECONDARY_NODE_CHANGE_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::DUAL_CONNECTIVITY_CONTROL_REQUEST::DC_DRB_TERMINATION_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      default:
                        NS_ABORT_MSG("Unknown Radio Resource Allocation Control Request Indication ID");
                    }
                }
              break;
              case RIC_INSERT_SERVICE_STYLES::CARRIER_AGGREGATION_CONTROL_REQUEST::VALUE:
                {
                  switch (indicationHeader.contents.format_2.InsertIndicationID)
                    {
                      case RIC_INSERT_SERVICE_STYLES::CARRIER_AGGREGATION_CONTROL_REQUEST::CA_SECONDARY_CELL_ADDITION_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      case RIC_INSERT_SERVICE_STYLES::CARRIER_AGGREGATION_CONTROL_REQUEST::CA_SECONDARY_CELL_MODIFICATION_AND_RELEASE_CONTROL_REQUEST::VALUE:
                        {

                        }
                      break;
                      default:
                        NS_ABORT_MSG("Unknown Radio Resource Allocation Control Request Indication ID");
                    }
                }
              break;
              case RIC_INSERT_SERVICE_STYLES::IDLE_MODE_MOBILITY_CONTROL_REQUEST::VALUE:
                    {
                        switch (indicationHeader.contents.format_2.InsertIndicationID)
                        {
                          case RIC_INSERT_SERVICE_STYLES::IDLE_MODE_MOBILITY_CONTROL_REQUEST::CELL_RESELECTION_PRIORITY_REQUEST::VALUE:
                            {

                            }
                            break;
                          default:
                            NS_ABORT_MSG("Unknown Radio Resource Allocation Control Request Indication ID");
                        }
                    }
                break;
              default:
                NS_ABORT_MSG("Unknown RIC Insert Style type");
            }
        }
      break;
      case RIC_INDICATION_HEADER_FORMAT_3:
        {
          //todo: INSERT Multiple Actions Control Request
        }
      break;
      default:
        NS_ABORT_MSG("Unknown RIC Indication Header Format");
    }

  enum KPM_INDICATION_FORMATS format = payload["MESSAGE"]["TYPE"];
  switch (format)
    {
      case KPM_INDICATION_FORMAT_1:
        {
          std::string ts = payload["COLLECTION START TIME"];
          std::string subscribed_endpoint = payload["MESSAGE"]["RAN FUNCTION"];
          std::string kpm = getSubEndpoint (src_endpoint, subscribed_endpoint); // Remove endpointRoot from full KPM endpoint
          std::vector<PeriodicMeasurementStruct> measurements = payload["MESSAGE"]["MEASUREMENTS"];
          if (measurements.size () > 0)
            {
              std::cout << to_string (measurements[0].measurements[0]) << std::endl;
            }
          auto kpmIt = m_kpmToEndpointStorage.find (kpm);
          if (kpmIt == m_kpmToEndpointStorage.end ())
            {
              m_kpmToEndpointStorage.emplace (
                  kpm,
                  std::map<std::string, std::deque<PeriodicMeasurementStruct>>{});
              kpmIt = m_kpmToEndpointStorage.find (kpm);
            }
          auto measuringE2NodeIt = kpmIt->second.find (src_endpoint);
          if (measuringE2NodeIt == kpmIt->second.end ())
            {
              kpmIt->second.emplace (src_endpoint, std::deque<PeriodicMeasurementStruct>{});
              measuringE2NodeIt = kpmIt->second.find (src_endpoint);
            }
          measuringE2NodeIt->second.push_front (PeriodicMeasurementStruct{ts, {}});
          //todo: notify endpoint (e.g. xapps) that fresh data is available
        }
      break;
      case KPM_INDICATION_FORMAT_2:
      case KPM_INDICATION_FORMAT_3:
      default:
        NS_ABORT_MSG ("Unsupported KPM indication format");
    }
}