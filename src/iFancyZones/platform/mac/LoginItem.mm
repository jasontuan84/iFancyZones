#include "LoginItem.h"

#import <Foundation/Foundation.h>
#import <ServiceManagement/ServiceManagement.h>

#include <QDebug>

namespace ifz {

bool LoginItem::isEnabled()
{
    const SMAppServiceStatus st = SMAppService.mainAppService.status;
    return st == SMAppServiceStatusEnabled || st == SMAppServiceStatusRequiresApproval;
}

bool LoginItem::setEnabled(bool enabled)
{
    if (enabled == isEnabled()) return true;

    SMAppService *svc = SMAppService.mainAppService;
    NSError *err = nil;
    const BOOL ok = enabled ? [svc registerAndReturnError:&err]
                            : [svc unregisterAndReturnError:&err];
    if (!ok) {
        qWarning() << "[iFancyZones] launch-at-login" << (enabled ? "register" : "unregister")
                   << "failed:" << QString::fromNSString(err.localizedDescription);
        return false;
    }
    if (svc.status == SMAppServiceStatusRequiresApproval) {
        qInfo() << "[iFancyZones] launch-at-login needs approval in"
                << "System Settings > General > Login Items";
    }
    return true;
}

} // namespace ifz
