/*
 * SPDX-License-Identifier: MIT
 */
#include "app_qr_manager.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <smooth_lvgl.hpp>

using namespace mooncake;

AppQrManager::AppQrManager()
{
    setAppInfo().name = "QR Manager";
}

void AppQrManager::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppQrManager::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    // TODO: Load entries registered from the phone instead of hardcoded ones
    _entries = {
        {"GitHub", "https://github.com/na2kera"},
        {"M5Stack", "https://m5stack.com"},
        {"StopWatch Docs", "https://docs.m5stack.com/en/core/StopWatch"},
    };
    _current_index = 0;

    _key_manager = std::make_unique<input::KeyManager>();

    LvglLockGuard lock;

    _qr_view = std::make_unique<view::QrView>();
    _qr_view->init(lv_screen_active());
    show_current_entry();
}

void AppQrManager::onRunning()
{
    input::KeyEvent event = input::KeyEvent::None;
    if (_key_manager) {
        event = _key_manager->update();
    }

    if (event == input::KeyEvent::GoHome) {
        close();
        return;
    }

    if (_entries.empty()) {
        return;
    }

    if (event == input::KeyEvent::GoPrevious) {
        _current_index = (_current_index + _entries.size() - 1) % _entries.size();
    } else if (event == input::KeyEvent::GoNext) {
        _current_index = (_current_index + 1) % _entries.size();
    } else {
        return;
    }

    LvglLockGuard lock;
    show_current_entry();
}

void AppQrManager::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;

    _qr_view.reset();
}

void AppQrManager::show_current_entry()
{
    if (_entries.empty()) {
        _qr_view->showEmpty();
        return;
    }

    const auto& entry = _entries[_current_index];
    _qr_view->showEntry(entry.label, entry.url, _current_index, _entries.size());
}
