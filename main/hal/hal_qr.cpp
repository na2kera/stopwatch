/*
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "utils/config_ap/config_ap.h"

#include <ArduinoJson.h>
#include <cerrno>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include <mooncake_log.h>

static const std::string_view _tag = "HAL-QR";

extern const char qr_config_ap_html_start[] asm("_binary_qr_config_ap_html_start");
extern const char qr_config_ap_html_end[] asm("_binary_qr_config_ap_html_end");

namespace {

constexpr const char* _qr_dir                = "/spiflash/qr";
constexpr const char* _entries_file          = "/spiflash/qr/entries.json";
constexpr const char* _entries_tmp_file      = "/spiflash/qr/entries.json.tmp";
constexpr std::size_t _max_request_size_byte = 16 * 1024;

bool ensure_qr_dir()
{
    if (mkdir(_qr_dir, 0775) == 0 || errno == EEXIST) {
        return true;
    }

    mclog::tagError(_tag, "failed to create qr dir: {}, errno={}", _qr_dir, errno);
    return false;
}

// Display fonts only cover ASCII, and URLs are expected to be percent-encoded already
bool is_printable_ascii(std::string_view text, bool allowSpace)
{
    for (char c : text) {
        if (c < (allowSpace ? 0x20 : 0x21) || c > 0x7E) {
            return false;
        }
    }
    return true;
}

bool validate_entry(const QrEntry& entry, std::string& message)
{
    if (entry.label.empty() || entry.label.size() > QrEntry::maxLabelLength) {
        message = "Label must be 1-" + std::to_string(QrEntry::maxLabelLength) + " characters";
        return false;
    }
    if (!is_printable_ascii(entry.label, true)) {
        message = "Label must be ASCII characters";
        return false;
    }
    if (entry.url.empty() || entry.url.size() > QrEntry::maxUrlLength) {
        message = "URL must be 1-" + std::to_string(QrEntry::maxUrlLength) + " characters";
        return false;
    }
    if (!is_printable_ascii(entry.url, false)) {
        message = "URL must be ASCII characters without spaces";
        return false;
    }
    return true;
}

bool parse_entries(std::string_view json, std::vector<QrEntry>& entries, std::string& message)
{
    JsonDocument doc;
    if (deserializeJson(doc, json.data(), json.size()) != DeserializationError::Ok) {
        message = "Invalid JSON";
        return false;
    }

    JsonArrayConst array = doc["entries"].as<JsonArrayConst>();
    if (array.isNull()) {
        message = "Missing entries";
        return false;
    }
    if (array.size() > QrEntry::maxEntryCount) {
        message = "Up to " + std::to_string(QrEntry::maxEntryCount) + " entries";
        return false;
    }

    entries.clear();
    for (JsonObjectConst item : array) {
        QrEntry entry;
        entry.label = item["label"] | "";
        entry.url   = item["url"] | "";
        if (!validate_entry(entry, message)) {
            return false;
        }
        entries.push_back(std::move(entry));
    }
    return true;
}

std::string make_entries_json(const std::vector<QrEntry>& entries)
{
    JsonDocument doc;
    JsonArray array = doc["entries"].to<JsonArray>();
    for (const auto& entry : entries) {
        JsonObject item = array.add<JsonObject>();
        item["label"]   = entry.label;
        item["url"]     = entry.url;
    }

    std::string json;
    serializeJson(doc, json);
    return json;
}

bool read_entries_file(std::vector<QrEntry>& entries)
{
    FILE* file = fopen(_entries_file, "rb");
    if (file == nullptr) {
        return false;
    }

    std::string json;
    char buffer[512];
    std::size_t read_size = 0;
    while ((read_size = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        json.append(buffer, read_size);
    }
    fclose(file);

    std::string message;
    if (!parse_entries(json, entries, message)) {
        mclog::tagError(_tag, "failed to parse entries file: {}", message);
        return false;
    }
    return true;
}

bool write_entries_file(const std::vector<QrEntry>& entries)
{
    if (!ensure_qr_dir()) {
        return false;
    }

    const std::string json = make_entries_json(entries);

    FILE* file = fopen(_entries_tmp_file, "wb");
    if (file == nullptr) {
        mclog::tagError(_tag, "failed to open file for write: {}", _entries_tmp_file);
        return false;
    }
    const bool written = fwrite(json.data(), 1, json.size(), file) == json.size();
    fclose(file);
    if (!written) {
        mclog::tagError(_tag, "failed to write file: {}", _entries_tmp_file);
        unlink(_entries_tmp_file);
        return false;
    }

    // FAT rename fails if the destination exists
    unlink(_entries_file);
    if (rename(_entries_tmp_file, _entries_file) != 0) {
        mclog::tagError(_tag, "failed to rename entries file, errno={}", errno);
        return false;
    }
    return true;
}

void send_json(httpd_req_t* req, const std::string& body)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, body.c_str(), body.size());
}

esp_err_t handle_get_entries(httpd_req_t* req)
{
    std::vector<QrEntry> entries;
    read_entries_file(entries);

    JsonDocument doc;
    doc["apSsid"]         = config_ap::ap_ssid();
    doc["maxEntryCount"]  = QrEntry::maxEntryCount;
    doc["maxLabelLength"] = QrEntry::maxLabelLength;
    doc["maxUrlLength"]   = QrEntry::maxUrlLength;
    JsonArray array       = doc["entries"].to<JsonArray>();
    for (const auto& entry : entries) {
        JsonObject item = array.add<JsonObject>();
        item["label"]   = entry.label;
        item["url"]     = entry.url;
    }

    std::string json;
    serializeJson(doc, json);
    send_json(req, json);
    return ESP_OK;
}

esp_err_t handle_save_entries(httpd_req_t* req)
{
    if (req->content_len <= 0 || static_cast<std::size_t>(req->content_len) > _max_request_size_byte) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid request size");
        return ESP_FAIL;
    }

    std::string body(static_cast<std::size_t>(req->content_len), '\0');
    std::size_t offset = 0;
    while (offset < body.size()) {
        const int received = httpd_req_recv(req, body.data() + offset, body.size() - offset);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to read request body");
            return ESP_FAIL;
        }
        offset += static_cast<std::size_t>(received);
    }

    std::vector<QrEntry> entries;
    std::string message;
    if (!parse_entries(body, entries, message)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, message.c_str());
        return ESP_FAIL;
    }

    if (!write_entries_file(entries)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to save entries");
        return ESP_FAIL;
    }

    mclog::tagInfo(_tag, "saved {} entries", entries.size());
    send_json(req, "{\"status\":\"ok\",\"message\":\"saved\"}");
    return ESP_OK;
}

}  // namespace

std::vector<QrEntry> Hal::loadQrEntries()
{
    std::vector<QrEntry> entries;
    if (!read_entries_file(entries)) {
        entries.clear();
    }
    return entries;
}

void Hal::startQrEditModeViaAp(std::function<void(std::string_view)> onLog)
{
    mclog::tagInfo(_tag, "start qr edit mode via AP");

    if (!ensure_qr_dir()) {
        if (onLog) {
            onLog("Failed to create QR storage directory");
        }
        return;
    }

    config_ap::Site site;
    site.indexHtml = std::string_view(qr_config_ap_html_start, qr_config_ap_html_end - qr_config_ap_html_start);
    site.routes    = {
        {"/qr/entries", HTTP_GET, handle_get_entries},
        {"/qr/entries", HTTP_POST, handle_save_entries},
    };

    config_ap::run(onLog, site);
}
