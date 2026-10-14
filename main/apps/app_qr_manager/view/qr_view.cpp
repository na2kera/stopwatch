/*
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

#include <assets/assets.h>
#include <string>

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr int _panel_size    = 466;
constexpr int _qr_size       = 260;
constexpr int _qr_margin     = 12;
constexpr int _label_width   = 300;
constexpr int _label_pos_y   = -170;
constexpr int _counter_pos_y = 175;
constexpr int _hint_width    = 320;

constexpr uint32_t _label_color     = 0xFFFFFF;
constexpr uint32_t _counter_color   = 0x9A9A9A;
constexpr uint32_t _hint_text_color = 0xD9D9D9;

}  // namespace

void QrView::init(lv_obj_t* parent)
{
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_black());
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _label = std::make_unique<Label>(_panel->get());
    _label->setTextFont(&MontserratSemiBold26);
    _label->setTextColor(lv_color_hex(_label_color));
    _label->setWidth(_label_width);
    _label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _label->setLongMode(LV_LABEL_LONG_MODE_DOTS);
    _label->align(LV_ALIGN_CENTER, 0, _label_pos_y);

    // White panel behind the QR code as its quiet zone
    _qr_panel = std::make_unique<Container>(_panel->get());
    _qr_panel->align(LV_ALIGN_CENTER, 0, 0);
    _qr_panel->setSize(_qr_size + _qr_margin * 2, _qr_size + _qr_margin * 2);
    _qr_panel->setRadius(16);
    _qr_panel->setBorderWidth(0);
    _qr_panel->setPaddingAll(0);
    _qr_panel->setBgColor(lv_color_white());
    _qr_panel->setBgOpa(LV_OPA_COVER);
    _qr_panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _qrcode = std::make_unique<Qrcode>(_qr_panel->get());
    _qrcode->setSize(_qr_size);
    _qrcode->setDarkColor(lv_color_black());
    _qrcode->setLightColor(lv_color_white());
    _qrcode->align(LV_ALIGN_CENTER, 0, 0);

    _counter_label = std::make_unique<Label>(_panel->get());
    _counter_label->setTextFont(&lv_font_montserrat_24);
    _counter_label->setTextColor(lv_color_hex(_counter_color));
    _counter_label->align(LV_ALIGN_CENTER, 0, _counter_pos_y);

    _empty_hint_label = std::make_unique<Label>(_panel->get());
    _empty_hint_label->setText("No URL registered");
    _empty_hint_label->setTextFont(&lv_font_montserrat_24);
    _empty_hint_label->setTextColor(lv_color_hex(_hint_text_color));
    _empty_hint_label->setWidth(_hint_width);
    _empty_hint_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _empty_hint_label->align(LV_ALIGN_CENTER, 0, 0);
    _empty_hint_label->setHidden(true);
}

void QrView::showEntry(std::string_view label, std::string_view url, std::size_t index, std::size_t count)
{
    _label->setText(std::string(label));
    _qrcode->update(url);
    _counter_label->setText(std::to_string(index + 1) + " / " + std::to_string(count));

    _label->setHidden(false);
    _qr_panel->setHidden(false);
    _counter_label->setHidden(count <= 1);
    _empty_hint_label->setHidden(true);
}

void QrView::showEmpty()
{
    _label->setHidden(true);
    _qr_panel->setHidden(true);
    _counter_label->setHidden(true);
    _empty_hint_label->setHidden(false);
}
