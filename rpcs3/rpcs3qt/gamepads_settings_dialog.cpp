#include "gamepads_settings_dialog.h"
#include "pad_settings_dialog.h"
#include "Emu/IdManager.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QInputDialog>
#include <QMessageBox>

inline std::string sstr(const QString& _in) { return _in.toStdString(); }
constexpr auto qstr = QString::fromStdString;

inline bool CreateConfigFile(const QString& dir, const QString& name)
{
	QString input_dir = qstr(fs::get_config_dir()) + "/InputConfigs/";
	if (!QDir().mkdir(input_dir) && !QDir().exists(input_dir))
	{
		LOG_ERROR(GENERAL, "Failed to create dir %s", sstr(input_dir));
		return false;
	}
	if (!QDir().mkdir(dir) && !QDir().exists(dir))
	{
		LOG_ERROR(GENERAL, "Failed to create dir %s", sstr(dir));
		return false;
	}

	QString filename = dir + name + ".yml";
	QFile new_file(filename);

	if (!new_file.open(QIODevice::WriteOnly))
	{
		LOG_ERROR(GENERAL, "Failed to create file %s", sstr(filename));
		return false;
	}

	new_file.close();
	return true;
};

// taken from https://stackoverflow.com/a/30818424/8353754
// because size policies won't work as expected (see similar bugs in Qt bugtracker)
inline void resizeComboBoxView(QComboBox* combo)
{
	int max_width = 0;
	QFontMetrics fm(combo->font());
	for (int i = 0; i < combo->count(); ++i)
	{
		int width = fm.width(combo->itemText(i));
		if (width > max_width) max_width = width;
	}
	if (combo->view()->minimumWidth() < max_width)
	{
		// add scrollbar width and margin
		max_width += combo->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
		max_width += combo->view()->autoScrollMargin();
		combo->view()->setMinimumWidth(max_width);
	}
};

gamepads_settings_dialog::gamepads_settings_dialog(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Gamepads Settings"));

	// read tooltips from json
	QFile json_file(":/Json/tooltips.json");
	json_file.open(QIODevice::ReadOnly | QIODevice::Text);
	QJsonObject json_input = QJsonDocument::fromJson(json_file.readAll()).object().value("input").toObject();
	json_file.close();

	QVBoxLayout *dialog_layout = new QVBoxLayout();
	QHBoxLayout *all_players = new QHBoxLayout();

	pt = fxm::get<PadThread>();
	if (!pt)
		pt = fxm::import<PadThread>(Emu.GetCallbacks().get_pad_handler);

	if (!pt)
		LOG_ERROR(GENERAL, "gamepads_settings_dialog: Unable to get pad_thread");

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		QGroupBox *grp_player = new QGroupBox(QString(tr("Player %1").arg(i + 1)));

		QVBoxLayout *ppad_layout = new QVBoxLayout();

		co_inputtype[i] = new QComboBox();
		co_inputtype[i]->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
		co_inputtype[i]->view()->setTextElideMode(Qt::ElideNone);
#ifdef WIN32
		co_inputtype[i]->setToolTip(json_input["padHandlerBox"].toString());
#else
		co_inputtype[i]->setToolTip(json_input["padHandlerBox_Linux"].toString());
#endif
		ppad_layout->addWidget(co_inputtype[i]);

		co_deviceID[i] = new QComboBox();
		co_deviceID[i]->setEnabled(false);
		co_deviceID[i]->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
		co_deviceID[i]->view()->setTextElideMode(Qt::ElideNone);
		ppad_layout->addWidget(co_deviceID[i]);

		co_profile[i] = new QComboBox();
		co_profile[i]->setEnabled(false);
		co_profile[i]->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
		co_profile[i]->view()->setTextElideMode(Qt::ElideNone);
		ppad_layout->addWidget(co_profile[i]);

		QHBoxLayout *button_layout = new QHBoxLayout();
		bu_new_profile[i] = new QPushButton(tr("Add Profile"));
		bu_new_profile[i]->setEnabled(false);
		bu_config[i] = new QPushButton(tr("Configure"));
		bu_config[i]->setEnabled(false);
		button_layout->setContentsMargins(0, 0, 0, 0);
		button_layout->addWidget(bu_config[i]);
		button_layout->addWidget(bu_new_profile[i]);
		ppad_layout->addLayout(button_layout);

		grp_player->setLayout(ppad_layout);
		grp_player->setFixedSize(grp_player->sizeHint());

		// fill comboboxes after setting the groupbox's size to prevent stretch
		std::vector<std::string> str_inputs = g_cfg_input.player[0]->handler.to_list();
		for (int index = 0; index < str_inputs.size(); index++)
		{
			co_inputtype[i]->addItem(qstr(str_inputs[index]), QVariant(index));
		}
		resizeComboBoxView(co_inputtype[i]);

		all_players->addWidget(grp_player);

		if (i == 3)
		{
			dialog_layout->addLayout(all_players);
			all_players = new QHBoxLayout();
			all_players->addStretch();
		}
	}

	all_players->addStretch();
	dialog_layout->addLayout(all_players);

	QHBoxLayout *buttons_layout = new QHBoxLayout();
	QPushButton *ok_button = new QPushButton(tr("OK"));
	QPushButton *cancel_button = new QPushButton(tr("Cancel"));
	QPushButton *refresh_button = new QPushButton(tr("Refresh"));
	buttons_layout->addWidget(ok_button);
	buttons_layout->addWidget(refresh_button);
	buttons_layout->addWidget(cancel_button);
	buttons_layout->addStretch();
	dialog_layout->addLayout(buttons_layout);

	setLayout(dialog_layout);
	layout()->setSizeConstraint(QLayout::SetFixedSize);

	//Set the values from config
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		// No extra loops are necessary because setCurrentText does it for us
		co_inputtype[i]->setCurrentText(qstr(g_cfg_input.player[i]->handler.to_string()));
		// Device will still be empty on Null handler, so fill them by force
		ChangeInputType(i);
	}

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		connect(co_inputtype[i], &QComboBox::currentTextChanged, [=]
		{
			ChangeInputType(i);
		});

		connect(bu_config[i], &QAbstractButton::clicked, [=]
		{
			ClickConfigButton(i);
		});

		connect(bu_new_profile[i], &QAbstractButton::clicked, [=]
		{
			QInputDialog* dialog = new QInputDialog(this);
			dialog->setWindowTitle(tr("Choose a unique name"));
			dialog->setLabelText(tr("Profile Name: "));
			dialog->setFixedSize(500, 100);

			while (dialog->exec() != QDialog::Rejected)
			{
				QString friendlyName = dialog->textValue();
				if (friendlyName == "")
				{
					QMessageBox::warning(this, tr("Error"), tr("Name cannot be empty"));
					continue;
				}
				if (friendlyName.contains("."))
				{
					QMessageBox::warning(this, tr("Error"), tr("Must choose a name without '.'"));
					continue;
				}
				if (co_profile[i]->findText(friendlyName) != -1)
				{
					QMessageBox::warning(this, tr("Error"), tr("Please choose a non-existing name"));
					continue;
				}
				if (CreateConfigFile(qstr(PadThread::get_config_dir(g_cfg_input.player[i]->handler)), friendlyName))
				{
					co_profile[i]->addItem(friendlyName);
					co_profile[i]->setCurrentText(friendlyName);
				}
				break;
			}
		});
	}
	connect(ok_button, &QPushButton::clicked, this, &gamepads_settings_dialog::SaveExit);
	connect(cancel_button, &QPushButton::clicked, this, &gamepads_settings_dialog::CancelExit);
	connect(refresh_button, &QPushButton::clicked, [&] { 
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			// Retrigger input check on each dialog which will handle device updates
			ChangeInputType(i);
		}
	});
}

void gamepads_settings_dialog::SaveExit()
{
	//Check for invalid selection
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (co_deviceID[i]->currentData() == -1)
		{
			g_cfg_input.player[i]->handler.from_default();
			g_cfg_input.player[i]->device.from_default();
			g_cfg_input.player[i]->profile.from_default();
		}
		else {
			std::string tmp = sstr(co_inputtype[i]->currentText());
			if (!g_cfg_input.player[i]->handler.from_string(tmp))
			{
				LOG_ERROR(GENERAL, "Failed to convert input string:%s", tmp);
			}

			tmp = sstr(co_deviceID[i]->currentText());
			if (!g_cfg_input.player[i]->device.from_string(tmp))
			{
				//Something went wrong 
				LOG_ERROR(GENERAL, "Failed to convert device string: %s", tmp);
				return;
			}

			tmp = sstr(co_profile[i]->currentText());
			if (!g_cfg_input.player[i]->profile.from_string(tmp))
			{
				//Something went wrong 
				LOG_ERROR(GENERAL, "Failed to convert profile string: %s", tmp);
				return;
			}
		}
	}

	g_cfg_input.save();
	
	if (pt)
		pt->RefreshPads();

	QDialog::accept();
}

void gamepads_settings_dialog::CancelExit()
{
	QDialog::accept();
}

void gamepads_settings_dialog::ChangeInputType(int player)
{
	std::string device = g_cfg_input.player[player]->device.to_string();

	pad_handler handler_type = static_cast<pad_handler>(co_inputtype[player]->currentData().toInt());

	if (!pt) {
		LOG_ERROR(GENERAL, "ChangeInputType() pad_thread not running;");
		return;
	}

	std::vector<std::string> list_devices = pt->GetDeviceListForHandler(handler_type);

	// Refill the device combobox with currently available devices
	co_deviceID[player]->clear();

	for (int i = 0; i < list_devices.size(); i++)
	{
		co_deviceID[player]->addItem(qstr(list_devices[i]), i);
	}

	// force add any disconnected devices, ignoring the default ones
	// todo: mark these as disconnected in the ui so user is aware
	bool force_add = false;
	if ((handler_type == g_cfg_input.player[player]->handler) && (device != g_cfg_input.player[player]->device.def) && std::find(list_devices.begin(), list_devices.end(), device) == list_devices.end()) {
		co_deviceID[player]->addItem(qstr(device), (int)list_devices.size() + 1);
		force_add = true;
	}

	// Handle empty device list
	bool device_found = list_devices.size() > 0 || force_add;
	co_deviceID[player]->setEnabled(device_found);

	if (device_found)
	{
		co_deviceID[player]->setCurrentText(qstr(device));
	}
	else
	{
		co_deviceID[player]->addItem(tr("No Device Detected"), -1);
	}
	bool config_enabled = handler_type != pad_handler::null ? device_found :false;
	co_profile[player]->clear();

	// update profile list if possible
	if (config_enabled)
	{
		QString s_profile_dir = qstr(PadThread::get_config_dir(handler_type));
		QStringList profiles = gui_settings::GetDirEntries(QDir(s_profile_dir), QStringList() << "*.yml");

		if (profiles.isEmpty())
		{
			QString def_name = "Default Profile";
			if (!CreateConfigFile(s_profile_dir, def_name))
			{
				config_enabled = false;
			}
			else
			{
				co_profile[player]->addItem(def_name);
				co_profile[player]->setCurrentText(def_name);
			}
		}
		else
		{
			for (const auto& prof : profiles)
			{
				co_profile[player]->addItem(prof);
			}
			std::string profile = g_cfg_input.player[player]->profile.to_string();
			co_profile[player]->setCurrentText(qstr(profile));
		}
	}

	if (!config_enabled)
		co_profile[player]->addItem(tr("No Profiles"));

	// enable configuration and profile list if possible
	bu_config[player]->setEnabled(config_enabled);
	bu_new_profile[player]->setEnabled(config_enabled);
	co_profile[player]->setEnabled(config_enabled);

	// update view
	resizeComboBoxView(co_deviceID[player]);
	resizeComboBoxView(co_profile[player]);
}

void gamepads_settings_dialog::ClickConfigButton(int player)
{
	if (pt) {
		pad_handler handler_type = static_cast<pad_handler>(co_inputtype[player]->currentData().toInt());
		// todo: eventually should change emulatedpad based on requested 'type' of controller
		// also deal with disconnected controllers somehow
		std::unique_ptr<EmulatedPad> pad = pt->CreateTemporaryPad(handler_type, sstr(co_deviceID[player]->currentText()));
		if (pad) {
			pad_settings_dialog dlg(std::move(pad), handler_type, sstr(co_profile[player]->currentText()));
			dlg.exec();
		}
		else {
			LOG_ERROR(GENERAL, "Invalid pad sent to dialog");
		}
	}
	else {
		LOG_ERROR(GENERAL, "ClickConfigButton(): pad_thread not running");
	}
}
