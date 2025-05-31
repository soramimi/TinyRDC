#include "MainWindow.h"
#include "Global.h"
#include <QApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include "joinpath.h"

ApplicationGlobal *global;

int main(int argc, char *argv[])
{
	ApplicationGlobal g;
	global = &g;

	global->organization_name = ORGANIZATION_NAME;
	global->application_name = APPLICATION_NAME;
	global->this_executive_program = QFileInfo(argv[0]).absoluteFilePath();
	global->generic_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	global->app_config_dir = global->generic_config_dir / global->organization_name / global->application_name;
	global->config_file_path = joinpath(global->app_config_dir, global->application_name + ".ini");

	QApplication a(argc, argv);
	MainWindow w;
	w.show();
	return a.exec();
}
