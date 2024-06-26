/*
 * Fledge HNZ south plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Justin Facquet
 * 
 */

#ifndef _HNZ_UTILITY_H
#define _HNZ_UTILITY_H

#include <string>
#include <logger.h>
#include <audit_logger.h>

#define PLUGIN_NAME "hnz"

namespace HnzUtility {
    
    static const std::string NamePlugin = PLUGIN_NAME;

    /*
     * Log helper function that will log both in the Fledge syslog file and in stdout for unit tests
     */
    template<class... Args>
    void log_debug(const std::string& format, Args&&... args) {  
        #ifdef UNIT_TEST
        printf(std::string(format).append("\n").c_str(), std::forward<Args>(args)...);
        fflush(stdout);
        #endif
        Logger::getLogger()->debug(format.c_str(), std::forward<Args>(args)...);
    }

    template<class... Args>
    void log_info(const std::string& format, Args&&... args) {    
        #ifdef UNIT_TEST
        printf(std::string(format).append("\n").c_str(), std::forward<Args>(args)...);
        fflush(stdout);
        #endif
        Logger::getLogger()->info(format.c_str(), std::forward<Args>(args)...);
    }

    template<class... Args>
    void log_warn(const std::string& format, Args&&... args) { 
        #ifdef UNIT_TEST  
        printf(std::string(format).append("\n").c_str(), std::forward<Args>(args)...);
        fflush(stdout);
        #endif
        Logger::getLogger()->warn(format.c_str(), std::forward<Args>(args)...);
    }

    template<class... Args>
    void log_error(const std::string& format, Args&&... args) {   
        #ifdef UNIT_TEST
        printf(std::string(format).append("\n").c_str(), std::forward<Args>(args)...);
        fflush(stdout);
        #endif
        Logger::getLogger()->error(format.c_str(), std::forward<Args>(args)...);
    }

    template<class... Args>
    void log_fatal(const std::string& format, Args&&... args) {  
        #ifdef UNIT_TEST
        printf(std::string(format).append("\n").c_str(), std::forward<Args>(args)...);
        fflush(stdout);
        #endif
        Logger::getLogger()->fatal(format.c_str(), std::forward<Args>(args)...);
    }

    inline void audit_fail(const std::string& code, const std::string& data) {  
        AuditLogger::getLogger()->auditLog(code.c_str(), "FAILURE", data.c_str());
    }

    inline void audit_success(const std::string& code, const std::string& data) {  
        AuditLogger::getLogger()->auditLog(code.c_str(), "SUCCESS", data.c_str());
    }

    inline void audit_warn(const std::string& code, const std::string& data) {  
        AuditLogger::getLogger()->auditLog(code.c_str(), "WARNING", data.c_str());
    }

    inline void audit_info(const std::string& code, const std::string& data) {  
        AuditLogger::getLogger()->auditLog(code.c_str(), "INFORMATION", data.c_str());
    }
}

#endif /* _HNZ_UTILITY_H */