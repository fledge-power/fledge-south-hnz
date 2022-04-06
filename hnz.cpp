//
// Created by Lucas Barret on 26/05/2021.
//

#include "include/hnz.h"
#include <reading.h>
#include <logger.h>
#include <config_category.h>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <chrono>
#include <sstream>
#include <ctime>

using namespace nlohmann;
using namespace std;
using namespace std::chrono;

json HNZ::m_stack_configuration;
json HNZ::m_msg_configuration;
json HNZ::m_pivot_configuration;

HNZ::HNZ(const char *ip, int port)
{
    if (strlen(ip) > 1)
        this->m_ip = ip;
    else
    {
        this->m_ip = "127.0.0.1";
    }

    if (port > 0)
    {
        m_port = port;
    }
    else
    {
        m_port = 6001;
    }
}

void HNZ::setJsonConfig(const std::string &stack_configuration, const std::string &msg_configuration, const std::string &pivot_configuration)
{
    Logger::getLogger()->info("Reading json config string...");

    try
    {
        m_stack_configuration = json::parse(stack_configuration)["protocol_stack"];
    }
    catch (json::parse_error &e)
    {
        Logger::getLogger()->fatal("Couldn't read protocol_stack json config string : " + string(e.what()));
    }

    try
    {
        m_msg_configuration = json::parse(msg_configuration)["exchanged_data"];
    }
    catch (json::parse_error &e)
    {
        Logger::getLogger()->fatal("Couldn't read exchanged_data json config string : " + string(e.what()));
    }

    try
    {
        m_pivot_configuration = json::parse(pivot_configuration)["protocol_translation"];
    }
    catch (json::parse_error &e)
    {
        Logger::getLogger()->fatal("Couldn't read protocol_translation json config string : " + string(e.what()));
    }

    Logger::getLogger()->info("Json config parsed successsfully.");
}

void HNZ::restart()
{
    stop();
    start();
}

/** Try to connect to server (m_retry_number try) */
int HNZ::connect()
{
    int i = 1;
    while ((i <= m_retry_number) or (m_retry_number == -1))
    {
        Logger::getLogger()->info("Connecting to server ... [" + to_string(i) + "/" + to_string(m_retry_number) + "]");
        m_connected = !(m_client->connect_Server(m_ip.c_str(), m_port));
        if (m_connected)
        {
            Logger::getLogger()->info("Connected.");
			frame_number = 0;
            return 0;
        }
        else
        {
            Logger::getLogger()->warn("Error in connection, retrying in " + to_string(m_retry_delay) + "s ...");
            high_resolution_clock::time_point beginning_time = high_resolution_clock::now();
            duration<double, std::milli> time_span = high_resolution_clock::now() - beginning_time;
            int time_out = m_retry_delay * 1000;

            while (time_span.count() < time_out)
            {
                time_span = high_resolution_clock::now() - beginning_time;
            }
        }
        i++;
    }
    return -1;
}

/** Starts the plugin */
void HNZ::start()
{
    m_fledge = new HNZFledge(this, &m_pivot_configuration);

    // Fledge logging level setting
    switch (m_getConfigValue<int>(m_stack_configuration, "/transport_layer/llevel"_json_pointer))
    {
    case 1:
        Logger::getLogger()->setMinLevel("debug");
        break;
    case 2:
        Logger::getLogger()->setMinLevel("info");
        break;
    case 3:
        Logger::getLogger()->setMinLevel("warning");
        break;
    default:
        Logger::getLogger()->setMinLevel("error");
        break;
    }

    Logger::getLogger()->info("Starting HNZ");

    loopActivated = true;
    m_ip = m_getConfigValue<string>(m_stack_configuration, "/transport_layer/connection/path/srv_ip"_json_pointer);
    m_port = m_getConfigValue<int>(m_stack_configuration, "/transport_layer/connection/path/port"_json_pointer);
    m_retry_number = m_getConfigValue<int>(m_stack_configuration, "/transport_layer/retry_number"_json_pointer);
    m_retry_delay = m_getConfigValue<int>(m_stack_configuration, "/transport_layer/retry_delay"_json_pointer);

    // Connect to the server
    m_client = new HNZClient();
    int conn_ok = !connect();

    if (conn_ok)
    {
        // Connected to the server, waiting for data
        loopThread = std::thread(&HNZ::receive, this);
    }
    else
    {
        Logger::getLogger()->fatal("Unable to connect to server, stopping ...");
        stop();
    }
}

void HNZ::receive()
{
    Logger::getLogger()->warn("Listening for data ...");

    while (loopActivated)
    {
        std::unique_lock<std::mutex> guard2(loopLock);

        MSG_TRAME *frReceived;

        // Waiting for data
        frReceived = (m_client->receiveFr());
        if (frReceived != nullptr)
        {
            Logger::getLogger()->warn("Data Received !");
            // Checking the CRC
            if (m_client->checkCRC(frReceived)) {
               Logger::getLogger()->info("CRC is good");
                unsigned char *data = frReceived->aubTrame;
                int size = frReceived->usLgBuffer;
				Logger::getLogger()->error(convert_data_to_str(data, size));
                analyze_frame(data, size);
            } else {
               Logger::getLogger()->warn("The CRC does not match");
            }
        }
        else
        {
            Logger::getLogger()->warn("No data available, checking connection ...");
            try
            {
                // Try to reconnect
                int conn_ok = !connect();
                if (not conn_ok)
                {
                    Logger::getLogger()->warn("Connection lost");
                    stop_loop();
                }
            }
            catch (...)
            {
                Logger::getLogger()->error("Error in connection, retrying ...");
            }
        }
        guard2.unlock();
        std::chrono::milliseconds timespan(1000);
        std::this_thread::sleep_for(timespan);
    }
}

void HNZ::analyze_frame(unsigned char *data, int size)
{
    // Get address byte
    unsigned char addr = data[0];
    // Recognition of the type of message
    unsigned char c = data[1];
    auto *myFr = new MSG_TRAME;

    Logger::getLogger()->info("Frame size : " + to_string(size));

    switch (c)
    {
    case UA:
        // Ignoring the UA ?
		Logger::getLogger()->info("UA reçu");
        break;
    case SARM:
    {
		frame_number = 0;
        module10M = 0;
        Logger::getLogger()->info("SARM reçu");
        // Sending UA
        unsigned char msg[1];
        msg[0] = UA; 
		//Logger::getLogger()->warn(to_string(sizeof(msg)));
        m_client->createAndSendFr(addr, msg, sizeof(msg));
        Logger::getLogger()->info("UA envoyé");
        // Envoi d'un SARM
        unsigned char addrA = addr+2;
        msg[0] = SARM;
        m_client->createAndSendFr(addrA, msg, sizeof(msg));
        Logger::getLogger()->info("SARM envoyé");
		Logger::getLogger()->warn("Procedure initialized.");
		
		unsigned char msg_date[5];
		time_t now = time(0);
		tm *time_struct = gmtime(&now);
		msg_date[0] = (frame_number % 8) * 0x20;
		msg_date[1] = 0x1c;
		msg_date[2] = time_struct->tm_mday;
		msg_date[3] = time_struct->tm_mon + 1;
		msg_date[4] = time_struct->tm_year -100;
		m_client->createAndSendFr(addrA, msg_date, sizeof(msg_date));
		Logger::getLogger()->warn("Mise a la date envoyée : "+to_string((int)msg_date[1])+"/"+to_string((int)msg_date[2])+"/"+to_string((int)msg_date[3]));
								 
		unsigned char msg_hour[5];
		long int ms_since_epoch, mod10m, millis;
		ms_since_epoch = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
		mod10m = (ms_since_epoch % 86400000) / 600000; //ms_since_beginning_of_day / ms_in_10min_interval
		millis = ms_since_epoch - (mod10m * 600000); //ms_since_beginning_of_10min_interval
		msg_hour[0] = (frame_number % 8) * 0x20 + 0x02;
		msg_hour[1] = 0x1d;
		msg_hour[2] = mod10m;
		msg_hour[3] = (millis / 10) >> 8;
		msg_hour[4] = (millis / 10) & 0xff;
		msg_hour[5] = 0x00;
		m_client->createAndSendFr(addrA, msg_hour, sizeof(msg_hour));
		Logger::getLogger()->warn("Mise a l'heure envoyée : "+to_string(msg_hour[1]*10/60)+":"+to_string((msg_hour[1]*10)%60)+","+to_string(millis)+"ms");

        unsigned char msg_dmd_cg[3];
		msg_dmd_cg[0] = (frame_number % 8) * 0x20 + 0x04;
        msg_dmd_cg[1] = 0x13;
		msg_dmd_cg[2] = 0x01;
		m_client->createAndSendFr(addrA, msg_dmd_cg, sizeof(msg_dmd_cg));
        Logger::getLogger()->warn("Demande de CG envoyé");
        
		break;
    }
    default:
        // Information frame
        // Get NR, P ans NS field
        int ns = (c >> 1) & 0x07;
        int pf = (c >> 4) & 0x01;
        int nr = (c >> 5) & 0x07;
        bool info = (c & 0x01) == 0;
        if (info) {
            // Trame d'info
            Logger::getLogger()->info("Trame info reçu : ns = " + to_string(ns) + ", p = " + to_string(pf) + ", nr = " + to_string(nr));
        } else {
            // Trame de supervision
            Logger::getLogger()->info("Trame de supervision : f = " + to_string(pf) + ", nr = " + to_string(nr));
        }

        int payloadSize = size - 4; // Remove address, type, CRC (2 bytes)
        
        if (analyze_info_frame(data+2, addr, ns, pf, nr, payloadSize)) {
            // Computing the frame number & sending RR
			unsigned char msg[1];
			if (!pf)
			{
            	frame_number++;
				msg[0] = 0x01 + (frame_number % 8) * 0x20;
				m_client->createAndSendFr(addr, msg, sizeof(msg));
				Logger::getLogger()->info("RR envoyé");
			}
			else
			{
				msg[0] = 0x01 + (frame_number % 8) * 0x20 + 0x10;
				m_client->createAndSendFr(addr, msg, sizeof(msg));
				Logger::getLogger()->info("Repetition, renvoi du RR");
			}            
        }
        break;
    }
}

std::string HNZ::convert_data_to_str(unsigned char *data, int len)
{
    std::string s = "";
    for (int i = 0; i < len; i++)
    {
        s += to_string(data[i]);
        if (i < len - 1) s += " ";
    }
    return s;
}

/* Analyze a frame of information and return a bool if frame is good */
bool HNZ::analyze_info_frame(unsigned char *data, unsigned char addr, int ns, int p, int nr, int payloadSize)
{
    int len = 0; // Length of message to push in Fledge
    confDatas confDatas;
    int value, quality, ts, ts_qual;
	long int scd_since_epoch, epoch_mod_day;

    unsigned char t = data[0]; // Payload type
    int info_address = 0;

    addr = (int) (addr >> 2); // 6 bits de poids fort = adresse
	
	bool nbrTM;

    // Analyzing the payload type
    switch (t)
    {
    case TM4:
        Logger::getLogger()->info("Received TM4");
        for (size_t i = 0; i < 4; i++)
        {
            // 4 TM inside a TM cyclique
            // Header
            info_address += stoi(to_string((int) data[1]) + to_string(i)); // ADTM + i
            confDatas = HNZ::m_checkExchangedDataLayer(addr, "02", info_address);

            // Item
            int noctet = 2 + i;
            value = (int) data[noctet]; // VALTMi
            quality = (value == 0xFF); // Invalide si VALTMi = 0xFF
            ts = 0;
            ts_qual = 0;

            sendToFledge(t, value, quality, ts, ts_qual, confDatas.label, confDatas.internal_id);
        }

        len = 6;
        break;
    case TSCE:
        Logger::getLogger()->info("Received TSCE");
        // Header
        info_address += stoi(to_string((int) data[1]) + to_string((int) (data[2] >> 5))); // AD0 + ADB
        //Logger::getLogger()->info("Info address = " + to_string(info_address) + " et addr = " + to_string(addr));
        confDatas = HNZ::m_checkExchangedDataLayer(addr, "0B", info_address);

        // Item
        value = (int) (data[2] >> 3) & 0x1; // E
        quality = (int) (data[2] >> 4) & 0x1; // V
		scd_since_epoch = duration_cast<seconds>(high_resolution_clock::now().time_since_epoch()).count();
		epoch_mod_day = scd_since_epoch - scd_since_epoch % 86400;
        ts = epoch_mod_day;
		ts += module10M * 10 * 60000;
        ts += (int) ((data[3] << 8) | data[4]) * 10;
        ts_qual = stoi(to_string((int) (data[2] >> 2) & 0x1) + to_string((int) (data[2] >> 1) & 0x1) + to_string((int) (data[2] & 0x1)));

        sendToFledge(t, value, quality, ts, ts_qual, confDatas.label, confDatas.internal_id);

        // Size of this message
        len = 5;
        break;
    case TSCG:
        Logger::getLogger()->info("Received TSCG");
        for (size_t i = 0; i < 16; i++)
        {
            // 16 TS inside a TSCG
            // Header
            info_address += stoi(to_string((int) data[1] + (int) i/8) + to_string(i % 8)); // AD0 + i%8  ou (AD0+1) + i%8
            confDatas = HNZ::m_checkExchangedDataLayer(addr, "16", info_address);

            // Item
            int noctet = 2 + (i / 4);
            value = (int) (data[noctet] >> (3 - (i % 4)) * 2) & 0x1; // E
            quality = (int) (data[noctet] >> (3 - (i % 4)) * 2) & 0x2; // V
            ts = 0;
            ts_qual = 0;

            sendToFledge(t, value, quality, ts, ts_qual, confDatas.label, confDatas.internal_id);
        }
        // Size of this message
        len = 6;
        break;
    case TMN:
		Logger::getLogger()->info("Received TMN");
		// 2 or 4 TM inside a TMn
		nbrTM = ((data[6] >> 7) == 1) ? 4 : 2;
		for (size_t i = 0; i < nbrTM; i++)
		{
			// 2 or 4 TM inside a TMn
			// Header
			info_address += stoi(to_string((int) data[1]) + to_string(i*4)); // ADTM + i*4
			confDatas = HNZ::m_checkExchangedDataLayer(addr, "0C", info_address);

			// Item
			if (nbrTM == 4) {
				int noctet = 2 + i;

				value = (int) (data[noctet]); // Vi
				quality = (int) (data[6] >> i) & 0x1; // Ii
			} else {
				int noctet = 2 + (i * 2);

				value = (int) (data[noctet + 1] << 8 || data[noctet]); // Concat V1/V2 and V3/V4
				quality = (int) (data[6] >> i*2) & 0x1; // I1 or I3
			}

			ts = 0;
			ts_qual = 0;

			sendToFledge(t, value, quality, ts, ts_qual, confDatas.label, confDatas.internal_id);
		}

		len = 7;
		break;
    case 0x13:
        Logger::getLogger()->info("Received CG request/BULLE");
        //confDatas = HNZ::m_checkExchangedDataLayer(addr,"13",0);
		confDatas = protocolDatas;
        len = 2;
        break;
    case 0x0F:
        module10M = (int) data[1];
        Logger::getLogger()->info("Received Modulo 10mn");
        //confDatas = HNZ::m_checkExchangedDataLayer(addr,"0F",0);
		confDatas = protocolDatas;
        len = 2;
        break;
    case 0x09:
        Logger::getLogger()->info("Received ATC, not implemented");
        //confDatas = HNZ::m_checkExchangedDataLayer(addr,"09",0);
		confDatas = protocolDatas;
        len = 3;
        break;
    default:
        Logger::getLogger()->info("Received an unknown type");
        break;
    }

    if (len != 0) {
        // Logging
        std::string content = convert_data_to_str(data, len);
        Logger::getLogger()->info("Data : [ " + content + " ]");

        // Check the length of the payload (There can be several messages in the same frame)
        if (len != payloadSize) {
            // Analyze the rest of the payload
            return analyze_info_frame(data+len, addr, ns, p, nr, payloadSize - len);
        }
        // We analyzed all the payload, the sizes correspond
        return true;
    } else {
        Logger::getLogger()->info("Message inconnu");
        // TODO : Envoyer un RR si le message est inconnu ?
        return false;
    }
}

void HNZ::sendToFledge(unsigned char t, int value, int quality, int ts, int ts_qual, std::string label, std::string internal_id) {
    if (label == "internal" && internal_id == "internal")
    {
        Logger::getLogger()->warn("Message protocolaire");
        return;
    }
    if (label != "" && internal_id != "")
    {
        // Prepare the value datapoint
        Datapoint* dp = m_fledge->m_addData<std::string>(value, quality, ts, ts_qual);
        // Send datapoint to fledge
        m_fledge->sendData(dp, to_string(t), internal_id, label);
    }
    else
    {
        Logger::getLogger()->warn("Message not found in exchanged msg config..");
    }
}

void HNZ::stop_loop()
{
    this->loopActivated = false;
    m_client->stop();
    loopThread.join();
    this->stop();
}

/** Disconnect from the HNZ server */
void HNZ::stop()
{
    delete m_client;
    m_client = nullptr;
}

/**
 * Called when a data changed event is received. This calls back to the south service
 * and adds the points to the readings queue to send.
 *
 * @param points    The points in the reading we must create
 */
void HNZ::ingest(Reading &reading)
{
    (*m_ingest)(m_data, reading);
}

/**
 * Save the callback function and its data
 * @param data   The Ingest function data
 * @param cb     The callback function to call
 */
void HNZ::registerIngest(void *data, INGEST_CB cb)
{
    m_ingest = cb;
    m_data = data;
}

void HNZFledge::sendData(Datapoint* dp, std::string code, std::string internal_id, const std::string& label)
{
    // Create the header
    auto* data_header = new vector<Datapoint*>;

    for (auto& feature : (*m_pivot_configuration)["mapping"]["data_object_header"].items())
    {
        if (feature.value() == "message_code")
            data_header->push_back(m_createDatapoint(feature.key(), code));
        else if (feature.value() == "internal_id")
            data_header->push_back(m_createDatapoint(feature.key(), internal_id));
    }

    DatapointValue header_dpv(data_header, true);

    auto* header_dp = new Datapoint("data_object_header", header_dpv);

    Datapoint* item_dp = dp;

    Reading reading(label, {header_dp, item_dp});
    m_hnz->ingest(reading);
}

template <class T>
Datapoint* HNZFledge::m_addData(int value, int quality, int ts, int ts_qual)
{
    auto* measure_features = new vector<Datapoint*>;

    for (auto& feature : (*m_pivot_configuration)["mapping"]["data_object_item"].items())
    {
        if (feature.value() == "value")
            measure_features->push_back(m_createDatapoint(feature.key(), (long int) value));
        else if (feature.value() == "quality")
            measure_features->push_back(m_createDatapoint(feature.key(), (long int) quality));
		else if (feature.value() == "timestamp")
            measure_features->push_back(m_createDatapoint(feature.key(), (long int) ts));
        else if (feature.value() == "ts_qual")
            measure_features->push_back(m_createDatapoint(feature.key(), (long int) ts_qual));
    }

    DatapointValue dpv(measure_features, true);

    auto* dp = new Datapoint("data_object_item", dpv);
    return dp;
}

template <class T>
T HNZ::m_getConfigValue(json configuration, json_pointer<json> path)
{
    T typed_value;

    try
    {
        typed_value = configuration.at(path);
    }
    catch (json::parse_error &e)
    {
        Logger::getLogger()->fatal("Couldn't parse value " + path.to_string() + " : " + e.what());
    }
    catch (json::out_of_range &e)
    {
        Logger::getLogger()->fatal("Couldn't reach value " + path.to_string() + " : " + e.what());
    }

    return typed_value;
}

HNZ::confDatas HNZ::m_checkExchangedDataLayer(const int address, const std::string& message_code, const int info_address)
{
    bool know_station_address = false, know_message_code = false, know_info_address = false;
    confDatas data;
	
	//Logger::getLogger()->warn("Checking " + to_string(address) + " " + message_code + " " + to_string(info_address));
	for (auto& element : m_msg_configuration["msg_list"])
	{
		if (m_getConfigValue<unsigned int>(element, "/station_address"_json_pointer) == address)
		{
			//Logger::getLogger()->warn("ADDR:" + to_string(m_getConfigValue<unsigned int>(element, "/station_address"_json_pointer)));
			know_station_address = true;
			if (m_getConfigValue<string>(element, "/message_code"_json_pointer) == message_code)
			{
				//Logger::getLogger()->warn("MSGCODE:" + m_getConfigValue<string>(element, "/message_code"_json_pointer));
				know_message_code = true;
				if (m_getConfigValue<unsigned int>(element, "/info_address"_json_pointer) == info_address)
				{
					know_info_address = true;
					//Logger::getLogger()->warn("INFOADDR:"+ to_string(m_getConfigValue<unsigned int>(element, "/info_address"_json_pointer)));
                    data.label = element["label"];
                    data.internal_id = element["internal_id"];
					//Logger::getLogger()->warn("Found : " + data.label + " " + data.internal_id);
					return data;
				}
			}
		}
	}
	//Logger::getLogger()->warn("Legacy : " + data.label + " " + data.internal_id);
	
	if (!know_station_address)
		Logger::getLogger()->warn("Unknown Station Address (" + to_string(address) +")");
	else if (!know_message_code)
		Logger::getLogger()->warn("Unknown Message Code (" + message_code + ")");
	else if (!know_info_address)
		Logger::getLogger()->warn("Unknown Info Address (" + to_string(info_address) +")");
	else
		Logger::getLogger()->warn("Error while checking data layer config");

	data.label = "";
    data.internal_id = "";
    return data;
}