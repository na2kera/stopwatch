/*
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <memory>
#include <smooth_lvgl.hpp>
#include <string_view>
#include <uitk/short_namespace.hpp>

namespace view {

class QrView {
public:
    void init(lv_obj_t* parent = lv_screen_active());
    void showEntry(std::string_view label, std::string_view url, std::size_t index, std::size_t count);
    void showEmpty();

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _label;
    std::unique_ptr<uitk::lvgl_cpp::Container> _qr_panel;
    std::unique_ptr<uitk::lvgl_cpp::Qrcode> _qrcode;
    std::unique_ptr<uitk::lvgl_cpp::Label> _counter_label;
    std::unique_ptr<uitk::lvgl_cpp::Label> _empty_hint_label;
};

}  // namespace view
