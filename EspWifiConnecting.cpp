/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 30.01.2024

  Copyright (C) 2024, Johannes Natter

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

#include <chrono>
#include <esp_err.h>
#include <nvs_flash.h>

#include "EspWifiConnecting.h"

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StDependenciesInit) \
		gen(StConfigure) \
		gen(StConnect) \
		gen(StConnectedWait) \
		gen(StIfUpWait) \
		gen(StMain) \
		gen(StDisconnect) \
		gen(StDisconnectedWait) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 0
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

using namespace std;
using namespace chrono;

const uint32_t cUpdateDelayMs = 200;
const uint32_t cIfUpWaitTmoMs = 5000;

bool EspWifiConnecting::mConnected = false;

EspWifiConnecting::EspWifiConnecting()
	: Processing("EspWifiConnecting")
	, mStartMs(0)
	, mpNetInterface(NULL)
	, mpHostname("DSPC_ESP_WIFI")
	, mpSsid(NULL)
	, mpPassword(NULL)
	, mWifiConnected(false)
	, mRssi(0)
{
	mState = StStart;
	mIpInfo.ip.addr = 0;
}

/* member functions */

Success EspWifiConnecting::process()
{
	uint32_t curTimeMs = millis();
	uint32_t diffMs = curTimeMs - mStartMs;
	Success success;
	esp_err_t res;
	bool ok;
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

		mState = StConfigure;

		break;
	case StConfigure:

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

		mState = StConnect;

		break;
	case StConnect:

		res = esp_wifi_connect();
		if (res != ESP_OK)
			return procErrLog(-1, "could not connect WiFi: %s (0x%04x)",
								esp_err_to_name(res), res);

		mStartMs = curTimeMs;
		mState = StConnectedWait;

		break;
	case StConnectedWait:
#if 0 // Do not spam logs. Use network interface up as connected indicator
		infoWifiUpdate();
		if (!mWifiConnected)
			break;

		procDbgLog("WiFi connected");
#endif
		mState = StIfUpWait;

		break;
	case StIfUpWait:

		if (diffMs > cIfUpWaitTmoMs)
		{
			//procDbgLog("Timeout reached for interface up");
			mState = StConnect;
			break;
		}

		ok = esp_netif_is_netif_up(mpNetInterface);
		if (!ok)
			break;

		// Interface must be up in order to get current IP info
		res = esp_netif_get_ip_info(mpNetInterface, &mIpInfo);
		if (res != ESP_OK)
			break;

		if (!mIpInfo.ip.addr)
			break;

		procDbgLog("Interface up. IP: " IPSTR, IP2STR(&mIpInfo.ip));

		mConnected = true;

		mStartMs = curTimeMs;
		mState = StMain;

		break;
	case StMain:

		if (diffMs < cUpdateDelayMs)
			break;
		mStartMs = curTimeMs;

		infoWifiUpdate();
		if (mWifiConnected)
			break;

		mState = StDisconnect;

		break;
	case StDisconnect:

		res = esp_wifi_disconnect();
		if (res != ESP_OK)
			procErrLog(-1, "could not disconnect WiFi: %s (0x%04x)",
								esp_err_to_name(res), res);

		res = esp_wifi_stop();
		if (res != ESP_OK)
			procErrLog(-1, "could not stop WiFi: %s (0x%04x)",
								esp_err_to_name(res), res);

		mConnected = false;

		mState = StDisconnectedWait;

		break;
	case StDisconnectedWait:

		infoWifiUpdate();
		if (mWifiConnected)
			break;

		procDbgLog("WiFi disconnected");

		mState = StConfigure;

		break;
	default:
		break;
	}

	return Pending;
}

void EspWifiConnecting::infoWifiUpdate()
{
	wifi_ap_record_t apRemoteInfo;
	esp_err_t res;

	res = esp_wifi_sta_get_ap_info(&apRemoteInfo);
	if (res == ESP_ERR_WIFI_CONN) // not initialized
		return;

	if (res == ESP_ERR_WIFI_NOT_CONNECT)
	{
		mWifiConnected = false;
		return;
	}

	mWifiConnected = true;
	mRssi = apRemoteInfo.rssi;
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

	cfgWifi.sta.failure_retry_cnt = 0;
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

bool EspWifiConnecting::ok()
{
	return mConnected;
}

uint32_t EspWifiConnecting::millis()
{
	auto now = steady_clock::now();
	auto nowMs = time_point_cast<milliseconds>(now);
	return (uint32_t)nowMs.time_since_epoch().count();
}

