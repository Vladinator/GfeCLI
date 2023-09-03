//#define USE_PROMISES

#pragma warning(disable: 4244)

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <codecvt>
#include <future>
#include <algorithm>
#include <cctype>

#include <Shlwapi.h>
#include <tlhelp32.h>
#include <Psapi.h>

#include "GfeSDKWrapper.hpp"

GfeSdkWrapper gfeSdkWrapper;

std::string wcharToString(wchar_t* text)
{
	std::string str;
	for (size_t i = 0; i < wcslen(text); ++i)
	{
		wchar_t wchr = text[i];
		char nchr = static_cast<char>(wchr);
		str.push_back(nchr);
	}
	return str;
}

std::string msTimeToString(int milliseconds)
{
	if (milliseconds < 0)
	{
		milliseconds = -milliseconds;
	}
	int seconds = milliseconds / 1000;
	int minutes = seconds / 60;
	int hours = minutes / 60;
	seconds %= 60;
	minutes %= 60;
	std::ostringstream formattedTime;
	formattedTime << std::setfill('0') << std::setw(2) << hours << ":"
		<< std::setfill('0') << std::setw(2) << minutes << ":"
		<< std::setfill('0') << std::setw(2) << seconds;
	return formattedTime.str();
}

constexpr auto MAX_HIGHLIGHT_DURATION = 1200;

class Arguments
{

public:

	bool valid = false;
	int processExists = -1;
	int durationExists = -1;
	int offsetExists = -1;
	std::string processName;
	std::string processPath;
	int processPid = 0;
	std::string highlightTime;
	int highlightDuration = 0;
	std::string offsetTime;
	int offsetDuration = 0;

	Arguments(const int argc, const char* argv[])
	{
		for (int i = 1; i < argc; ++i)
		{
			std::string arg = argv[i];
			if (arg == "--process" && i + 1 < argc)
			{
				processName = argv[i + 1];
				i++;
			}
			else if (arg == "--highlight" && i + 1 < argc)
			{
				highlightTime = argv[i + 1];
				i++;
			}
			else if (arg == "--offset" && i + 1 < argc)
			{
				offsetTime = argv[i + 1];
				i++;
			}
		}
#ifdef _DEBUG
		processName = "wow";
		highlightTime = "00:00:10";
		offsetTime = "10";
#endif
		if (!processName.empty())
		{
			processExists = FindProcess();
		}
		if (!highlightTime.empty())
		{
			durationExists = ParseTimeStrInto(highlightTime, highlightDuration);
		}
		if (!offsetTime.empty())
		{
			offsetExists = ParseTimeStrInto(offsetTime, offsetDuration);
		}
		valid = processExists == 0 && durationExists == 0;
	}

private:

	int FindProcess()
	{
		std::string processNameLower = processName;
		std::transform(processNameLower.begin(), processNameLower.end(), processNameLower.begin(), [](unsigned char c) { return std::tolower(c); });
		int result = 1;
		PROCESSENTRY32 entry;
		entry.dwSize = sizeof(PROCESSENTRY32);
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		if (Process32FirstW(snapshot, &entry) == TRUE)
		{
			while (Process32NextW(snapshot, &entry) == TRUE)
			{
				std::wstring processNameW(entry.szExeFile);
				std::string processNameA(processNameW.begin(), processNameW.end());
				std::string processNameALower = processNameA;
				std::transform(processNameALower.begin(), processNameALower.end(), processNameALower.begin(), [](unsigned char c) { return std::tolower(c); });
				if (processNameLower == processNameALower || processNameALower.find(processNameLower) != std::string::npos)
				{
					HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
					if (hProcess == NULL)
					{
						result = 2;
					}
					else
					{
						wchar_t szTargetFileName[MAX_PATH];
						if (GetModuleFileNameExW(hProcess, NULL, szTargetFileName, MAX_PATH) > 0)
						{
							result = 0;
							processPath = wcharToString(szTargetFileName);
							processPid = entry.th32ProcessID;
						}
						else
						{
							result = 3;
						}
						CloseHandle(hProcess);
					}
					break;
				}
			}
		}
		CloseHandle(snapshot);
		return result;
	}

	int ParseTimeStrInto(std::string& timeStr, int& intoVar)
	{
		int hours, minutes, seconds;
		char colon;
		std::istringstream iss(timeStr);
		if (iss >> hours >> colon >> minutes >> colon >> seconds)
		{
			intoVar = hours * 3600 + minutes * 60 + seconds;
			return 0;
		}
		iss.clear();
		iss.seekg(0);
		if (iss >> seconds)
		{
			intoVar = seconds;
			return 0;
		}
		return 1;
	}

};

void log(const std::string& str)
{
	std::cout << str << std::endl;
}

void log(const std::wstring& str)
{
	std::wcout << str << std::endl;
}

struct HighlightsDataHolder
{
	std::string id;
	bool isScreenshot = false;
	int startDelta = 0;
	int endDelta = 0;
	std::vector<GfeSDK::NVGSDK_LocalizedPair> namePairs;
	std::map<std::string, std::string> namePairsData;
};

struct HighlightsData
{
	std::string gameName;
	std::string defaultLocale;
	std::vector<HighlightsDataHolder> highlightsData;
	std::vector<GfeSDK::NVGSDK_Highlight> highlights;
};

HighlightsData highlight;

std::string groupId = "GROUP_1";

int open(const std::string& name, const std::string& targetPath, int targetPid)
{
	std::string locale = "en-us";
	std::string label = "Highlight";
	highlight.gameName = name;
	highlight.defaultLocale = "en-US";
	highlight.highlightsData.resize(1);
	highlight.highlights.resize(1);
	highlight.highlightsData[0].id = "HIGHLIGHT";
	highlight.highlightsData[0].isScreenshot = false;
	highlight.highlightsData[0].startDelta = 0;
	highlight.highlightsData[0].endDelta = 0;
	highlight.highlightsData[0].namePairsData[locale] = label;
	highlight.highlightsData[0].namePairs.resize(1);
	highlight.highlightsData[0].namePairs[0].localeCode = locale.c_str();
	highlight.highlightsData[0].namePairs[0].localizedString = label.c_str();
	highlight.highlights[0].id = "HIGHLIGHT";
	highlight.highlights[0].userInterest = true;
	highlight.highlights[0].significance = GfeSDK::NVGSDK_HighlightSignificance::NVGSDK_HIGHLIGHT_SIGNIFICANCE_GOOD;
	highlight.highlights[0].highlightTags = GfeSDK::NVGSDK_HighlightType::NVGSDK_HIGHLIGHT_TYPE_MILESTONE;
	highlight.highlights[0].nameTable = new GfeSDK::NVGSDK_LocalizedPair[1];
	highlight.highlights[0].nameTable[0] = highlight.highlightsData[0].namePairs[0];
	highlight.highlights[0].nameTableSize = 1;
	return gfeSdkWrapper.Init(highlight.gameName.c_str(), highlight.defaultLocale.c_str(), &highlight.highlights[0], highlight.highlights.size(), targetPath.c_str(), targetPid);
}

int save(int duration, int offset)
{
	if (duration > MAX_HIGHLIGHT_DURATION)
	{
		duration = MAX_HIGHLIGHT_DURATION;
	}
	if (offset > MAX_HIGHLIGHT_DURATION)
	{
		offset = MAX_HIGHLIGHT_DURATION;
	}
	if (duration + offset > MAX_HIGHLIGHT_DURATION)
	{
		duration = MAX_HIGHLIGHT_DURATION;
		offset = 0;
	}
	highlight.highlightsData[0].startDelta = (duration + offset) * -1000;
	highlight.highlightsData[0].endDelta = offset * -1000;
#ifdef USE_PROMISES
	int result;
	result = gfeSdkWrapper.OnOpenGroup(groupId);
	if (result != 1)
	{
		return result;
	}
	result = gfeSdkWrapper.OnSaveVideo(highlight.highlightsData[0].id, groupId, highlight.highlightsData[0].startDelta, highlight.highlightsData[0].endDelta);
	if (result != 1)
	{
		return result;
	}
	gfeSdkWrapper.OnCloseGroup(groupId, true);
	return result;
#else
	(std::async([&]() { gfeSdkWrapper.OnOpenGroup(groupId); })).wait();
	(std::async([&]() { gfeSdkWrapper.OnSaveVideo(highlight.highlightsData[0].id, groupId, highlight.highlightsData[0].startDelta, highlight.highlightsData[0].endDelta); })).wait();
	(std::async([&]() { gfeSdkWrapper.OnCloseGroup(groupId, true); })).wait();
	return 1;
#endif
}

void close()
{
	gfeSdkWrapper.DeInit();
}

void signalHandler(int signal)
{
	if (signal == SIGINT)
	{
		log("Aborted by user...");
		close();
		exit(EXIT_FAILURE);
	}
}

int main(const int argc, const char* argv[])
{
	int code = EXIT_FAILURE;
	Arguments args = Arguments(argc, argv);
	if (!args.valid)
	{
		if (args.processExists == -1 || args.durationExists == -1)
		{
			log("Invalid use of gfecli. Please include the required options.");
			log("");
			log("Options:");
			log("  --process <executable>");
			log("  --highlight <duration>");
			log("  --offset <duration>");
			log("");
			log("Variables:");
			log("  executable - the full process name or partial name");
			log("  duration - HH:MM:SS format or in seconds");
			log("");
			log("Examples:");
			log("  Save highlight of the past 2m 30s");
			log("    gfecli --process game.exe --highlight 00:02:30");
			log("  Save highlight of the past 5m but start from 1m ago");
			log("    gfecli --process game.exe --highlight 00:05:00 --offset 00:01:00");
		}
		else
		{
			log("Unable to find process or highlight duration.");
		}
		return code;
	}
	signal(SIGINT, signalHandler);
	int result = open("gfecli", args.processPath, args.processPid);
	if (result == 1)
	{
		result = save(args.highlightDuration, args.offsetDuration);
		int startFrom = highlight.highlightsData[0].startDelta;
		int endAt = highlight.highlightsData[0].endDelta;
		int duration = startFrom - endAt;
		std::string rangeText = msTimeToString(duration) + " [-" + msTimeToString(startFrom) + ", -" + msTimeToString(endAt) + "]";
		if (result == 1)
		{
			code = EXIT_SUCCESS;
			log("Successfully saved highlight. " + rangeText);
		}
		else
		{
			log("Failed to save highlight. " + rangeText);
		}
	}
	else if (result == 2)
	{
		log("Permission denied.");
	}
	else if (result != 3)
	{
		log("Permission request timed out.");
	}
	close();
	return code;
}
