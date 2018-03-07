#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QAction>
#include <QPainter>

#include "pad_settings_dialog.h"
#include "ui_pad_settings_dialog.h"

#include "Emu/IdManager.h"
#include "Emu/Io/PadThread.h"

inline std::string sstr(const QString& _in) { return _in.toStdString(); }
constexpr auto qstr = QString::fromStdString;

pad_settings_dialog::pad_settings_dialog(std::unique_ptr<EmulatedPad> pad, pad_handler handler, const std::string& profile, QWidget *parent)
	: QDialog(parent), ui(new Ui::pad_settings_dialog), m_pad(std::move(pad)), m_handler_type(handler), m_padWatcher(m_pad->GetRawPressWatcher())
{
	ui->setupUi(this);

	ui->b_cancel->setDefault(true);
	connect(ui->b_cancel, &QAbstractButton::clicked, this, &QWidget::close);

	m_padButtons = new QButtonGroup(this);
	m_palette = ui->b_left->palette(); // save normal palette

	// todo: can we just pass the pad_config in instead of doing this?
	m_pt = fxm::get<PadThread>();
	if (!m_pt)
		m_pt = fxm::import<PadThread>(Emu.GetCallbacks().get_pad_handler);

	if (!m_pt) {
		LOG_ERROR(GENERAL, "pad_settings_dialog: Unable to get pad_thread");
		return;
	}

    m_pt->EnableCapture(this);
	m_pt->OverlayInUse(true);

	std::string title = "Configure " + fmt::format("%s", m_handler_type);
	setWindowTitle(tr(title.c_str()));

	std::string cfg_name = PadThread::get_config_dir(m_handler_type) + profile + ".yml";
	m_pt->init_config(m_handler_type, m_handler_cfg, cfg_name);

	m_handler_cfg.load();

	// in order to show 'true' sticks, turn off deadzones, just make sure to set and save it later
	// the same could be done with squircle. This eventually should be rethought out to almost have keycode_cfg's come from emulated pad
	// then we could show current stick values vs what is being 'emulated'/sent to ps3
	m_lstickdeadzone = m_handler_cfg.lstickdeadzone;
	m_rstickdeadzone = m_handler_cfg.rstickdeadzone;

	m_handler_cfg.lstickdeadzone.set(0);
	m_handler_cfg.rstickdeadzone.set(0);

	ui->chb_vibration_large->setChecked((bool)m_handler_cfg.enable_vibration_motor_large);
	ui->chb_vibration_small->setChecked((bool)m_handler_cfg.enable_vibration_motor_small);
	ui->chb_vibration_switch->setChecked((bool)m_handler_cfg.switch_vibration_motors);

	// Use timer to get button input
	connect(&m_timer_input, &QTimer::timeout, [&]()
	{
		auto data = m_pad->GetPadData();
		// todo: remove me when qt isnt used for keyboard
		if (m_handler_type != pad_handler::keyboard) {
			ui->preview_trigger_left->setValue(data.m_press_L2);
			ui->preview_trigger_right->setValue(data.m_press_R2);

			RepaintPreviewLabel(ui->preview_stick_left, ui->slider_stick_left->value(), ui->slider_stick_left->size().width(), data.m_analog_left_x - 127, 127 - data.m_analog_left_y);
			RepaintPreviewLabel(ui->preview_stick_right, ui->slider_stick_right->value(), ui->slider_stick_right->size().width(), data.m_analog_right_x - 127, 127 -data.m_analog_right_y);
		}
		// Binding check
		if (m_timer.isActive()) {
			auto rtn = m_padWatcher->CheckForRawPress();
			if (rtn.first) {
				LOG_NOTICE(HLE, "GetNextButtonPress: %s button %s pressed", m_handler_type, rtn.second);
				if (m_button_id > button_ids::id_pad_begin && m_button_id < button_ids::id_pad_end)
				{
					m_cfg_entries[m_button_id].key = rtn.second;
					m_cfg_entries[m_button_id].text = qstr(rtn.second);
					ReactivateButtons();
				}
			}
		}

	});

	m_timer_input.start(5);

	// i hate this...todo: change keyboard handler to not be qt and remove this 
	if (m_handler_type == pad_handler::keyboard) {
		ui->b_blacklist->setEnabled(false);
		ui->verticalLayout_right->removeWidget(ui->gb_sticks);
		ui->verticalLayout_left->removeWidget(ui->gb_triggers);

		delete ui->gb_sticks;
		delete ui->gb_triggers;
	}
	else {
		auto initSlider = [=](QSlider* slider, const s32& value, const s32& min, const s32& max)
		{
			slider->setEnabled(true);
			slider->setRange(min, max);
			slider->setValue(value);
		};

		// Enable Trigger Thresholds
		initSlider(ui->slider_trigger_left, m_handler_cfg.ltriggerthreshold, 0, 255);
		initSlider(ui->slider_trigger_right, m_handler_cfg.rtriggerthreshold, 0, 255);
		ui->preview_trigger_left->setRange(0, 255);
		ui->preview_trigger_right->setRange(0, 255);

		// Enable Stick Deadzones
		initSlider(ui->slider_stick_left, m_lstickdeadzone, 0, 255);
		initSlider(ui->slider_stick_right, m_rstickdeadzone, 0, 255);

		// Initialize stick preview
		RepaintPreviewLabel(ui->preview_stick_left, ui->slider_stick_left->value(), ui->slider_stick_left->size().width(), 0, 0);
		RepaintPreviewLabel(ui->preview_stick_right, ui->slider_stick_right->value(), ui->slider_stick_right->size().width(), 0, 0);

	}

	// Enable Vibration Checkboxes
	if (m_pad->GetVibrateMotors().size() > 0)
	{
		const u8 min_force = 0;
		const u8 max_force = 255;

		ui->chb_vibration_large->setEnabled(true);
		ui->chb_vibration_small->setEnabled(true);
		ui->chb_vibration_switch->setEnabled(true);

		connect(ui->chb_vibration_large, &QCheckBox::clicked, [&](bool checked)
		{
			if (!checked) return;

			ui->chb_vibration_switch->isChecked() ? m_pad->SetRumble(min_force, true)
				: m_pad->SetRumble(max_force, false);

			QTimer::singleShot(300, [&]()
			{
				m_pad->SetRumble(min_force, false);
			});
		});

		connect(ui->chb_vibration_small, &QCheckBox::clicked, [&](bool checked)
		{
			if (!checked) return;

			ui->chb_vibration_switch->isChecked() ? m_pad->SetRumble(max_force, false)
				: m_pad->SetRumble(min_force, true);

			QTimer::singleShot(300, [&]()
			{
				m_pad->SetRumble(min_force, false);
			});
		});

		connect(ui->chb_vibration_switch, &QCheckBox::clicked, [&](bool checked)
		{
			checked ? m_pad->SetRumble(min_force, true) : m_pad->SetRumble(max_force, false);

			QTimer::singleShot(200, [&]()
			{
				checked ? m_pad->SetRumble(max_force, false) : m_pad->SetRumble(min_force, true);

				QTimer::singleShot(200, [&]()
				{
					m_pad->SetRumble(min_force, false);
				});
			});
		});
	}
	else
	{
		ui->verticalLayout_left->removeWidget(ui->gb_vibration);
		delete ui->gb_vibration;
	}

	auto insertButton = [this](int id, QPushButton* button, cfg::string* cfg_name)
	{
		QString name = qstr(*cfg_name);
		m_cfg_entries.insert(std::make_pair(id, pad_button{ cfg_name, *cfg_name, name }));
		m_padButtons->addButton(button, id);
		button->setText(name);
	};

	insertButton(button_ids::id_pad_lstick_left,  ui->b_lstick_left,  &m_handler_cfg.ls_left);
	insertButton(button_ids::id_pad_lstick_down,  ui->b_lstick_down,  &m_handler_cfg.ls_down);
	insertButton(button_ids::id_pad_lstick_right, ui->b_lstick_right, &m_handler_cfg.ls_right);
	insertButton(button_ids::id_pad_lstick_up,    ui->b_lstick_up,    &m_handler_cfg.ls_up);

	insertButton(button_ids::id_pad_left,  ui->b_left,  &m_handler_cfg.left);
	insertButton(button_ids::id_pad_down,  ui->b_down,  &m_handler_cfg.down);
	insertButton(button_ids::id_pad_right, ui->b_right, &m_handler_cfg.right);
	insertButton(button_ids::id_pad_up,    ui->b_up,    &m_handler_cfg.up);

	insertButton(button_ids::id_pad_l1, ui->b_shift_l1, &m_handler_cfg.l1);
	insertButton(button_ids::id_pad_l2, ui->b_shift_l2, &m_handler_cfg.l2);
	insertButton(button_ids::id_pad_l3, ui->b_shift_l3, &m_handler_cfg.l3);

	insertButton(button_ids::id_pad_start,  ui->b_start,  &m_handler_cfg.start);
	insertButton(button_ids::id_pad_select, ui->b_select, &m_handler_cfg.select);
	insertButton(button_ids::id_pad_ps,     ui->b_ps,     &m_handler_cfg.ps);

	insertButton(button_ids::id_pad_r1, ui->b_shift_r1, &m_handler_cfg.r1);
	insertButton(button_ids::id_pad_r2, ui->b_shift_r2, &m_handler_cfg.r2);
	insertButton(button_ids::id_pad_r3, ui->b_shift_r3, &m_handler_cfg.r3);

	insertButton(button_ids::id_pad_square,   ui->b_square,   &m_handler_cfg.square);
	insertButton(button_ids::id_pad_cross,    ui->b_cross,    &m_handler_cfg.cross);
	insertButton(button_ids::id_pad_circle,   ui->b_circle,   &m_handler_cfg.circle);
	insertButton(button_ids::id_pad_triangle, ui->b_triangle, &m_handler_cfg.triangle);

	insertButton(button_ids::id_pad_rstick_left,  ui->b_rstick_left,  &m_handler_cfg.rs_left);
	insertButton(button_ids::id_pad_rstick_down,  ui->b_rstick_down,  &m_handler_cfg.rs_down);
	insertButton(button_ids::id_pad_rstick_right, ui->b_rstick_right, &m_handler_cfg.rs_right);
	insertButton(button_ids::id_pad_rstick_up,    ui->b_rstick_up,    &m_handler_cfg.rs_up);

	m_padButtons->addButton(ui->b_reset,     button_ids::id_reset_parameters);
	m_padButtons->addButton(ui->b_blacklist, button_ids::id_blacklist);
	m_padButtons->addButton(ui->b_ok,        button_ids::id_ok);
	m_padButtons->addButton(ui->b_cancel,    button_ids::id_cancel);

	connect(m_padButtons, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &pad_settings_dialog::OnPadButtonClicked);

	connect(&m_timer, &QTimer::timeout, [&]()
	{
		if (--m_seconds <= 0)
		{
			ReactivateButtons();
			return;
		}
		m_padButtons->button(m_button_id)->setText(tr("[ Waiting %1 ]").arg(m_seconds));
	});

	UpdateLabel();

	gui_settings settings(this);

	// repaint and resize controller image
	ui->l_controller->setPixmap(settings.colorizedPixmap(*ui->l_controller->pixmap(), QColor(), gui::get_Label_Color("l_controller"), false, true));
	ui->l_controller->setMaximumSize(ui->gb_description->sizeHint().width(), ui->l_controller->maximumHeight() * ui->gb_description->sizeHint().width() / ui->l_controller->maximumWidth());

	layout()->setSizeConstraint(QLayout::SetFixedSize);
}

pad_settings_dialog::~pad_settings_dialog()
{
	delete ui;
}

void pad_settings_dialog::ReactivateButtons()
{
	m_timer.stop();
	m_seconds = MAX_SECONDS;

	if (m_button_id == button_ids::id_pad_begin)
	{
		return;
	}

	if (m_padButtons->button(m_button_id))
	{
		m_padButtons->button(m_button_id)->setPalette(m_palette);
	}

	m_button_id = button_ids::id_pad_begin;
	UpdateLabel();
	SwitchButtons(true);

	for (auto but : m_padButtons->buttons())
	{
		but->setFocusPolicy(Qt::StrongFocus);
	}
}

void pad_settings_dialog::RepaintPreviewLabel(QLabel* l, int dz, int w, int x, int y)
{
	int max = 127;
	int origin = w * 0.1;
	int width = w * 0.8;
	int dz_width = width * dz / 255;
	int dz_origin = (w - dz_width) / 2;

	int xpos = (w + (x * width / max)) / 2;
	int ypos = (w + (y * -1 * width / max)) / 2;

	QPixmap pm(w, w);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing, true);
	QPen pen(Qt::black, 2);
	p.setPen(pen);
	QBrush brush(Qt::white);
	p.setBrush(brush);
	p.drawEllipse(origin, origin, width, width);
	pen = QPen(Qt::red, 2);
	p.setPen(pen);
	p.drawEllipse(dz_origin, dz_origin, dz_width, dz_width);
	pen = QPen(Qt::blue, 2);
	p.setPen(pen);
	p.drawEllipse(xpos, ypos, 1, 1);
	l->setPixmap(pm);
}

void pad_settings_dialog::UpdateLabel(bool is_reset)
{
	if (is_reset)
	{
        if (m_pad->GetVibrateMotors().size() > 0) {
			ui->chb_vibration_large->setChecked((bool)m_handler_cfg.enable_vibration_motor_large);
			ui->chb_vibration_small->setChecked((bool)m_handler_cfg.enable_vibration_motor_small);
			ui->chb_vibration_switch->setChecked((bool)m_handler_cfg.switch_vibration_motors);
		}

		ui->slider_trigger_left->setValue(m_handler_cfg.ltriggerthreshold);
		ui->slider_trigger_right->setValue(m_handler_cfg.rtriggerthreshold);
		ui->slider_stick_left->setValue(m_rstickdeadzone);
		ui->slider_stick_right->setValue(m_lstickdeadzone);
	}

	for (auto& entry : m_cfg_entries)
	{
		if (is_reset)
		{
			entry.second.key = *entry.second.cfg_name;
			entry.second.text = qstr(entry.second.key);
		}

		entry.second.cfg_name->from_string(entry.second.key);
		m_padButtons->button(entry.first)->setText(entry.second.text);
	}
	m_pad->RebindController(m_handler_cfg);
}

void pad_settings_dialog::SwitchButtons(bool is_enabled)
{
	for (int i = button_ids::id_pad_begin + 1; i < button_ids::id_pad_end; i++)
	{
		m_padButtons->button(i)->setEnabled(is_enabled);
	}
}

void pad_settings_dialog::SaveConfig()
{
	for (const auto& entry : m_cfg_entries)
	{
		entry.second.cfg_name->from_string(entry.second.key);
	}

	if (m_pad->GetVibrateMotors().size() > 0)
	{
		m_handler_cfg.enable_vibration_motor_large.set(ui->chb_vibration_large->isChecked());
		m_handler_cfg.enable_vibration_motor_small.set(ui->chb_vibration_small->isChecked());
		m_handler_cfg.switch_vibration_motors.set(ui->chb_vibration_switch->isChecked());
	}

	if (m_handler_type != pad_handler::keyboard) {
		m_handler_cfg.ltriggerthreshold.set(ui->slider_trigger_left->value());
		m_handler_cfg.rtriggerthreshold.set(ui->slider_trigger_right->value());
		m_handler_cfg.lstickdeadzone.set(ui->slider_stick_left->value());
		m_handler_cfg.rstickdeadzone.set(ui->slider_stick_right->value());
	}
	m_handler_cfg.save();
}

void pad_settings_dialog::OnPadButtonClicked(int id)
{
	switch (id)
	{
	case button_ids::id_pad_begin:
	case button_ids::id_pad_end:
	case button_ids::id_cancel:
		return;
	case button_ids::id_reset_parameters:
		ReactivateButtons();
		m_handler_cfg.from_default();
		m_lstickdeadzone = m_handler_cfg.lstickdeadzone;
		m_rstickdeadzone = m_handler_cfg.rstickdeadzone;
		UpdateLabel(true);
		return;
	case button_ids::id_blacklist:
		//todo:
		return;
	case button_ids::id_ok:
		SaveConfig();
		m_pt->OverlayInUse(false);
		QDialog::accept();
		return;
	default:
		break;
	}

	for (auto but : m_padButtons->buttons())
	{
		but->setFocusPolicy(Qt::ClickFocus);
	}

	m_button_id = id;
	m_padButtons->button(m_button_id)->setText(tr("[ Waiting %1 ]").arg(MAX_SECONDS));
	m_padButtons->button(m_button_id)->setPalette(QPalette(Qt::blue));
	SwitchButtons(false); // disable all buttons, needed for using Space, Enter and other specific buttons
	m_timer.start(1000);
	m_padWatcher->Reset();
}
