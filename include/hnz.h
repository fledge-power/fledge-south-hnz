//
// Created by Lucas Barret on 26/05/2021.
//

#ifndef WORK_HNZ_H
#define WORK_HNZ_H

#include <iostream>
#include <cstring>
#include <reading.h>
#include <logger.h>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <mutex>
#include <atomic>
#include <utility>
//#include "../../lib/src/inc/hnz_client.h"
#include "../../libhnz/src/inc/hnz_client.h"
#include <json.hpp> // https://github.com/nlohmann/json
#include "hnzconf.h"

class HNZFledge;

class HNZ
{
public:
    typedef void (*INGEST_CB)(void *, Reading);

    HNZ();
    ~HNZ() = default;

    struct confDatas {
        std::string label, internal_id;
    };
    
    typedef struct confDatas confDatas;

    void		setAssetName(const std::string& asset) { m_asset = asset; }
    void		restart();
    void        start();
    void		stop();
    void        receive();
    int		    connect();
    void        stop_loop();
    void        analyze_frame(unsigned char* data, int size);
    bool        analyze_info_frame(unsigned char *data, unsigned char addr, int ns, int p, int nr, int size);
    void        sendToFledge(unsigned char t, int value, int quality, int ts, int ts_qual, std::string label, std::string internal_id);
    std::string convert_data_to_str(unsigned char *data, int len);

	
	confDatas protocolDatas = {"na", "na"};

    confDatas m_checkExchangedDataLayer(const int address, const std::string& message_code, const int info_address);

    void		ingest(Reading& reading);
    void		registerIngest(void *data, void (*cb)(void *, Reading));

    std::string		m_asset;
    
    HNZClient*      m_client;

    std::mutex loopLock;
    std::atomic<bool> loopActivated;
    std::thread loopThread;
	
	static void setJsonConfig(const std::string& configuration, const std::string& msg_configuration, const std::string& pivot_configuration);

private:
    // configuration
    static HNZConf *m_conf;

    INGEST_CB			m_ingest;     // Callback function used to send data to south service
    void*               m_data;       // Ingest function data
    bool				m_connected;
    HNZFledge*          m_fledge;
	int					frame_number, module10M;
	
	template <class T>
    static T m_getConfigValue(nlohmann::json configuration, nlohmann::json_pointer<nlohmann::json> path);
	static nlohmann::json m_stack_configuration;
    static nlohmann::json m_msg_configuration;
    static nlohmann::json m_pivot_configuration;
};

class HNZFledge
{
public :
    explicit HNZFledge(HNZ *hnz, nlohmann::json* pivot_configuration) : m_hnz(hnz), m_pivot_configuration(pivot_configuration) {};

    // ==================================================================== //
    // Note : The overloaded method addData is used to prevent the user from
    // giving value type that can't be handled. The real work is forwarded
    // to the private method m_addData


    // Sends the datapoints passed as Reading to Fledge
    void sendData(Datapoint* dp, std::string code, std::string internal_id, const std::string& label);

    template<class T>
    Datapoint* m_addData(int value, int quality, int ts, int ts_qual);

private:

    template<class T>
    static Datapoint* m_createDatapoint(const std::string& dataname, const T value)
    {
        DatapointValue dp_value = DatapointValue(value);
        return new Datapoint(dataname,dp_value);
    }

    HNZ* m_hnz;
	nlohmann::json* m_pivot_configuration;
	
};


#endif //WORK_HNZ_H
