#pragma once

#include "Program.hpp"

#ifdef HAVE_GFESDK

#include <codecvt>
#include <locale>
#include <memory>

#include <gfesdk/bindings/cpp/isdk_cpp_impl.h>
#include <gfesdk/bindings/cpp/highlights/ihighlights_cpp_impl.h>

class Gfe
{

public:

	Gfe();
	int Init(char const* gameName, char const* defaultLocale, GfeSDK::NVGSDK_Highlight* highlights, size_t numHighlights, char const* targetPath, int targetPid);
	void DeInit();
	void OnNotification(GfeSDK::NVGSDK_NotificationType type, GfeSDK::NotificationBase const&);
	int OnOpenGroup(std::string const& id);
	int OnCloseGroup(std::string const& id, bool destroy = false);
	int OnSaveVideo(std::string const& highlightId, std::string const& groupId, int startDelta, int endDelta);

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

#endif
