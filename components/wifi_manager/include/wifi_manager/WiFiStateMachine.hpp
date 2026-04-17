#pragma once

#include "device/DeferredExecutor.hpp"
#include "wifi_types/WiFiTypes.hpp"
#include "esp_netif_types.h"

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
	void onUnexpectedEvent(const char* eventName);

    void onDriverStarted();
    void onDriverStopped();

    // AP events
    void onApStarted();
    void onApStopped();

    // STA events
    void onStaConnecting();
    void onStaConnected();
    void onStaGotIp(const ip_event_got_ip_t *ip);
    void onStaDisconnected(wifi_types::WiFiError reason);

    // Provisioning events
    void onProvisioningRequestReceived();
    void onProvisioningCredentialsReceived(const wifi_types::WiFiCredential &creds);
    void onProvisioningTestResult(bool success);

    // Errors
    void onError(wifi_types::WiFiError error);
    void startRuntime();
    void reset();

	wifi_types::WiFiState getState() const;
	size_t getCredentialIndex() const;
	std::string getCurrentSSID() const;
	
  private:
    WiFiContext &ctx;
	WiFiInterface* wifi = nullptr;
	device::DeferredExecutor defer;

    wifi_types::WiFiState currentState = wifi_types::WiFiState::UNINITIALISED;
	wifi_types::WiFiError error = wifi_types::WiFiError::NONE;
	
    size_t currentCredentialIndex = 0;
	wifi_types::WiFiCredential* currentCredential = nullptr;
	
	int retryCount = 0;
	static constexpr int MAX_RETRIES = 3;

    wifi_types::WiFiCredential getCredential(size_t index) const;
	
	void transitionTo(wifi_types::WiFiState newState);
    void enterState(wifi_types::WiFiState newState);
	void tryNextCredential();
	void startProvisioningAp();
	void startProvisioningTestSta();
	void startRuntimeSta();

};

} // namespace wifi_manager