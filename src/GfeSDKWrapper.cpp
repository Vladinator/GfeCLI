//#define USE_PROMISES

#include <future>
#include <Windows.h>

#include "GfeSDKWrapper.hpp"

const std::chrono::milliseconds asyncTimeout(5000);

char logBuffer[512];

void log(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsprintf_s(logBuffer, sizeof(logBuffer) / sizeof(logBuffer[0]), fmt, args);
	printf("%s\n", logBuffer);
	OutputDebugStringA(logBuffer);
	OutputDebugStringA("\n");
	va_end(args);
}

static std::wstring permissionToStringW(GfeSDK::NVGSDK_Permission p)
{
	switch (p)
	{
	case GfeSDK::NVGSDK_PERMISSION_MUST_ASK:
		return L"Must Ask";
	case GfeSDK::NVGSDK_PERMISSION_GRANTED:
		return L"Granted";
	case GfeSDK::NVGSDK_PERMISSION_DENIED:
		return L"Denied";
	default:
		return L"UNKNOWN";
	}
}

GfeSdkWrapper::GfeSdkWrapper() :
	m_currentPermission(permissionToStringW(GfeSDK::NVGSDK_PERMISSION_MUST_ASK)),
	m_lastOverlayEvent(L""),
	m_lastResult(L""),
	m_lastQueryResult(L"")
{
}

int GfeSdkWrapper::Init(char const* gameName, char const* defaultLocale, GfeSDK::NVGSDK_Highlight* highlights, size_t numHighlights, char const* targetPath, int targetPid)
{
	using namespace std::placeholders;

	GfeSDK::CreateInputParams createParams;
	createParams.appName = "GfeCLI";
	createParams.pollForCallbacks = true;
	createParams.requiredScopes = {
		GfeSDK::NVGSDK_SCOPE_HIGHLIGHTS,
		GfeSDK::NVGSDK_SCOPE_HIGHLIGHTS_VIDEO,
		GfeSDK::NVGSDK_SCOPE_HIGHLIGHTS_SCREENSHOT
	};
	createParams.notificationCallback = std::bind(&GfeSdkWrapper::OnNotification, this, _1, _2);

	if (targetPath != NULL && targetPid != 0)
	{
		createParams.targetPath = targetPath;
		createParams.targetPid = targetPid;
	}

	GfeSDK::CreateResponse response;
	GfeSDK::Core* gfesdkCore = GfeSDK::Core::Create(createParams, response);

	if (GfeSDK::NVGSDK_SUCCEEDED(response.returnCode))
	{
		log("Success: %s", GfeSDK::NVGSDK_RetCodeToString(response.returnCode));
		log("PC is running GFE version %s", response.nvidiaGfeVersion.c_str());
		log("PC is running GfeSDK version %d.%d", response.versionMajor, response.versionMinor);
		switch (response.returnCode)
		{
		case GfeSDK::NVGSDK_SUCCESS_VERSION_OLD_GFE:
			log("Compatible, but the user should update to the latest version of GFE.");
			break;
		case GfeSDK::NVGSDK_SUCCESS_VERSION_OLD_SDK:
			log("Compatible, but this application should update to a more recent version of GfeSDK.");
			break;
		}
	}
	else
	{
		log("Failure: %s", GfeSDK::NVGSDK_RetCodeToString(response.returnCode));
		switch (response.returnCode)
		{
		case GfeSDK::NVGSDK_ERR_SDK_VERSION:
			log("This application uses a version of GfeSDK that is too old to communicate with GFE.");
			log("PC is running GFE version %s", response.nvidiaGfeVersion.c_str());
			log("PC is running GfeSDK version %d.%d", response.versionMajor, response.versionMinor);
			break;
		case GfeSDK::NVGSDK_SUCCESS_VERSION_OLD_SDK:
			log("This user is running a version of GFE that is too old to communicate with applications GfeSDK.");
			log("PC is running GFE version %s", response.nvidiaGfeVersion.c_str());
			log("PC is running GfeSDK version %d.%d", response.versionMajor, response.versionMinor);
			break;
		}
		return 3;
	}

	m_gfesdk.reset(gfesdkCore);
	UpdateLastResultString(response.returnCode);

	if (response.scopePermissions.find(GfeSDK::NVGSDK_SCOPE_HIGHLIGHTS_VIDEO) != response.scopePermissions.end())
	{
		m_currentPermission = permissionToStringW(response.scopePermissions[GfeSDK::NVGSDK_SCOPE_HIGHLIGHTS_VIDEO]);
	}

	GfeSDK::RequestPermissionsParams requestPermissionsParams;

	for (auto&& entry : response.scopePermissions)
	{
		if (entry.second == GfeSDK::NVGSDK_PERMISSION_MUST_ASK)
		{
			requestPermissionsParams.scopes.push_back(entry.first);
		}
	}

	if (!requestPermissionsParams.scopes.empty())
	{
		std::promise<void> promise;
		std::future<void> future = promise.get_future();
		int result = 0;
		m_gfesdk->RequestPermissionsAsync(requestPermissionsParams, [this, defaultLocale, highlights, numHighlights, &promise, &result](GfeSDK::NVGSDK_RetCode rc, void* cbContext)
			{
				UpdateLastResultString(rc);
				result = 2;
				if (GfeSDK::NVGSDK_SUCCEEDED(rc))
				{
					ConfigureHighlights(defaultLocale, highlights, numHighlights);
					result = 1;
				}
				promise.set_value();
			}
		);
		future.wait_for(asyncTimeout);
		return result;
	}

	ConfigureHighlights(defaultLocale, highlights, numHighlights);
	return 1;
}

void GfeSdkWrapper::DeInit()
{
	GfeSDK::Core* gfesdkCore = m_gfesdk.release();
	delete gfesdkCore;
}

//void GfeSdkWrapper::OnTick(void)
//{
//	if (!m_gfesdk)
//	{
//		return;
//	}
//	m_gfesdk->Poll();
//}

void GfeSdkWrapper::OnNotification(GfeSDK::NVGSDK_NotificationType type, GfeSDK::NotificationBase const& notification)
{
	switch (type)
	{
	case GfeSDK::NVGSDK_NOTIFICATION_PERMISSIONS_CHANGED:
	{
		GfeSDK::PermissionsChangedNotification const& n = static_cast<GfeSDK::PermissionsChangedNotification const&>(notification);
		auto hlPermission = n.scopePermissions.find(GfeSDK::NVGSDK_SCOPE_HIGHLIGHTS_VIDEO);
		if (hlPermission != n.scopePermissions.end())
		{
			m_currentPermission = permissionToStringW(hlPermission->second);
		}
		break;
	}
	case GfeSDK::NVGSDK_NOTIFICATION_OVERLAY_STATE_CHANGED:
	{
		GfeSDK::OverlayStateChangedNotification const& n = static_cast<GfeSDK::OverlayStateChangedNotification const&>(notification);
		m_lastOverlayEvent.clear();
		switch (n.state)
		{
		case GfeSDK::NVGSDK_OVERLAY_STATE_MAIN:
			m_lastOverlayEvent += L"Main Overlay Window";
			break;
		case GfeSDK::NVGSDK_OVERLAY_STATE_PERMISSION:
			m_lastOverlayEvent += L"Permission Overlay Window";
			break;
		case GfeSDK::NVGSDK_OVERLAY_STATE_HIGHLIGHTS_SUMMARY:
			m_lastOverlayEvent += L"Highlights Summary Overlay Window";
			break;
		default:
			m_lastOverlayEvent += L"UNKNOWNWindow";
		}
		m_lastOverlayEvent += (n.open ? L" OPEN" : L" CLOSE");
		break;
	}
	}
}

int GfeSdkWrapper::OnOpenGroup(std::string const& id)
{
	if (!m_highlights)
	{
		return 2;
	}
	GfeSDK::HighlightOpenGroupParams params;
	params.groupId = id;
	params.groupDescriptionLocaleTable["en-US"] = id;
#ifdef USE_PROMISES
	std::promise<void> promise;
	std::future<void> future = promise.get_future();
	int result = 0;
	m_highlights->OpenGroupAsync(params, [this, &promise, &result](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
			result = 1;
			promise.set_value();
		}
	);
	future.wait_for(asyncTimeout);
	return result;
#else
	m_highlights->OpenGroupAsync(params, [this](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
		}
	);
	return 1;
#endif
}

int GfeSdkWrapper::OnCloseGroup(std::string const& id, bool destroy)
{
	if (!m_highlights)
	{
		return 2;
	}
	GfeSDK::HighlightCloseGroupParams params;
	params.groupId = id;
	params.destroyHighlights = destroy;
#ifdef USE_PROMISES
	std::promise<void> promise;
	std::future<void> future = promise.get_future();
	int result = 0;
	m_highlights->CloseGroupAsync(params, [this, &promise, &result](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
			result = 1;
			promise.set_value();
		}
	);
	future.wait_for(asyncTimeout);
	return result;
#else
	m_highlights->CloseGroupAsync(params, [this](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
		}
	);
	return 1;
#endif
}

//int GfeSdkWrapper::OnSaveScreenshot(std::string const& highlightId, std::string const& groupId)
//{
//	GfeSDK::ScreenshotHighlightParams params;
//	params.groupId = groupId;
//	params.highlightId = highlightId;
//	std::promise<void> promise;
//	std::future<void> future = promise.get_future();
//	int result = 0;
//	m_highlights->SetScreenshotHighlightAsync(params, [this, &promise, &result](GfeSDK::NVGSDK_RetCode rc, void*)
//		{
//			UpdateLastResultString(rc);
//			result = 1;
//			promise.set_value();
//		}
//	);
//	future.wait_for(asyncTimeout);
//	return result;
//}

int GfeSdkWrapper::OnSaveVideo(std::string const& highlightId, std::string const& groupId, int startDelta, int endDelta)
{
	GfeSDK::VideoHighlightParams params;
	params.startDelta = startDelta;
	params.endDelta = endDelta;
	params.groupId = groupId;
	params.highlightId = highlightId;
#ifdef USE_PROMISES
	std::promise<void> promise;
	std::future<void> future = promise.get_future();
	int result = 0;
	m_highlights->SetVideoHighlightAsync(params, [this, &promise, &result](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
			result = 1;
			promise.set_value();
		}
	);
	future.wait_for(asyncTimeout);
	return result;
#else
	m_highlights->SetVideoHighlightAsync(params, [this](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
		}
	);
	return 1;
#endif
}

//int GfeSdkWrapper::OnGetNumHighlights(std::string const& groupId, GfeSDK::NVGSDK_HighlightSignificance sigFilter, GfeSDK::NVGSDK_HighlightType tagFilter)
//{
//	GfeSDK::GroupView v;
//	v.groupId = groupId;
//	v.significanceFilter = sigFilter;
//	v.tagsFilter = tagFilter;
//	std::promise<void> promise;
//	std::future<void> future = promise.get_future();
//	int result = 0;
//	m_highlights->GetNumberOfHighlightsAsync(v, [this](GfeSDK::NVGSDK_RetCode rc, GfeSDK::GetNumberOfHighlightsResponse const* response, void*)
//		{
//			UpdateLastResultString(rc);
//			result = 2;
//			if (GfeSDK::NVGSDK_SUCCEEDED(rc))
//			{
//				m_lastQueryResult = std::to_wstring(response->numHighlights);
//				result = 1;
//			}
//			promise.set_value();
//		}
//	);
//	future.wait_for(asyncTimeout);
//	return result;
//}

//int GfeSdkWrapper::OnOpenSummary(char const* groupIds[], size_t numGroups, GfeSDK::NVGSDK_HighlightSignificance sigFilter, GfeSDK::NVGSDK_HighlightType tagFilter)
//{
//	GfeSDK::SummaryParams params;
//	for (size_t i = 0; i < numGroups; ++i)
//	{
//		GfeSDK::GroupView v;
//		v.groupId = groupIds[i];
//		v.significanceFilter = sigFilter;
//		v.tagsFilter = tagFilter;
//		params.groupViews.push_back(v);
//	}
//	std::promise<void> promise;
//	std::future<void> future = promise.get_future();
//	int result = 0;
//	m_highlights->OpenSummaryAsync(params, [this, &promise, &result](GfeSDK::NVGSDK_RetCode rc, void*)
//		{
//			UpdateLastResultString(rc);
//			result = 1;
//			promise.set_value();
//		}
//	);
//	future.wait_for(asyncTimeout);
//	return result;
//}

//int GfeSdkWrapper::OnRequestLanguage(void)
//{
//	std::promise<void> promise;
//	std::future<void> future = promise.get_future();
//	int result = 0;
//	m_gfesdk->GetUILanguageAsync([this, &promise, &result](GfeSDK::NVGSDK_RetCode rc, GfeSDK::GetUILanguageResponse const* response, void* context)
//		{
//			GfeSdkWrapper* thiz = reinterpret_cast<GfeSdkWrapper*>(context);
//			UpdateLastResultString(rc);
//			result = 2;
//			if (GfeSDK::NVGSDK_SUCCEEDED(rc))
//			{
//				m_lastQueryResult = m_converter.from_bytes(response->cultureCode);
//				result = 1;
//			}
//			promise.set_value();
//		},
//		this
//	);
//	future.wait_for(asyncTimeout);
//	return result;
//}

//int GfeSdkWrapper::OnRequestUserSettings(void)
//{
//	std::promise<void> promise;
//	std::future<void> future = promise.get_future();
//	int result = 0;
//	m_highlights->GetUserSettingsAsync([this, &promise, &result](GfeSDK::NVGSDK_RetCode rc, GfeSDK::GetUserSettingsResponse const* response, void*)
//		{
//			UpdateLastResultString(rc);
//			m_lastQueryResult = L"";
//			result = 2;
//			if (GfeSDK::NVGSDK_SUCCEEDED(rc))
//			{
//				for (auto it = response->highlightSettings.begin(); it != response->highlightSettings.end(); ++it)
//				{
//					m_lastQueryResult += L"\n" + m_converter.from_bytes(it->highlightId) + (it->enabled ? L": ON" : L": OFF");
//				}
//				result = 1;
//			}
//			promise.set_value();
//		}
//	);
//	future.wait_for(asyncTimeout);
//	return result;
//}

//std::wstring const& GfeSdkWrapper::GetCurrentPermissionStr(void) const
//{
//	return m_currentPermission;
//}

//std::wstring const& GfeSdkWrapper::GetLastOverlayEvent(void) const
//{
//	return m_lastOverlayEvent;
//}

//std::wstring const& GfeSdkWrapper::GetLastResult(void) const
//{
//	return m_lastResult;
//}

//std::wstring const& GfeSdkWrapper::GetLastQueryResult(void) const
//{
//	return m_lastQueryResult;
//}

void GfeSdkWrapper::ConfigureHighlights(char const* defaultLocale, GfeSDK::NVGSDK_Highlight* highlights, size_t numHighlights)
{
	m_highlights.reset(GfeSDK::Highlights::Create(m_gfesdk.get()));
	GfeSDK::HighlightConfigParams configParams;
	configParams.defaultLocale = defaultLocale;
	for (size_t i = 0; i < numHighlights; ++i)
	{
		GfeSDK::HighlightDefinition highlightDef;
		highlightDef.id = highlights[i].id;
		highlightDef.userDefaultInterest = highlights[i].userInterest;
		highlightDef.significance = highlights[i].significance;
		highlightDef.highlightTags = highlights[i].highlightTags;
		for (size_t j = 0; j < highlights[i].nameTableSize; ++j)
		{
			highlightDef.nameLocaleTable[highlights[i].nameTable[j].localeCode] = highlights[i].nameTable[j].localizedString;
		}
		configParams.highlightDefinitions.push_back(highlightDef);
	}
	m_highlights->ConfigureAsync(configParams, [this](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
		}
	);
}

void GfeSdkWrapper::UpdateLastResultString(GfeSDK::NVGSDK_RetCode rc)
{
	m_lastResult = m_converter.from_bytes(GfeSDK::RetCodeToString(rc));
}
