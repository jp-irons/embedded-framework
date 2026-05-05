#pragma once

#include "AppFileTable.hpp"
#include "TemperatureHandler.hpp"
#include "framework/EmbeddedFileHandler.hpp"
#include "core/FrameworkContext.hpp"

/**
 * ApplicationContext — the app-side counterpart to FrameworkContext.
 *
 * Responsibilities:
 *  - Own the app's embedded file table and file handler.
 *  - Register app routes and file handlers with the framework before start().
 *  - Set the entry point so the root path redirects to the app's UI.
 *  - Delegate start() / loop() calls.
 *
 * To add app-specific API handlers, declare them as members here, then
 * register them in start() via fw_.addRoute(...).
 */
class ApplicationContext {
  public:
    explicit ApplicationContext(framework::FrameworkContext &fw);
    ~ApplicationContext();

    void start();
    void loop();

  private:
    framework::FrameworkContext &fw_;

    // App embedded file table + handler.
    // appFileTable_ MUST be declared before appFileHandler_ so it is
    // initialised first (appFileHandler_ holds a reference to it).
    AppFileTable                         appFileTable_;
    embedded_files::EmbeddedFileHandler  appFileHandler_;

    TemperatureHandler temperatureHandler_;
};
