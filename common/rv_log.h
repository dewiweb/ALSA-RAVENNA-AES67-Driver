// rv_log.h  --  Ravenna Project

#ifndef RV_LOG_H
#define RV_LOG_H

#include <stdio.h>
#include <string.h>

#ifdef WIN32
	#ifdef __cplusplus
	extern "C" {
	#endif
		extern int g_bANSI_Enabled;
		void rv_log_add_console();
	#ifdef __cplusplus
	}	
	#endif
#endif

// to make the preprocessor directives as __LINE__ interpreted
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#ifdef WIN32
#define __FILE_NAME__ (strrchr(__FILE__, '\\') ? (strrchr(__FILE__, '\\') + 1) : __FILE__)
#else
#define __FILE_NAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1) : __FILE__)
#endif
//#define AT "In " __FILE__ ", line " TOSTRING(__LINE__) ": "


/*#ifdef __linux__
#include <syslog.h>

#define rv_log(log_level, ...) \
	{syslog(log_level, AT __VA_ARGS__);}
#define rv_log2(log_level, ...) \
		{;}
//    {syslog(log_level, __VA_ARGS__);}

#else // not __linux__*/

#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */
#define LOG_TRACE		8


#define RV_LOG_BLACK(x)		{printf("\033[30m\033[1m");	printf x; printf("\033[0m");}
#define RV_LOG_RED(x)		{printf("\033[31m\033[1m");	printf x; printf("\033[0m");}
#define RV_LOG_GREEN(x)		{printf("\033[32m\033[1m");	printf x; printf("\033[0m");}
#define RV_LOG_YELLOW(x)	{printf("\033[33m\033[1m");	printf x; printf("\033[0m");}
#define RV_LOG_BLUE(x)		{printf("\033[34m\033[1m");	printf x; printf("\033[0m");}
#define RV_LOG_MAGENTA(x)	{printf("\033[35m\033[1m");	printf x; printf("\033[0m");}
#define RV_LOG_CYAN(x)		{printf("\033[36m\033[1m");	printf x; printf("\033[0m");}
#define RV_LOG_WHITE(x)		{printf("\033[37m\033[1m");	printf x; printf("\033[0m");}

// LOG4C emulation
#define LOG4C_PRIORITY_FATAL	LOG_EMERG	/** fatal */	
#define LOG4C_PRIORITY_ALERT	LOG_ALERT	/** alert */	
#define LOG4C_PRIORITY_CRIT		LOG_CRIT	/** crit */	    
#define LOG4C_PRIORITY_ERROR	LOG_ERR		/** error */	
#define LOG4C_PRIORITY_WARN		LOG_WARNING	/** warn */	    
#define LOG4C_PRIORITY_NOTICE	LOG_NOTICE	/** notice */	
#define LOG4C_PRIORITY_INFO		LOG_INFO	/** info */	    
#define LOG4C_PRIORITY_DEBUG	LOG_DEBUG	/** debug */	
#define LOG4C_PRIORITY_TRACE	LOG_TRACE	/** trace */
#define LOG4C_PRIORITY_NOTSET	9			/** notset */	
#define LOG4C_PRIORITY_UNKNOWN	10			/** unknown */

#define LOG_MAX_LEVEL LOG_DEBUG

//syslog(log_level, AT __VA_ARGS__);

#if UNDER_RTSS
#include "MTAL_DP.h"
#define rv_log(log_level, ...) { if(log_level <= LOG_MAX_LEVEL) { MTAL_DP("In %s, line %i: ", __FILE_NAME__, __LINE__); MTAL_DP(__VA_ARGS__); }}

#define log4c_log(device_category, log_level, ...) { if(log_level <= LOG_MAX_LEVEL) { MTAL_DP("In %s, line %i: ", __FILE_NAME__, __LINE__);  MTAL_DP(__VA_ARGS__); MTAL_DP("\r\n"); }}
#elif WIN32	
	//#define rv_log(log_level, ...) { if(log_level <= LOG_MAX_LEVEL) { printf("In %s, line %i: ", __FILE_NAME__, __LINE__); printf(__VA_ARGS__); /*printf("\r\n");*/ fflush(stdout); }}
	//#define log4c_log(device_category, log_level, ...) { if(log_level <= LOG_MAX_LEVEL) { printf("In %s, line %i: ", __FILE_NAME__, __LINE__);  printf(__VA_ARGS__); printf("\r\n"); fflush(stdout); }}
#define rv_log(log_level, ...)  \
		{\
		if(log_level <= LOG_MAX_LEVEL)\
			{\
			printf("In %s, line %i: ", __FILE_NAME__, __LINE__); \
			if(g_bANSI_Enabled == 0)\
				printf(__VA_ARGS__);\
			else if(log_level < LOG_WARNING) \
				RV_LOG_RED((__VA_ARGS__)) \
			else if(log_level == LOG_WARNING) \
				RV_LOG_YELLOW((__VA_ARGS__)) \
			else if(log_level == LOG_NOTICE) \
				RV_LOG_CYAN((__VA_ARGS__)) \
			else if(log_level == LOG_INFO) \
				RV_LOG_GREEN((__VA_ARGS__)) \
			else if(log_level == LOG_DEBUG) \
				RV_LOG_MAGENTA((__VA_ARGS__)) \
			else \
				printf(__VA_ARGS__);\
			/*printf("\r\n");*/ fflush(stdout);\
			}\
				}

#define log4c_log(device_category, log_level, ...)\
		{\
			if(log_level <= LOG_MAX_LEVEL) {\
				if(log_level <= LOG_MAX_LEVEL)\
				{\
					printf("In %s, line %i: ", __FILE_NAME__, __LINE__); \
					if(log_level < LOG_WARNING) \
						RV_LOG_RED((__VA_ARGS__)) \
					else if(log_level == LOG_WARNING) \
						RV_LOG_YELLOW((__VA_ARGS__)) \
					else if(log_level == LOG_NOTICE) \
						RV_LOG_CYAN((__VA_ARGS__)) \
					else if(log_level == LOG_INFO) \
						RV_LOG_GREEN((__VA_ARGS__)) \
					else if(log_level == LOG_DEBUG) \
						RV_LOG_MAGENTA((__VA_ARGS__)) \
					else \
						printf(__VA_ARGS__);\
					printf("\r\n"); fflush(stdout);\
				 }\
			}\
		}


#elif OSX
//#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    #include <syslog.h>
    // Note: we are minimize log_level before calling syslog because there is only 3 bits for priority
    #define rv_log(log_level, ...)  \
    {\
        int log_level_tmp = log_level > 7 ? 7 : log_level;\
        if(log_level_tmp <= LOG_MAX_LEVEL)\
        {\
            char cBuffer[512]; \
            snprintf(cBuffer, 512, "In %s, line %i: ", __FILE_NAME__, __LINE__);\
            snprintf(cBuffer + strlen(cBuffer), 512 - strlen(cBuffer), __VA_ARGS__);\
            syslog(log_level, "%s", cBuffer);\
        }\
    }
    /*    if(log_level <= LOG_MAX_LEVEL) {\
            printf("In %s, line %i: ", __FILE_NAME__, __LINE__); \
            printf(__VA_ARGS__);\
        }\
    }*/
    #define log4c_log(device_category, log_level, ...)\
    {\
        int log_level_tmp = log_level > 7 ? 7 : log_level;\
        if(log_level_tmp <= LOG_ERR) {\
            char cBuffer[512];\
            snprintf(cBuffer, 512, "[%s] In %s, line %i: ", TOSTRING(device_category), __FILE_NAME__, __LINE__);\
            snprintf(cBuffer + strlen(cBuffer), 512 - strlen(cBuffer), __VA_ARGS__);\
            syslog(log_level, "%s", cBuffer);\
        }\
    }
    /*        \
            printf("In %s, line %i: ", __FILE_NAME__, __LINE__); \
            printf(__VA_ARGS__);\
            printf("\r\n"); fflush(stdout);\
        }\
    }*/
//#pragma clang diagnostic pop
#else
	#include <syslog.h>
	#define rv_log(log_level, ...)  \
		{\
		char cBuffer[512];\
		snprintf(cBuffer, 512, "In %s, line %i: ", __FILE_NAME__, __LINE__);\
		snprintf(cBuffer + strlen(cBuffer), 512 - strlen(cBuffer), __VA_ARGS__);\
		syslog(log_level, "%s", cBuffer);\
		if(log_level <= LOG_MAX_LEVEL)\
			{\
			printf("In %s, line %i: ", __FILE_NAME__, __LINE__); \
			if(log_level < LOG_WARNING) \
				RV_LOG_RED((__VA_ARGS__)) \
			else if(log_level == LOG_WARNING) \
				RV_LOG_YELLOW((__VA_ARGS__)) \
			else if(log_level == LOG_NOTICE) \
				RV_LOG_CYAN((__VA_ARGS__)) \
			else if(log_level == LOG_INFO) \
				RV_LOG_GREEN((__VA_ARGS__)) \
			else if(log_level == LOG_DEBUG) \
				RV_LOG_MAGENTA((__VA_ARGS__)) \
			else \
				printf(__VA_ARGS__);\
			/*printf("\r\n");*/ fflush(stdout);\
			}\
				}

	#define log4c_log(device_category, log_level, ...)\
		{\
			if(log_level <= LOG_MAX_LEVEL) {\
				char cBuffer[512];\
				snprintf(cBuffer, 512, "[%s] In %s, line %i: ", TOSTRING(device_category), __FILE_NAME__, __LINE__);\
				snprintf(cBuffer + strlen(cBuffer), 512 - strlen(cBuffer), __VA_ARGS__);\
				syslog(log_level, "%s", cBuffer);\
				if(log_level <= LOG_MAX_LEVEL)\
				{\
					printf("In %s, line %i: ", __FILE_NAME__, __LINE__); \
					if(log_level < LOG_WARNING) \
						RV_LOG_RED((__VA_ARGS__)) \
					else if(log_level == LOG_WARNING) \
						RV_LOG_YELLOW((__VA_ARGS__)) \
					else if(log_level == LOG_NOTICE) \
						RV_LOG_CYAN((__VA_ARGS__)) \
					else if(log_level == LOG_INFO) \
						RV_LOG_GREEN((__VA_ARGS__)) \
					else if(log_level == LOG_DEBUG) \
						RV_LOG_MAGENTA((__VA_ARGS__)) \
					else \
						printf(__VA_ARGS__);\
					printf("\r\n"); fflush(stdout);\
				 }\
			}\
		}

#endif // WIN32

#define rv_log2(log_level, ...) \
    { printf(__VA_ARGS__); printf("\r\n"); fflush(stdout); }


#endif
