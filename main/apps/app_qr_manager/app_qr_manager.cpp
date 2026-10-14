/*
 * SPDX-License-Identifier: MIT
 */
#include "app_qr_manager.h"
#include <apps/common/loading_page/loading_page.h>
#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <smooth_lvgl.hpp>

using namespace mooncake;

AppQrManager::AppQrManager()
{
    setAppInfo().name = "QR Manager";
    setAppInfo().icon = (void*)&icon_qr_manager;
}

void AppQrManager::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppQrManager::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _current_index = 0;
    _key_manager   = std::make_unique<input::KeyManager>();

    LvglLockGuard lock;

    _qr_view = std::make_unique<view::QrView>();
    _qr_view->init(lv_screen_active());
    reload_entries();
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

    LvglLockGuard lock;

    if (!_entries.empty()) {
        if (event == input::KeyEvent::GoPrevious) {
            _current_index = (_current_index + _entries.size() - 1) % _entries.size();
            show_current_entry();
        } else if (event == input::KeyEvent::GoNext) {
            _current_index = (_current_index + 1) % _entries.size();
            show_current_entry();
        }
    }

    _qr_view->update();

    if (_qr_view->consumeEditRequested()) {
        mclog::info("start qr edit mode");
        auto loading_page = std::make_unique<view::LoadingPage>(0x2B2B2B, 0xFFFFFF);
        GetHAL().lvglUnlock();
        GetHAL().startQrEditModeViaAp([&](std::string_view msg) {
            LvglLockGuard lock;
            loading_page->setMessage(msg);
        });
        GetHAL().lvglLock();

        loading_page.reset();
        reload_entries();
    }
}

void AppQrManager::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;

    _qr_view.reset();
}

void AppQrManager::reload_entries()
{
    _entries = GetHAL().loadQrEntries();
    mclog::tagInfo(getAppInfo().name, "loaded {} entries", _entries.size());
    if (_current_index >= _entries.size()) {
        _current_index = 0;
    }
    show_current_entry();
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
