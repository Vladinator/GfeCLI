#include "Gfe.hpp"

#ifdef HAVE_GFESDK

#include <future>
#include <Windows.h>

const std::chrono::milliseconds timeoutAction(10000);
const std::chrono::milliseconds timeout(250);

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

Gfe::Gfe() :
	m_currentPermission(permissionToStringW(GfeSDK::NVGSDK_PERMISSION_MUST_ASK)),
	m_lastOverlayEvent(L""),
	m_lastResult(L""),
	m_lastQueryResult(L"")
{
}

int Gfe::Init(char const* gameName, char const* defaultLocale, GfeSDK::NVGSDK_Highlight* highlights, size_t numHighlights, char const* targetPath, int targetPid)
{
	GfeSDK::CreateInputParams createParams;
	createParams.appName = "GfeCLI";
	createParams.pollForCallbacks = true;
	createParams.requiredScopes = {
		GfeSDK::NVGSDK_SCOPE_HIGHLIGHTS,
		GfeSDK::NVGSDK_SCOPE_HIGHLIGHTS_VIDEO,
		GfeSDK::NVGSDK_SCOPE_HIGHLIGHTS_SCREENSHOT
	};
	createParams.notificationCallback = std::bind(&Gfe::OnNotification, this, std::placeholders::_1, std::placeholders::_2);
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
		std::promise<int> promise;
		std::future<int> future = promise.get_future();
		m_gfesdk->RequestPermissionsAsync(requestPermissionsParams, [this, &promise, defaultLocale, highlights, numHighlights](GfeSDK::NVGSDK_RetCode rc, void* cbContext)
			{
				UpdateLastResultString(rc);
				if (!GfeSDK::NVGSDK_SUCCEEDED(rc))
				{
					promise.set_value(2);
					return;
				}
				ConfigureHighlights(defaultLocale, highlights, numHighlights);
				promise.set_value(1);
			}
		);
		if (future.wait_for(timeoutAction) == std::future_status::ready)
		{
			return future.get();
		}
		return 0;
	}
	ConfigureHighlights(defaultLocale, highlights, numHighlights);
	return 1;
}

void Gfe::DeInit()
{
	GfeSDK::Core* gfesdkCore = m_gfesdk.release();
	delete gfesdkCore;
}

void Gfe::OnNotification(GfeSDK::NVGSDK_NotificationType type, GfeSDK::NotificationBase const& notification)
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

int Gfe::OnOpenGroup(std::string const& id)
{
	if (!m_highlights)
	{
		return 2;
	}
	GfeSDK::HighlightOpenGroupParams params;
	params.groupId = id;
	params.groupDescriptionLocaleTable["en-US"] = id;
	std::promise<int> promise;
	std::future<int> future = promise.get_future();
	m_highlights->OpenGroupAsync(params, [this, &promise](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
			promise.set_value(1);
		}
	);
	if (future.wait_for(timeout) == std::future_status::ready)
	{
		return future.get();
	}
	return 0;
}

int Gfe::OnCloseGroup(std::string const& id, bool destroy)
{
	if (!m_highlights)
	{
		return 2;
	}
	GfeSDK::HighlightCloseGroupParams params;
	params.groupId = id;
	params.destroyHighlights = destroy;
	std::promise<int> promise;
	std::future<int> future = promise.get_future();
	m_highlights->CloseGroupAsync(params, [this, &promise](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
			promise.set_value(1);
		}
	);
	if (future.wait_for(timeout) == std::future_status::ready)
	{
		return future.get();
	}
	return 0;
}

int Gfe::OnSaveVideo(std::string const& highlightId, std::string const& groupId, int startDelta, int endDelta)
{
	GfeSDK::VideoHighlightParams params;
	params.startDelta = startDelta;
	params.endDelta = endDelta;
	params.groupId = groupId;
	params.highlightId = highlightId;
	std::promise<int> promise;
	std::future<int> future = promise.get_future();
	m_highlights->SetVideoHighlightAsync(params, [this, &promise](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
			promise.set_value(1);
		}
	);
	if (future.wait_for(timeout) == std::future_status::ready)
	{
		return future.get();
	}
	return 0;
}

int Gfe::ConfigureHighlights(char const* defaultLocale, GfeSDK::NVGSDK_Highlight* highlights, size_t numHighlights)
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
	std::promise<int> promise;
	std::future<int> future = promise.get_future();
	m_highlights->ConfigureAsync(configParams, [this, &promise](GfeSDK::NVGSDK_RetCode rc, void*)
		{
			UpdateLastResultString(rc);
			promise.set_value(1);
		}
	);
	if (future.wait_for(timeout) == std::future_status::ready)
	{
		return future.get();
	}
	return 0;
}

void Gfe::UpdateLastResultString(GfeSDK::NVGSDK_RetCode rc)
{
	m_lastResult = m_converter.from_bytes(GfeSDK::RetCodeToString(rc));
}

#endif
