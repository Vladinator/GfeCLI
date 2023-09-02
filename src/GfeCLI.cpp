#pragma warning(disable: 4244)

#include <iostream>
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

constexpr auto MAX_HIGHLIGHT_DURATION = 1200;

class Arguments
{

public:

	bool valid = false;
	int processExists = -1;
	int durationExists = -1;
	std::string processName;
	std::string processPath;
	int processPid = 0;
	std::string highlightTime;
	int highlightDuration = 0;

	Arguments(int argc, char* argv[])
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
		}
		if (!processName.empty())
		{
			processExists = FindProcess();
		}
		if (!highlightTime.empty())
		{
			durationExists = ExtractDuration();
		}
		valid = processExists == 0 && durationExists == 0;
	}

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

	int ExtractDuration()
	{
		int result = 1;
		int hours, minutes, seconds;
		char colon;
		std::istringstream iss(highlightTime);
		if (iss >> hours >> colon >> minutes >> colon >> seconds)
		{
			highlightDuration = hours * 3600 + minutes * 60 + seconds;
			if (highlightDuration > 0)
			{
				result = 0;
			}
		}
		return result;
	}

};

void log(std::string str)
{
	std::cout << str << std::endl;
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

std::string groupId = "GROUP";

void open(std::string name, std::string targetPath, int targetPid)
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
	(std::async([&]() { gfeSdkWrapper.Init(highlight.gameName.c_str(), highlight.defaultLocale.c_str(), &highlight.highlights[0], highlight.highlights.size(), targetPath.c_str(), targetPid); })).wait();
}

void save(int duration)
{
	if (duration > MAX_HIGHLIGHT_DURATION)
	{
		duration = MAX_HIGHLIGHT_DURATION;
	}
	// const char* groupIds[] = { groupId.c_str() };
	highlight.highlightsData[0].startDelta = duration * -1000;
	(std::async([&]() { gfeSdkWrapper.OnOpenGroup(groupId); })).wait();
	(std::async([&]() { gfeSdkWrapper.OnSaveVideo(highlight.highlightsData[0].id, groupId, highlight.highlightsData[0].startDelta, highlight.highlightsData[0].endDelta); })).wait();
	// (std::async([&]() { gfeSdkWrapper.OnOpenSummary(groupIds, 1, highlight.highlights[0].significance, highlight.highlights[0].highlightTags); })).wait();
	(std::async([&]() { gfeSdkWrapper.OnCloseGroup(groupId, true); })).wait();
}

void close()
{
	gfeSdkWrapper.DeInit();
}

int main(int argc, char* argv[])
{
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
			log("");
			log("Example:");
			log("  Save highlight of the past 2m 30s");
			log("  gfecli --process game.exe --highlight 00:02:30");
		}
		else
		{
			log("gfecli was unable to save highlight for process.");
		}
		return 0;
	}
	open("gfecli", args.processPath, args.processPid);
	save(args.highlightDuration);
	close();
	return 0;
}
