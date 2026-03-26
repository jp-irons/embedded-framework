#include "ProvisioningServer.hpp"
#include "WiFiManager.hpp"  // for calling back via ctx.manager if needed
#include "CredentialStore.hpp"
#include "cJSON.h"


namespace wifi_manager {

ProvisioningServer::ProvisioningServer(WiFiContext& ctx)
    : ctx(ctx)
{
}

void ProvisioningServer::start() {
    // start HTTP/DNS provisioning server here
}

void ProvisioningServer::stop() {
    // stop provisioning server here
}

void ProvisioningServer::handleCredentials(const char* ssid, const char* password)
{
    credential_store::WifiCredential cred;
    cred.ssid     = ssid;
    cred.password = password;
    cred.priority = 0;

    if (ctx.creds) {
        ctx.creds->add(cred);
    }

    if (ctx.manager) {
        ctx.manager->onProvisioningComplete();
    }
}

void ProvisioningServer::handleSubmitCredentials(httpd_req_t* req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return;
    }
    buf[len] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return;
    }

    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* password = cJSON_GetObjectItem(root, "password");

    if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid/password");
        return;
    }

    credential_store::WifiCredential cred;
    cred.ssid     = ssid->valuestring;
    cred.password = password->valuestring;
    cred.priority = 0;

    bool ok = ctx.creds->add(cred);
    cJSON_Delete(root);

    if (!ok) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save");
        return;
    }

    // Notify WiFiManager
    ctx.manager->onProvisioningComplete();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

} // namespace wifi_manager