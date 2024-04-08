#ifndef SERVICE_BASE_H_
#define SERVICE_BASE_H_

#include <windows.h>
#include <string>

// Base Service class used to create windows services.
class ServiceBase {
public:
    ServiceBase(const ServiceBase& other) = delete;
    ServiceBase& operator=(const ServiceBase& other) = delete;

    ServiceBase(ServiceBase&& other) = delete;
    ServiceBase& operator=(ServiceBase&& other) = delete;

    virtual ~ServiceBase() {}

    // Called by windows when starting the service.
    bool Run() {
        return RunInternal(this);
    }

    const std::wstring& GetName() const {
        return m_name;
    }
    const std::wstring& GetDisplayName() const { return m_displayName; }
    const DWORD GetStartType() const { return m_dwStartType; }
    const DWORD GetErrorControlType() const { return m_dwErrorCtrlType; }
    const std::wstring& GetDependencies() const { return m_depends; }

    // Account info service runs under.
    const std::wstring& GetAccount() const { return m_account; }
    const std::wstring& GetPassword() const { return m_password; }
protected:
    ServiceBase(const std::wstring& name,
        const std::wstring& displayName,
        DWORD dwStartType,
        DWORD dwErrCtrlType = SERVICE_ERROR_NORMAL,
        DWORD dwAcceptedCmds = SERVICE_ACCEPT_STOP,
        const std::wstring& depends = (L""),
        const std::wstring& account = (L""),
        const std::wstring& PassWord = (L""));

    void SetStatus(DWORD dwState, DWORD dwErrCode = NO_ERROR, DWORD dwWait = 0);

    // Overro=ide these functions as you need.
    virtual void OnStart(DWORD argc, wchar_t* argv[]) = 0;
    virtual void OnStop() {}
    virtual void OnPause() {}
    virtual void OnContinue() {}
    virtual void OnShutdown() {}
    virtual void OnSessionChange(DWORD /*evtType*/,
        WTSSESSION_NOTIFICATION* /*notification*/) {}
private:
    // Registers handle and starts the service.
    static void WINAPI SvcMain(DWORD argc, TCHAR* argv[]);

    // Called whenever service control manager updates service Status.
    static DWORD WINAPI ServiceCtrlHandler(DWORD ctrlCode, DWORD evtType,
        void* evtData, void* context);

    static bool RunInternal(ServiceBase* svc);

    void Start(DWORD argc, TCHAR* argv[]);
    void Stop();
    void Pause();
    void Continue();
    void Shutdown();

    std::wstring m_name;
    std::wstring m_displayName;
    DWORD m_dwStartType;
    DWORD m_dwErrorCtrlType;
    std::wstring m_depends;
    std::wstring m_account;
    std::wstring m_password;

    // Info about service dependencies and user account.
    bool m_hasDepends = false;
    bool m_hasAcc = false;
    bool m_hasPass = false;

    SERVICE_STATUS m_svcStatus;
    SERVICE_STATUS_HANDLE m_svcStatusHandle;

    static ServiceBase* m_service;
};

#endif // SERVICE_BASE_H_
