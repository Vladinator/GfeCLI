#pragma once

#include <codecvt>
#include <locale>
#include <memory>

#include <gfesdk/bindings/cpp/isdk_cpp_impl.h>
#include <gfesdk/bindings/cpp/highlights/ihighlights_cpp_impl.h>

class GfeSdkWrapper
{

public:

	GfeSdkWrapper();
	int Init(char const* gameName, char const* defaultLocale, GfeSDK::NVGSDK_Highlight* highlights, size_t numHighlights, char const* targetPath, int targetPid);
	void DeInit();
	//void OnTick(void);
	void OnNotification(GfeSDK::NVGSDK_NotificationType type, GfeSDK::NotificationBase const&);
	int OnOpenGroup(std::string const& id);
	int OnCloseGroup(std::string const& id, bool destroy = false);
	// int OnSaveScreenshot(std::string const& highlightId, std::string const& groupId);
	int OnSaveVideo(std::string const& highlightId, std::string const& groupId, int startDelta, int endDelta);
	//int OnGetNumHighlights(std::string const& groupId, GfeSDK::NVGSDK_HighlightSignificance sigFilter, GfeSDK::NVGSDK_HighlightType tagFilter);
	//int OnOpenSummary(char const* groupIds[], size_t numGroups, GfeSDK::NVGSDK_HighlightSignificance sigFilter, GfeSDK::NVGSDK_HighlightType tagFilter);
	//int OnRequestLanguage(void);
	//int OnRequestUserSettings(void);
	//std::wstring const& GetCurrentPermissionStr(void) const;
	//std::wstring const& GetLastOverlayEvent(void) const;
	//std::wstring const& GetLastResult(void) const;
	//std::wstring const& GetLastQueryResult(void) const;

private:

	void ConfigureHighlights(char const* defaultLocale, GfeSDK::NVGSDK_Highlight* highlights, size_t numHighlights);
	void UpdateLastResultString(GfeSDK::NVGSDK_RetCode rc);
	std::unique_ptr<GfeSDK::Core> m_gfesdk;
	std::unique_ptr<GfeSDK::Highlights> m_highlights;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> m_converter;
	std::wstring m_currentPermission;
	std::wstring m_lastOverlayEvent;
	std::wstring m_lastResult;
	std::wstring m_lastQueryResult;

};
