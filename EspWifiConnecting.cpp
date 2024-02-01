/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 30.01.2024

  Copyright (C) 2024-now Authors and www.dsp-crowd.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <esp_err.h>
#include <nvs_flash.h>

#include "EspWifiConnecting.h"

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StDependenciesInit) \
		gen(StMain) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 0
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

using namespace std;

#define LOG_LVL	0

const uint16_t cCntDelayMax = 1000;

EspWifiConnecting::EspWifiConnecting()
	: Processing("EspWifiConnecting")
	, mpNetInterface(NULL)
	, mpHostname("DSPC_ESP_WIFI")
	, mpSsid(NULL)
	, mpPassword(NULL)
	, mCntDelay(0)
	, mRssi(0)
{
	mState = StStart;
	mIpInfo.ip.addr = 0;
}

/* member functions */

Success EspWifiConnecting::process()
{
	//uint32_t curTimeMs = millis();
	//uint32_t diffMs = curTimeMs - mStartMs;
	Success success;
	esp_err_t res;
#if 0
	dStateTrace;
#endif
	switch (mState)
	{
	case StStart:

		if (!mpHostname)
			return procErrLog(-1, "Network hostname not set");

		if (!mpSsid)
			return procErrLog(-1, "WiFi SSID not set");

		if (!mpPassword)
			return procErrLog(-1, "WiFi password not set");

		mState = StDependenciesInit;

		break;
	case StDependenciesInit:

		res = nvs_flash_init();
		if (res != ESP_OK)
			return procErrLog(-1, "could not init NVS: %s (0x%04x)",
								esp_err_to_name(res), res);

		res = esp_netif_init();
		if (res != ESP_OK)
			return procErrLog(-1, "could not init network interface: %s (0x%04x)",
								esp_err_to_name(res), res);

		res = esp_event_loop_create_default();
		if (res != ESP_OK)
			return procErrLog(-1, "could not create event loop: %s (0x%04x)",
								esp_err_to_name(res), res);

		{
			esp_netif_config_t cfg = ESP_NETIF_DEFAULT_WIFI_STA();
			mpNetInterface = esp_netif_new(&cfg);
		}
		if (!mpNetInterface)
			return procErrLog(-1, "could not create WiFi STA interface");

		res = esp_netif_attach_wifi_station(mpNetInterface);
		if (res != ESP_OK)
			return procErrLog(-1, "could not attach network interface: %s (0x%04x)",
								esp_err_to_name(res), res);

		res = esp_wifi_set_default_wifi_sta_handlers();
		if (res != ESP_OK)
			return procErrLog(-1, "could not set WiFi STA handlers: %s (0x%04x)",
								esp_err_to_name(res), res);

		res = esp_netif_set_hostname(mpNetInterface, mpHostname);
		if (res != ESP_OK)
			return procErrLog(-1, "could not set hostname: %s (0x%04x)",
								esp_err_to_name(res), res);

		{
			wifi_init_config_t cfgWifiInit = WIFI_INIT_CONFIG_DEFAULT();
			res = esp_wifi_init(&cfgWifiInit);
		}
		if (res != ESP_OK)
			return procErrLog(-1, "could not init WiFi: %s (0x%04x)",
								esp_err_to_name(res), res);

		res = esp_wifi_set_mode(WIFI_MODE_STA);
		if (res != ESP_OK)
			return procErrLog(-1, "could not set WiFi mode: %s (0x%04x)",
								esp_err_to_name(res), res);

		res = esp_wifi_start();
		if (res != ESP_OK)
			return procErrLog(-1, "could not start WiFi: %s (0x%04x)",
								esp_err_to_name(res), res);

		success = wifiConfigure();
		if (success != Positive)
			return procErrLog(-1, "could not configure WiFi");

		res = esp_wifi_connect();
		if (res != ESP_OK)
			return procErrLog(-1, "could not connect WiFi: %s (0x%04x)",
								esp_err_to_name(res), res);

		mState = StMain;

		break;
	case StMain:

		++mCntDelay;
		if (mCntDelay < cCntDelayMax)
			break;
		mCntDelay = 0;

		res = esp_netif_get_ip_info(mpNetInterface, &mIpInfo);
		if (res != ESP_OK)
			break;

		{
			wifi_ap_record_t apRemoteInfo;
			res = esp_wifi_sta_get_ap_info(&apRemoteInfo);
			if (res != ESP_OK)
				break;

			mRssi = apRemoteInfo.rssi;
		}

		break;
	default:
		break;
	}

	return Pending;
}

Success EspWifiConnecting::wifiConfigure()
{
	wifi_config_t cfgWifi;
	esp_err_t res;

	res = esp_wifi_get_config(WIFI_IF_STA, &cfgWifi);
	if (res != ESP_OK)
		return procErrLog(-1, "could not get WiFi configuration: %s (0x%04x)",
							esp_err_to_name(res), res);

	strncpy((char *)cfgWifi.sta.ssid, mpSsid, sizeof(cfgWifi.sta.ssid));
	strncpy((char *)cfgWifi.sta.password, mpPassword, sizeof(cfgWifi.sta.password));

	cfgWifi.sta.failure_retry_cnt = 10;
	cfgWifi.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;

	res = esp_wifi_set_config(WIFI_IF_STA, &cfgWifi);
	if (res != ESP_OK)
		return procErrLog(-1, "could not set WiFi configuration: %s (0x%04x)",
							esp_err_to_name(res), res);

	return Positive;
}

void EspWifiConnecting::processInfo(char *pBuf, char *pBufEnd)
{
#if 1
	//dInfo("State\t\t\t%s\n", ProcStateString[mState]);
	dInfo("RSSI\t\t\t%ddBm\n", (int)mRssi);
#endif
}

/* static functions */

