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
	void Init(char const* gameName, char const* defaultLocale, GfeSDK::NVGSDK_Highlight* highlights, size_t numHighlights, char const* targetPath, int targetPid);
	void DeInit();
	void OnTick(void);
	void OnNotification(GfeSDK::NVGSDK_NotificationType type, GfeSDK::NotificationBase const&);
	void OnOpenGroup(std::string const& id);
	void OnCloseGroup(std::string const& id, bool destroy = false);
	void OnSaveScreenshot(std::string const& highlightId, std::string const& groupId);
	void OnSaveVideo(std::string const& highlightId, std::string const& groupId, int startDelta, int endDelta);
	void OnGetNumHighlights(std::string const& groupId, GfeSDK::NVGSDK_HighlightSignificance sigFilter, GfeSDK::NVGSDK_HighlightType tagFilter);
	void OnOpenSummary(char const* groupIds[], size_t numGroups, GfeSDK::NVGSDK_HighlightSignificance sigFilter, GfeSDK::NVGSDK_HighlightType tagFilter);
	void OnRequestLanguage(void);
	void OnRequestUserSettings(void);
	wchar_t const* GetCurrentPermissionStr(void);
	wchar_t const* GetLastOverlayEvent(void);
	wchar_t const* GetLastResult(void);
	wchar_t const* GetLastQueryResult(void);

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
