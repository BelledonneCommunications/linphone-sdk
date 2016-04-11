#include "unregister.h"

using namespace std;

UnregisterCommand::UnregisterCommand() :
		DaemonCommand("unregister", "unregister <register_id|ALL>", "Unregister the daemon from the specified proxy or from all proxies.") {
	addExample(new DaemonCommandExample("unregister 3",
						"Status: Error\n"
						"Reason: No register with such id."));
	addExample(new DaemonCommandExample("unregister 2",
						"Status: Ok"));
	addExample(new DaemonCommandExample("unregister ALL",
						"Status: Ok"));
}

void UnregisterCommand::exec(Daemon *app, const char *args) {
	LinphoneProxyConfig *cfg = NULL;
	string param;
	int pid;

	istringstream ist(args);
	ist >> param;
	if (ist.fail()) {
		app->sendResponse(Response("Missing parameter.", Response::Error));
		return;
	}
	if (param.compare("ALL") == 0) {
		for (int i = 1; i <= app->maxProxyId(); i++) {
			cfg = app->findProxy(i);
			if (cfg != NULL) {
				linphone_core_remove_proxy_config(app->getCore(), cfg);
			}
		}
	} else {
		ist.clear();
		ist.str(param);
		ist >> pid;
		if (ist.fail()) {
			app->sendResponse(Response("Incorrect parameter.", Response::Error));
			return;
		}
		cfg = app->findProxy(pid);
		if (cfg == NULL) {
			app->sendResponse(Response("No register with such id.", Response::Error));
			return;
		}
		linphone_core_remove_proxy_config(app->getCore(), cfg);
	}
	app->sendResponse(Response());
}
