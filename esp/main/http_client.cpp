#include "http_client.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <cstring>

static const char *TAG = "http_client";

static constexpr int TIMEOUT_MS = 10000;
static constexpr int MAX_CONSECUTIVE_FAILURES = 3;

class EspHttpClient : public HttpClient {
  public:
    EspHttpClient()
    {
        buffer_ = static_cast<char *>(heap_caps_malloc(HTTP_RESPONSE_BUFFER_SIZE, MALLOC_CAP_SPIRAM));
        if (!buffer_) {
            ESP_LOGE(TAG, "Failed to allocate HTTP buffer in SPIRAM");
            abort();
        }
        ESP_LOGI(TAG, "Allocated %zu KB HTTP buffer in SPIRAM", HTTP_RESPONSE_BUFFER_SIZE / 1024);
    }

    ~EspHttpClient() override
    {
        if (client_) {
            esp_http_client_cleanup(client_);
        }
        heap_caps_free(buffer_);
    }

    esp_err_t get(const std::string &url, std::string_view &response) override
    {
        response = {};

        // Reset buffer state
        buffer_offset_ = 0;
        oversized_ = false;

        if (!client_) {
            if (!initClient()) {
                consecutive_failures_++;
                return ESP_FAIL;
            }
        }
        esp_http_client_set_url(client_, url.c_str());

        esp_err_t err = esp_http_client_perform(client_);
        int status = esp_http_client_get_status_code(client_);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
            handleFailure();
            return oversized_ ? ESP_ERR_INVALID_SIZE : err;
        }

        if (status != 200) {
            ESP_LOGE(TAG, "HTTP status %d", status);
            handleFailure();
            return ESP_FAIL;
        }

        consecutive_failures_ = 0;
        buffer_[buffer_offset_] = '\0';
        response = std::string_view(buffer_, buffer_offset_);

        ESP_LOGI(TAG, "HTTP GET %d bytes from %s", buffer_offset_, url.c_str());
        return ESP_OK;
    }

  private:
    bool initClient()
    {
        esp_http_client_config_t config = {};
        config.url = "https://localhost"; // Placeholder; real URL set via set_url()
        config.event_handler = eventHandler;
        config.user_data = this;
        config.timeout_ms = TIMEOUT_MS;
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.buffer_size = 2048;
        config.buffer_size_tx = 1024;
        config.keep_alive_enable = true;

        client_ = esp_http_client_init(&config);
        if (!client_) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            return false;
        }
        ESP_LOGI(TAG, "Initialized HTTP client");
        return true;
    }

    void handleFailure()
    {
        consecutive_failures_++;
        if (consecutive_failures_ >= MAX_CONSECUTIVE_FAILURES) {
            ESP_LOGW(TAG, "Resetting HTTP client after %d consecutive failures", consecutive_failures_);
            if (client_) {
                esp_http_client_cleanup(client_);
                client_ = nullptr;
            }
            consecutive_failures_ = 0;
        }
    }

    static esp_err_t eventHandler(esp_http_client_event_t *evt)
    {
        if (evt->event_id != HTTP_EVENT_ON_DATA || !evt->data || evt->data_len <= 0) {
            return ESP_OK;
        }

        auto *self = static_cast<EspHttpClient *>(evt->user_data);

        if (self->buffer_offset_ + evt->data_len + 1 > HTTP_RESPONSE_BUFFER_SIZE) {
            self->oversized_ = true;
            ESP_LOGW(TAG, "Response exceeded buffer (%zu bytes)", HTTP_RESPONSE_BUFFER_SIZE);
            return ESP_FAIL;
        }

        memcpy(self->buffer_ + self->buffer_offset_, evt->data, evt->data_len);
        self->buffer_offset_ += evt->data_len;
        return ESP_OK;
    }

    char *buffer_ = nullptr;
    int buffer_offset_ = 0;
    bool oversized_ = false;
    esp_http_client_handle_t client_ = nullptr;
    int consecutive_failures_ = 0;
};

HttpClient &HttpClient::instance()
{
    static EspHttpClient instance;
    return instance;
}
