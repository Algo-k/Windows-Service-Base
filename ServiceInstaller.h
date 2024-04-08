#ifndef SERVICE_INSTALLER_H_
#define SERVICE_INSTALLER_H_

#include "Service_Base.h"

class ServiceInstaller {
public:
	static bool Install(const ServiceBase& service);
	static bool Uninstall(const ServiceBase& service);
	static bool DoStartSvc(const ServiceBase& service);
	static bool DoStopSvc(const ServiceBase& service);
	static bool AutoRestart(const ServiceBase& service);
private:
	ServiceInstaller() {}
};

#endif // SERVICE_INSTALLER_H_

