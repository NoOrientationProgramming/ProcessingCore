
idf_component_register(
	SRCS
	"Processing.cpp"
	"Log.cpp"
	"SystemCommanding.cpp"
	"SystemDebugging.cpp"
	"TcpListening.cpp"
	"TcpTransfering.cpp"
	"EspWifiConnecting.cpp"
	INCLUDE_DIRS
	"."
	PRIV_REQUIRES
	"nvs_flash"
	"esp_wifi"
)

