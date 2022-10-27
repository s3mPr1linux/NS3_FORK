//
// Created by Gabriel Ferreira (@gabrielcarvfer) on 27/10/22.
//

#include "ORAN-message-types.h"

namespace ns3
{

std::map<ORAN_MESSAGE_TYPES, std::string> oran_msg_str
    {
        {RIC_SUBSCRIPTION_REQUEST,                  "RIC_SUBSCRIPTION_REQUEST"},
        {RIC_SUBSCRIPTION_DELETE_REQUEST,           "RIC_SUBSCRIPTION_DELETE_REQUEST"},
        {RIC_CONTROL_REQUEST,                       "RIC_CONTROL_REQUEST"},
        {E2_SETUP_RESPONSE,                         "E2_SETUP_RESPONSE"},
        {E2_SETUP_FAILURE,                          "E2_SETUP_FAILURE"},
        {RIC_SERVICE_QUERY,                         "RIC_SERVICE_QUERY"},
        {RIC_SERVICE_UPDATE_ACKNOWLEDGE,            "RIC_SERVICE_UPDATE_ACKNOWLEDGE"},
        {RIC_SERVICE_UPDATE_FAILURE,                "RIC_SERVICE_UPDATE_FAILURE"},
        {E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE,  "E2_NODE_CONFIGURATION_UPDATE_ACKNOWLEDGE"},
        {E2_NODE_CONFIGURATION_UPDATE_FAILURE,      "E2_NODE_CONFIGURATION_UPDATE_FAILURE"},
        {E2_CONNECTION_UPDATE_ACKNOWLEDGE,          "E2_CONNECTION_UPDATE_ACKNOWLEDGE"},
        {E2_CONNECTION_UPDATE_FAILURE,              "E2_CONNECTION_UPDATE_FAILURE"},
        {RIC_SUBSCRIPTION_RESPONSE,                 "RIC_SUBSCRIPTION_RESPONSE"},
        {RIC_SUBSCRIPTION_FAILURE,                  "RIC_SUBSCRIPTION_FAILURE"},
        {RIC_SUBSCRIPTION_DELETE_RESPONSE,          "RIC_SUBSCRIPTION_DELETE_RESPONSE"},
        {RIC_SUBSCRIPTION_DELETE_FAILURE,           "RIC_SUBSCRIPTION_DELETE_FAILURE"},
        {RIC_SUBSCRIPTION_DELETE_REQUIRED,          "RIC_SUBSCRIPTION_DELETE_REQUIRED"},
        {RIC_INDICATION,                            "RIC_INDICATION"},
        {RIC_CONTROL_ACKNOWLEDGE,                   "RIC_CONTROL_ACKNOWLEDGE"},
        {RIC_CONTROL_FAILURE,                       "RIC_CONTROL_FAILURE"},
        {E2_SETUP_REQUEST,                          "E2_SETUP_REQUEST"},
        {RIC_SERVICE_UPDATE,                        "RIC_SERVICE_UPDATE"},
        {E2_NODE_CONFIGURATION_UPDATE,              "E2_NODE_CONFIGURATION_UPDATE"},
        {E2_CONNECTION_UPDATE,                      "E2_CONNECTION_UPDATE"},
        {RESET_REQUEST,                             "RESET_REQUEST"},
        {RESET_RESPONSE,                            "RESET_RESPONSE"},
        {ERROR_INDICATION,                          "ERROR_INDICATION"},
        {E2_REMOVAL_REQUEST,                        "E2_REMOVAL_REQUEST"},
        {E2_REMOVAL_RESPONSE,                       "E2_REMOVAL_RESPONSE"},
        {E2_REMOVAL_FAILURE,                        "E2_REMOVAL_FAILURE"},
    };
}