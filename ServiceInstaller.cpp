#include "pch.h"
#include "ServiceInstaller.h"

#include <Windows.h>
#include <string.h>
#include <iostream>

namespace {
    class ServiceHandle {
    public:
        ServiceHandle(SC_HANDLE handle)
            : m_handle(handle) {}

        ~ServiceHandle() {
            if (m_handle) {
                ::CloseServiceHandle(m_handle);
            }
        }

        operator SC_HANDLE() {
            return m_handle;
        }

    private:
        SC_HANDLE m_handle = nullptr;
    };
}

//static
bool ServiceInstaller::Install(const ServiceBase& service)
{
    wchar_t modulePath[MAX_PATH];

    if (::GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
        printf(("Couldn't get module file name: %d\n"), ::GetLastError());
        return false;
    }

    std::wstring escapedPath(modulePath);
    if (modulePath[0] != L'\"')
    {
        escapedPath = L'\"' + escapedPath + L'\"';
    }
    wchar_t* bin = (wchar_t*)escapedPath.c_str();

    std::wcout << L"bin = " << bin << L"\n";

    ServiceHandle svcControlManager = ::OpenSCManagerW(nullptr, nullptr,
        SC_MANAGER_ALL_ACCESS);
    if (!svcControlManager) {
        printf("Couldn't open service control manager: %d\n", GetLastError());
        return false;
    }

    const std::wstring& depends = service.GetDependencies();
    const std::wstring& acc = service.GetAccount();
    const std::wstring& pass = service.GetPassword();

    ServiceHandle servHandle = ::CreateServiceW(svcControlManager,
        service.GetName().c_str(),
        service.GetDisplayName().c_str(),
        SERVICE_QUERY_STATUS,
        SERVICE_WIN32_OWN_PROCESS,
        service.GetStartType(),
        service.GetErrorControlType(),
        bin,
        nullptr,
        nullptr,
        (depends.length() <= 2 ? nullptr : depends.c_str()),
        (acc.length() <= 2 ? nullptr : acc.c_str()),
        (pass.length() <= 2 ? nullptr : pass.c_str()));
    if (!servHandle)
    {
        printf("Couldn't create service: %d\n", ::GetLastError());
        return false;
    }
    CloseServiceHandle(servHandle);
    CloseServiceHandle(svcControlManager);
    return true;
}

//static
bool ServiceInstaller::Uninstall(const ServiceBase& service) {
    ServiceHandle svcControlManager = ::OpenSCManagerW(nullptr, nullptr,
        SC_MANAGER_CONNECT);

    if (!svcControlManager) {
        printf("Couldn't open service control manager: %d\n", GetLastError());
        return false;
    }

    ServiceHandle servHandle = ::OpenServiceW(svcControlManager, service.GetName().c_str(),
        SERVICE_QUERY_STATUS |
        SERVICE_STOP |
        DELETE);

    if (!servHandle) {
        printf("Couldn't open service control manager: %d\n", ::GetLastError());
        return false;
    }

    SERVICE_STATUS servStatus = {};
    if (::ControlService(servHandle, SERVICE_CONTROL_STOP, &servStatus)) {
        printf("Stoping service %s\n", service.GetName());

        while (::QueryServiceStatus(servHandle, &servStatus)) {
            if (servStatus.dwCurrentState != SERVICE_STOP_PENDING) {
                break;
            }
        }

        if (servStatus.dwCurrentState != SERVICE_STOPPED) {
            printf("Failed to stop the service\n");
        }
        else {
            printf("Service stopped\n");
        }
    }
    else {
        printf("Didn't control service: %d\n", ::GetLastError());
    }

    if (!::DeleteService(servHandle)) {
        printf("Failed to delete the service: %d\n", GetLastError());
        return false;
    }

    return true;
}

bool ServiceInstaller::AutoRestart(const ServiceBase& service)
{
    auto schSCManager = OpenSCManagerW(
        NULL,                    // local computer
        NULL,                    // servicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return false;
    }

    // Get a handle to the service.

    auto schService = OpenServiceW(
        schSCManager,         // SCM database 
        service.GetName().c_str(),            // name of service 
        SERVICE_ALL_ACCESS);  // full access 

    if (schService == NULL)
    {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return false;
    }


    SERVICE_FAILURE_ACTIONS sfa;
    SC_ACTION actions[10];

    sfa.dwResetPeriod = 86400;
    sfa.lpCommand = NULL;
    sfa.lpRebootMsg = NULL;
    sfa.cActions = 10;
    sfa.lpsaActions = actions;

    for (int i = 0; i < 10; i++)
    {
        sfa.lpsaActions[i].Type = SC_ACTION_RESTART;
        sfa.lpsaActions[i].Delay = 256 << i;
    }

    if (ChangeServiceConfig2W(schService, SERVICE_CONFIG_FAILURE_ACTIONS, &sfa) == 0)
    {
        printf("Couldn't active auto restart: %d\n", ::GetLastError());
        return false;
    }
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    return true;
}

bool ServiceInstaller::DoStartSvc(const ServiceBase& service)
{
    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwOldCheckPoint;
    DWORD dwStartTickCount;
    DWORD dwWaitTime;
    DWORD dwBytesNeeded;

    // Get a handle to the SCM database. 

    auto schSCManager = OpenSCManagerW(
        NULL,                    // local computer
        NULL,                    // servicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return false;
    }

    // Get a handle to the service.

    auto schService = OpenServiceW(
        schSCManager,         // SCM database 
        service.GetName().c_str(),            // name of service 
        SERVICE_ALL_ACCESS);  // full access 

    if (schService == NULL)
    {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Check the Status in case the service is not stopped. 

    if (!QueryServiceStatusEx(
        schService,                     // handle to service 
        SC_STATUS_PROCESS_INFO,         // information level
        (LPBYTE)&ssStatus,             // address of structure
        sizeof(SERVICE_STATUS_PROCESS), // size of structure
        &dwBytesNeeded))              // size needed if buffer is too small
    {
        printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Check if the service is already running. It would be possible 
    // to stop the service here, but for simplicity this example just returns. 

    if (ssStatus.dwCurrentState != SERVICE_STOPPED && ssStatus.dwCurrentState != SERVICE_STOP_PENDING)
    {
        printf("Cannot start the service because it is already running\n");
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Save the tick count and initial checkpoint.

    dwStartTickCount = GetTickCount();
    dwOldCheckPoint = ssStatus.dwCheckPoint;

    // Wait for the service to stop before attempting to start it.

    while (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 

        dwWaitTime = ssStatus.dwWaitHint / 10;

        if (dwWaitTime < 1000)
            dwWaitTime = 1000;
        else if (dwWaitTime > 10000)
            dwWaitTime = 10000;

        Sleep(dwWaitTime);

        // Check the Status until the service is no longer stop pending. 

        if (!QueryServiceStatusEx(
            schService,                     // handle to service 
            SC_STATUS_PROCESS_INFO,         // information level
            (LPBYTE)&ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded))              // size needed if buffer is too small
        {
            printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return false;
        }

        if (ssStatus.dwCheckPoint > dwOldCheckPoint)
        {
            // Continue to wait and check.

            dwStartTickCount = GetTickCount();
            dwOldCheckPoint = ssStatus.dwCheckPoint;
        }
        else
        {
            if (GetTickCount() - dwStartTickCount > ssStatus.dwWaitHint)
            {
                printf("Timeout waiting for service to stop\n");
                CloseServiceHandle(schService);
                CloseServiceHandle(schSCManager);
                return false;
            }
        }
    }

    // Attempt to start the service.

    if (!StartService(
        schService,  // handle to service 
        0,           // number of arguments 
        NULL))      // no arguments 
    {
        printf("StartService failed (%d)\n", GetLastError());
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }
    else printf("Service start pending...\n");

    // Check the Status until the service is no longer start pending. 

    if (!QueryServiceStatusEx(
        schService,                     // handle to service 
        SC_STATUS_PROCESS_INFO,         // info level
        (LPBYTE)&ssStatus,             // address of structure
        sizeof(SERVICE_STATUS_PROCESS), // size of structure
        &dwBytesNeeded))              // if buffer too small
    {
        printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Save the tick count and initial checkpoint.

    dwStartTickCount = GetTickCount();
    dwOldCheckPoint = ssStatus.dwCheckPoint;

    while (ssStatus.dwCurrentState == SERVICE_START_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth the wait hint, but no less than 1 second and no 
        // more than 10 seconds. 

        dwWaitTime = ssStatus.dwWaitHint / 10;

        if (dwWaitTime < 1000)
            dwWaitTime = 1000;
        else if (dwWaitTime > 10000)
            dwWaitTime = 10000;

        Sleep(dwWaitTime);

        // Check the Status again. 

        if (!QueryServiceStatusEx(
            schService,             // handle to service 
            SC_STATUS_PROCESS_INFO, // info level
            (LPBYTE)&ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded))              // if buffer too small
        {
            printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
            break;
        }

        if (ssStatus.dwCheckPoint > dwOldCheckPoint)
        {
            // Continue to wait and check.

            dwStartTickCount = GetTickCount();
            dwOldCheckPoint = ssStatus.dwCheckPoint;
        }
        else
        {
            if (GetTickCount() - dwStartTickCount > ssStatus.dwWaitHint)
            {
                // No progress made within the wait hint.
                break;
            }
        }
    }

    // Determine whether the service is running.

    if (ssStatus.dwCurrentState == SERVICE_RUNNING)
    {
        printf("Service started successfully.\n");
    }
    else
    {
        printf("Service not started. \n");
        printf("  Current State: %d\n", ssStatus.dwCurrentState);
        printf("  Exit Code: %d\n", ssStatus.dwWin32ExitCode);
        printf("  Check Point: %d\n", ssStatus.dwCheckPoint);
        printf("  Wait Hint: %d\n", ssStatus.dwWaitHint);
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

bool ServiceInstaller::DoStopSvc(const ServiceBase& service)
{
    SERVICE_STATUS_PROCESS ssp;
    DWORD dwStartTime = GetTickCount();
    DWORD dwBytesNeeded;
    DWORD dwTimeout = 30000; // 30-second time-out
    DWORD dwWaitTime;

    // Get a handle to the SCM database. 

    auto schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return false;
    }

    // Get a handle to the service.

    auto schService = OpenService(
        schSCManager,         // SCM database 
        service.GetName().c_str(),            // name of service 
        SERVICE_STOP |
        SERVICE_QUERY_STATUS |
        SERVICE_ENUMERATE_DEPENDENTS);

    if (schService == NULL)
    {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return false;
    }

    // Make sure the service is not already stopped.

    if (!QueryServiceStatusEx(
        schService,
        SC_STATUS_PROCESS_INFO,
        (LPBYTE)&ssp,
        sizeof(SERVICE_STATUS_PROCESS),
        &dwBytesNeeded))
    {
        printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
        goto stop_cleanup;
    }

    if (ssp.dwCurrentState == SERVICE_STOPPED)
    {
        printf("Service is already stopped.\n");
        goto stop_cleanup;
    }

    // If a stop is pending, wait for it.

    while (ssp.dwCurrentState == SERVICE_STOP_PENDING)
    {
        printf("Service stop pending...\n");

        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 

        dwWaitTime = ssp.dwWaitHint / 10;

        if (dwWaitTime < 1000)
            dwWaitTime = 1000;
        else if (dwWaitTime > 10000)
            dwWaitTime = 10000;

        Sleep(dwWaitTime);

        if (!QueryServiceStatusEx(
            schService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp,
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded))
        {
            printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
            goto stop_cleanup;
        }

        if (ssp.dwCurrentState == SERVICE_STOPPED)
        {
            printf("Service stopped successfully.\n");
            goto stop_cleanup;
        }

        if (GetTickCount() - dwStartTime > dwTimeout)
        {
            printf("Service stop timed out.\n");
            goto stop_cleanup;
        }
    }

    // Send a stop code to the service.

    if (!ControlService(
        schService,
        SERVICE_CONTROL_STOP,
        (LPSERVICE_STATUS)&ssp))
    {
        printf("ControlService failed (%d)\n", GetLastError());
        goto stop_cleanup;
    }

    // Wait for the service to stop.

    while (ssp.dwCurrentState != SERVICE_STOPPED)
    {
        Sleep(ssp.dwWaitHint);
        if (!QueryServiceStatusEx(
            schService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp,
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded))
        {
            printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
            goto stop_cleanup;
        }

        if (ssp.dwCurrentState == SERVICE_STOPPED)
            break;

        if (GetTickCount() - dwStartTime > dwTimeout)
        {
            printf("Wait timed out\n");
            goto stop_cleanup;
        }
    }
    printf("Service stopped successfully\n");

stop_cleanup:
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}
