#include "stdafx.h"
#include "NotificationClient.h"

#include "SoundRender_CoreA.h"

CNotificationClient::CNotificationClient()
    : m_bComInitialized(false)
{
    Start();
}

CNotificationClient::~CNotificationClient() 
{
    Close();
}

inline bool CNotificationClient::Start() 
{
    // Initialize the COM library for the current thread
    HRESULT ihr = CoInitialize(nullptr);

    // RPC_E_CHANGED_MODE means COM already initialized in different mode - this is OK
    if (SUCCEEDED(ihr) || ihr == RPC_E_CHANGED_MODE) {
        m_bComInitialized = SUCCEEDED(ihr);

        // Create the device enumerator
        IMMDeviceEnumerator* pEnumerator;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if (SUCCEEDED(hr)) {
            // Register for device change notifications
            hr = pEnumerator->RegisterEndpointNotificationCallback(this);
            m_pEnumerator = pEnumerator;

            return true;
        }

        if (m_bComInitialized) {
            CoUninitialize();
            m_bComInitialized = false;
        }
    }

    return false;
}

inline void CNotificationClient::Close() 
{
    // Unregister the device enumerator
    if (m_pEnumerator) {
        m_pEnumerator->UnregisterEndpointNotificationCallback(this);
        m_pEnumerator->Release();
    }

    if (m_bComInitialized) {
        CoUninitialize();
        m_bComInitialized = false;
    }
}

inline STDMETHODIMP_(HRESULT __stdcall) CNotificationClient::OnDeviceAdded(LPCWSTR pwstrDeviceId) 
{
    // A new audio device has been added.
    return S_OK;
}


// IMMCNotificationClient methods

inline STDMETHODIMP_(HRESULT __stdcall) CNotificationClient::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
{
	// Only handle render (output) devices with console (default) role
    if (flow != eRender || role != eConsole)
        return S_OK;

    SoundRenderA->bPendingDefaultDeviceSwitch = TRUE;
    return S_OK;
}

inline STDMETHODIMP_(HRESULT __stdcall) CNotificationClient::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
    SoundRenderA->bPendingDeviceListRefresh = TRUE;
    return S_OK;
}

inline STDMETHODIMP_(HRESULT __stdcall) CNotificationClient::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
    SoundRenderA->bPendingDeviceListRefresh = TRUE;
    return S_OK;
}

inline STDMETHODIMP_(HRESULT __stdcall) CNotificationClient::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
    SoundRenderA->bPendingDeviceListRefresh = TRUE;
    return S_OK;
}
