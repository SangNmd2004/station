/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group; // nhóm sự kiện FreeRTOS, theo dõi các trạng thái kết nối Wifi

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;


static void event_handler(void* arg, esp_event_base_t event_base, // hàm xử lí sự kiện
                                int32_t event_id, void* event_data) //chuỗi định danh, id của sự kiện, thông tin và dữ liệu của sự kiện
{
    /*WIFI_EVENT_STA_START là sự kiện chế độ station khởi tạo, nó cố gắng kết nối với AP
  WIFI_EVENT_STA_DISCONNECTED là sự kiện khi bị ngắt kết nối, nó cố gắng thử lại cho đến khi đạt số lần thử tối đa
  -nếu không thành công thì đặt bit WIFI_FAIL_BIT
  IP_EVENT_STA_GOT_IP là sự kiện khi WiFi Station nhận được địa chỉ IP từ Access Point (AP), kết nối thành công
  và đặt bit thành WIFI_CONNECTED_BIT*/
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);//nếu số lần reconnect đến giới hạn thì đặt WIFI_FAIL_BIT
        }// hàm này được cài đặt sẵn trong FreeRTOS
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;// ép kiểu con trỏ
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        //IP2STR là marco để tách từng byte từ địa chỉ IP để biểu diễn thông qua IPSTR (%d.%d.%d.%d)
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) // hàm khởi tạo wifi ở chế độ Station, cho phép ESP32 kết nối đến 1 AP
{
    s_wifi_event_group = xEventGroupCreate();// tạo các nhóm sự kiện

    ESP_ERROR_CHECK(esp_netif_init()); // khởi tạo giao diện để ESP32 có thể sử dụng chức năng mạng

    ESP_ERROR_CHECK(esp_event_loop_create_default());// vòng lặp quản lí sự kiện
    esp_netif_create_default_wifi_sta();// khởi tạo giao diện wifi cho chế độ Station(ESP32 như là 1 Staion)

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();// marco tạo một cấu hình mặc định cho wifi (bộ nhớ, chế độ và callback)
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));//khởi tạo wifi với chế độ cfg

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
        // đăng kí hàm xử lí sự kiên event_handler cho một nhóm sự kiệnWIFI_EVENT: Nhóm sự kiện liên quan đến WiFi.
        //ESP_EVENT_ANY_ID: Đăng ký tất cả các loại sự kiện trong nhóm này.
        //&event_handler: tham chiếu tới hàm xử lý sự kiện.
        //NULL: Không truyền thêm dữ liệu (context) cho hàm xử lý sự kiện.
        //&instance_any_id: Con trỏ lưu thông tin instance của sự kiện để quản lý sau này (nếu cần)                                                
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));// tương tự
        // IP_EVENT_STA_GOT_IP để báo rằng ESP32 nhận được địa chỉ IP
    wifi_config_t wifi_config = { //cấu trúc dùng để cấu hình kết nối wifi
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,//tên
            .password = EXAMPLE_ESP_WIFI_PASS,//mật khẩu
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,// mức độ xác thực
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,//sae mode của WPA3
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );// chọn chế độ STA
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );// cấu hình cho chế độ STA
    ESP_ERROR_CHECK(esp_wifi_start() ); //bắt đầu quá trình

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, // hàm chờ sự kiện để kích hoạt
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,//không xóa các bit đã được kích hoạt sau khi hàm trả về
            pdFALSE,// không yêu cầu cả 2 bit được kích hoạt, chờ 1 trong 2
            portMAX_DELAY);// đợi mãi mãi cho đến khi có bit được kích hoạt

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}// các trường hợp khi kết nối

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();// khởi tạo NVS, bộ nhớ không mất dữ liệu khi mất nguồn
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }// nếu bộ nhớ đầy hoặc có phiên bản mới, xóa đi khởi tạo lại
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
}
