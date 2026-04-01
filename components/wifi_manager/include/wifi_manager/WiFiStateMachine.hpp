#pragma once

#include "credential_store/CredentialStore.hpp"
#include "esp_netif_types.h"
#include "WiFiTypes.hpp"

//namespace credential_store {
//class CredentialStore;
//struct WiFiCredential;
//}
//
namespace wifi_manager {

class WiFiInterface;
struct WiFiContext;

class WiFiStateMachine {
  public:
    WiFiStateMachine(WiFiContext &ctx);

    void start();

    // Driver lifecycle
    void onDriverStarted();
    void onDriverStopped();

    // AP events
    void onApStarted();
    void onApStopped();

    // STA events
    void onStaConnecting();
    void onStaConnected();
    void onStaGotIp(const ip_event_got_ip_t *ip);
    void onStaDisconnected(WiFiError reason);

    // Provisioning events
    void onProvisioningRequestReceived();
    void onProvisioningCredentialsReceived(const credential_store::WiFiCredential &creds);
    void onProvisioningTestResult(bool success);

    // Errors
    void onError(WiFiError error);
    void startRuntime();
    void reset();

	WiFiState getState() const;
	size_t getCredentialIndex() const;
	std::string getCurrentSSID() const;
	
  private:
    void transitionTo(WiFiState newState);

  private:
    WiFiContext &ctx;
	WiFiInterface* wifi = nullptr;

    WiFiState currentState = WiFiState::UNINITIALISED;
    size_t currentCredentialIndex = 0;
	credential_store::WiFiCredential* currentCredential = nullptr;

    credential_store::WiFiCredential getCredential(size_t index) const;
	
	void enterState(WiFiState newState);
	void tryNextCredential();
	void startProvisioningAp();
	void startProvisioningTestSta();
	void startRuntimeSta();

};

} // namespace wifi_manager