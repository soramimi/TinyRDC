#ifndef GLOBAL_H
#define GLOBAL_H

#include <QString>

#define ORGANIZATION_NAME "soramimi.jp"
#define APPLICATION_NAME "TonyRDC"

class MainWindow;

class ApplicationSettings {
public:
};

class ApplicationBasicData {
public:
	QString organization_name = ORGANIZATION_NAME;
	QString application_name = APPLICATION_NAME;
	QString this_executive_program;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;
};

class ApplicationGlobal : public ApplicationBasicData {
public:
	MainWindow *mainwindow = nullptr;
};

extern ApplicationGlobal *global;

#endif // GLOBAL_H
