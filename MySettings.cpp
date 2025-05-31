#include "MySettings.h"
#include "Global.h"

MySettings::MySettings()
	: QSettings(global->config_file_path, QSettings::IniFormat)
{
}

