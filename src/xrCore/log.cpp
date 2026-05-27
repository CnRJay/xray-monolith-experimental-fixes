#include "stdafx.h"
#pragma hdrstop

#include <time.h>
#include "resource.h"
#include "log.h"
#ifdef _EDITOR
#include "malloc.h"
#endif

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <tbb/concurrent_queue.h>

#include "profiler.h"

extern BOOL LogExecCB = TRUE;
static string_path logFName = "engine.log";
static string_path log_file_name = "engine.log";
static BOOL no_log = TRUE;
#ifdef PROFILE_CRITICAL_SECTIONS
static xrCriticalSection logCS(MUTEX_PROFILE_ID(log));
#else // PROFILE_CRITICAL_SECTIONS
static xrCriticalSection logCS;
#endif // PROFILE_CRITICAL_SECTIONS
xr_vector<xr_string> LogFile;
static LogCallback LogCB = 0;

// Async Logging Globals
static tbb::concurrent_queue<std::string> logQueue;
static std::atomic<bool> m_bLogThreadActive(false);
static std::atomic<bool> m_bLogFileReady(false);
static std::thread logWorkerThread;
static std::mutex logFileMtx; // Protects file access between Worker and FlushLog
static FILE* pLogFile = nullptr;

static void ProcessLogMessage(const std::string& msg)
{
    // Duplicate folding logic
    static shared_str last_str;
    static int items_count = 0;

    auto temp = shared_str(msg.c_str());

    // Lock LogFile access
    logCS.Enter();

    if (last_str.equal(temp))
    {
        xr_string tmp = temp.c_str();

        if (items_count == 0)
            items_count = 2;
        else
            items_count++;

        tmp += " [";
        tmp += std::to_string(items_count).c_str();
        tmp += "]";

        if (!LogFile.empty())
             LogFile.erase(LogFile.end()-1);
        LogFile.push_back(xr_string(tmp.c_str()));
    }
    else
    {
        LogFile.push_back(xr_string(temp.c_str()));
        last_str = temp;
        items_count = 0;
    }
    
    logCS.Leave();

    // Write to file
    if (pLogFile) {
        fprintf(pLogFile, "%s\n", msg.c_str());
    }
}

void LogThreadLoop()
{
    std::vector<std::string> localBuffer;
    localBuffer.reserve(1024);

    while (m_bLogThreadActive)
    {
        if (!m_bLogFileReady) {
             std::this_thread::sleep_for(std::chrono::milliseconds(10));
             continue;
        }

        // Pop messages into local buffer
        localBuffer.clear();
        
        std::string msg;
        int count = 0;
        // Limit batch size to keep responsiveness and avoid holding file lock too long
        while (count < 1000 && logQueue.try_pop(msg)) {
            localBuffer.push_back(std::move(msg));
            count++;
        }

        if (localBuffer.empty()) {
             std::this_thread::sleep_for(std::chrono::milliseconds(10));
             continue;
        }

        // Process batch
        {
            std::lock_guard<std::mutex> fileLock(logFileMtx);
            if (!pLogFile) {
                pLogFile = fopen(logFName, "a");
            }
            
            if (pLogFile) {
                for (const auto& m : localBuffer) {
                    ProcessLogMessage(m);
                }
                fflush(pLogFile);
            }
        }
    }
    
    // Cleanup on exit
    std::lock_guard<std::mutex> fileLock(logFileMtx);
    if (pLogFile) {
        fclose(pLogFile);
        pLogFile = nullptr;
    }
}

void FlushLog()
{
	PROF_EVENT();

	if (!no_log)
	{
		PROF_EVENT("Flushing");
        
        // Manual drain for crash safety
        std::vector<std::string> localBuffer;
        std::string msg;
        while (logQueue.try_pop(msg)) {
            localBuffer.push_back(std::move(msg));
        }

        std::lock_guard<std::mutex> fileLock(logFileMtx);
        // Ensure file is open
        if (!pLogFile && m_bLogFileReady) {
             pLogFile = fopen(logFName, "a");
        }
        
        if (pLogFile) {
             // Write pending
             for (const auto& m : localBuffer) {
                 ProcessLogMessage(m);
             }
             fflush(pLogFile);
        }
	}
}

std::string getCurrentTimeStamp(LPCSTR format = "%d.%m.%Y %H:%M:%S") {
	using namespace std::chrono;

	// get current time
	auto now = system_clock::now();

	// get number of milliseconds for the current second
	// (remainder after division into seconds)
	auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

	// convert to std::time_t in order to convert to std::tm (broken time)
	auto timer = system_clock::to_time_t(now);

	// convert to broken time
	std::tm bt = *std::localtime(&timer);

	std::ostringstream oss;

	oss << std::put_time(&bt, format); // HH:MM:SS
	oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

	return oss.str();
}

std::string timeInDMYHMSMMM()
{
	return getCurrentTimeStamp("%d.%m.%Y %H:%M:%S");
}

std::string timeInHMSMMM()
{
	return getCurrentTimeStamp("%H:%M:%S");
}

BOOL logTimestamps = FALSE;
enum Console_mark;
extern bool is_console_mark(Console_mark type);

void AddOne(const char* split)
{
#ifdef DEBUG
    OutputDebugString(split);
    OutputDebugString("\n");
#endif

	// DUMP_PHASE;
	{
		// demonized: add timestamps to log
		std::string t = split;
		if (logTimestamps) {
			std::string c = "";
			if (t.length() > 0 && is_console_mark((Console_mark)t[0])) {
				c += t[0];
				c += " ";
				t.erase(0, 1);
			}
			t = c + "[" + timeInHMSMMM() + "] " + t;
		}
		auto temp = shared_str(t.c_str());
		static shared_str last_str;
		static int items_count;

		if (last_str.equal(temp))
		{
			xr_string tmp = temp.c_str();

			if (items_count == 0)
				items_count = 2;
			else
				items_count++;

			tmp += " [";
			tmp += std::to_string(items_count).c_str();
			tmp += "]";

        // Push to Async Queue
        {
            logQueue.push(std::move(t));
        }
        // logQueueCV.notify_one(); // Removed as we use polling in worker
	}

	//exec CallBack
	if (LogExecCB && LogCB) {
        logCS.Enter();
        LogCB(split);
        logCS.Leave();
    }
}

void Log(const char* s)
{
	int i, j;

	u32 length = xr_strlen(s);
#ifndef _EDITOR
	PSTR split = (PSTR)_alloca((length + 1) * sizeof(char));
#else
    PSTR split = (PSTR)alloca((length + 1) * sizeof(char));
#endif
	for (i = 0, j = 0; s[i] != 0; i++)
	{
		if (s[i] == '\n')
		{
			split[j] = 0; // end of line
			if (split[0] == 0)
			{
				split[0] = ' ';
				split[1] = 0;
			}
			AddOne(split);
			j = 0;
		}
		else
		{
			split[j++] = s[i];
		}
	}
	split[j] = 0;
	AddOne(split);
}

void __cdecl Msg(const char* format, ...)
{
	va_list mark;
	string2048 buf;
	va_start(mark, format);
	int sz = _vsnprintf(buf, sizeof(buf) - 1, format, mark);
	buf[sizeof(buf) - 1] = 0;
	va_end(mark);
	if (sz) Log(buf);
}

void Log(const char* msg, const char* dop)
{
	if (!dop)
	{
		Log(msg);
		return;
	}

	u32 buffer_size = (xr_strlen(msg) + 1 + xr_strlen(dop) + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);
	strconcat(buffer_size, buf, msg, " ", dop);
	Log(buf);
}

void Log(const char* msg, u32 dop)
{
	u32 buffer_size = (xr_strlen(msg) + 1 + 10 + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s %d", msg, dop);
	Log(buf);
}

void Log(const char* msg, int dop)
{
	u32 buffer_size = (xr_strlen(msg) + 1 + 11 + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s %i", msg, dop);
	Log(buf);
}

void Log(const char* msg, float dop)
{
	// actually, float string representation should be no more, than 40 characters,
	// but we will count with slight overhead
	u32 buffer_size = (xr_strlen(msg) + 1 + 64 + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s %f", msg, dop);
	Log(buf);
}

void Log(const char* msg, const Fvector& dop)
{
	u32 buffer_size = (xr_strlen(msg) + 2 + 3 * (64 + 1) + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s (%f,%f,%f)", msg, VPUSH(dop));
	Log(buf);
}

void Log(const char* msg, const Fmatrix& dop)
{
	u32 buffer_size = (xr_strlen(msg) + 2 + 4 * (4 * (64 + 1) + 1) + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s:\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n",
	           msg,
	           dop.i.x, dop.i.y, dop.i.z, dop._14_,
	           dop.j.x, dop.j.y, dop.j.z, dop._24_,
	           dop.k.x, dop.k.y, dop.k.z, dop._34_,
	           dop.c.x, dop.c.y, dop.c.z, dop._44_
	);
	Log(buf);
}

void LogWinErr(const char* msg, long err_code)
{
	Msg("%s: %s", msg, Debug.error2string(err_code));
}

LogCallback SetLogCB(LogCallback cb)
{
	LogCallback result = LogCB;
	LogCB = cb;
	return (result);
}

LPCSTR log_name()
{
	return (log_file_name);
}

void InitLog()
{
	LogFile.reserve(10000);

    // Start Worker Thread
    m_bLogThreadActive = true;
    logWorkerThread = std::thread(LogThreadLoop);
}

void CreateLog(BOOL nl)
{
	no_log = nl;
	strconcat(sizeof(log_file_name), log_file_name, Core.ApplicationName, "_", Core.UserName, ".log");
	if (FS.path_exist("$logs$"))
		FS.update_path(logFName, "$logs$", log_file_name);
	if (!no_log)
	{
		//Alun: Backup existing log
		xr_string backup_logFName = EFS.ChangeFileExt(logFName, ".bkp");
		FS.file_rename(logFName, backup_logFName.c_str(), true);
		//-Alun
		IWriter* f = FS.w_open(logFName);
		if (f == nullptr)
		{
			MessageBox(NULL, "Can't create log file.", "Error", MB_ICONERROR);
			abort();
		}
		FS.w_close(f);

        // Signal File Ready
        m_bLogFileReady = true;
	}
}

void CloseLog(void)
{
	FlushLog();

    // Stop Worker Thread
    m_bLogThreadActive = false;
    if (logWorkerThread.joinable()) {
        logWorkerThread.join();
    }

	LogFile.clear();
}

xr_string FormatString(LPCSTR fmt, ...)
{
	va_list mark;
	string2048 buf;
	va_start(mark, fmt);
	int sz = _vsnprintf(buf, sizeof(buf) - 1, fmt, mark);
	buf[sizeof(buf) - 1] = 0;
	va_end(mark);
	if (sz) return xr_string(buf);
	return xr_string("");
}
