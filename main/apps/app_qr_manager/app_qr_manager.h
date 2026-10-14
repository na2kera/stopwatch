/*
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <memory>
#include <vector>

/**
 * @brief Show registered URLs as QR codes
 *
 */
class AppQrManager : public mooncake::AppAbility {
public:
    AppQrManager();

    // Override lifecycle callbacks
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    void reload_entries();
    void show_current_entry();

    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::QrView> _qr_view;
    std::vector<QrEntry> _entries;
    std::size_t _current_index = 0;
};
