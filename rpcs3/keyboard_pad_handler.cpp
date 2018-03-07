#include "keyboard_pad_handler.h"

#include <QApplication>
#include <QThread>

inline std::string sstr(const QString& _in) { return _in.toStdString(); }
constexpr auto qstr = QString::fromStdString;

keyboard_pad_handler::keyboard_pad_handler() : PadHandlerBase(pad_handler::keyboard, "Keyboard", ""), QObject()
{
    CreateBinding();
}

void keyboard_pad_handler::CreateBinding() {
    if (binding.get() != nullptr)
        return;

    binding = std::make_shared<Pad>();
    binding->connected = true;

    // since the qt key enums are over the place, this is a bit scattered
    for (u32 i = Qt::Key_Escape; i <= Qt::Key_Clear; ++i)
        binding->m_buttons.emplace_back(i, GetKeyName(i));

    for (u32 i = Qt::Key_Home; i <= Qt::Key_PageDown; ++i)
        binding->m_buttons.emplace_back(i, GetKeyName(i));
	
	binding->m_buttons.emplace_back(Qt::Key_Shift, "Shift");
	binding->m_buttons.emplace_back(Qt::Key_Control, "Ctrl");
	binding->m_buttons.emplace_back(Qt::Key_Control, "Meta");
	binding->m_buttons.emplace_back(Qt::Key_Control, "Alt");

    for (u32 i = Qt::Key_CapsLock; i <= Qt::Key_ScrollLock; ++i)
        binding->m_buttons.emplace_back(i, GetKeyName(i));

    for (u32 i = Qt::Key_F1; i <= Qt::Key_F24; ++i)
        binding->m_buttons.emplace_back(i, GetKeyName(i));

    for (u32 i = Qt::Key_Space; i <= Qt::Key_QuoteLeft; ++i)
        binding->m_buttons.emplace_back(i, GetKeyName(i));

    for (u32 i = Qt::Key_BraceLeft; i <= Qt::Key_AsciiTilde; ++i)
        binding->m_buttons.emplace_back(i, GetKeyName(i));

    for (u32 i = Qt::Key_nobreakspace; i <= Qt::Key_division; ++i)
        binding->m_buttons.emplace_back(i, GetKeyName(i));

    for (const auto& btn : mouse_list)
        binding->m_buttons.emplace_back(btn.first, btn.second);

}

void keyboard_pad_handler::init_config(pad_config* cfg, const std::string& name)
{
	// Set this profile's save location
	cfg->cfg_name = name;

	// Set default button mapping
	cfg->ls_left.def  = GetKeyName(Qt::Key_A);
	cfg->ls_down.def  = GetKeyName(Qt::Key_S);
	cfg->ls_right.def = GetKeyName(Qt::Key_D);
	cfg->ls_up.def    = GetKeyName(Qt::Key_W);
	cfg->rs_left.def  = GetKeyName(Qt::Key_Home);
	cfg->rs_down.def  = GetKeyName(Qt::Key_PageDown);
	cfg->rs_right.def = GetKeyName(Qt::Key_End);
	cfg->rs_up.def    = GetKeyName(Qt::Key_PageUp);
	cfg->start.def    = GetKeyName(Qt::Key_Return);
	cfg->select.def   = GetKeyName(Qt::Key_Space);
	cfg->ps.def       = GetKeyName(Qt::Key_Backspace);
	cfg->square.def   = GetKeyName(Qt::Key_Z);
	cfg->cross.def    = GetKeyName(Qt::Key_X);
	cfg->circle.def   = GetKeyName(Qt::Key_C);
	cfg->triangle.def = GetKeyName(Qt::Key_V);
	cfg->left.def     = GetKeyName(Qt::Key_Left);
	cfg->down.def     = GetKeyName(Qt::Key_Down);
	cfg->right.def    = GetKeyName(Qt::Key_Right);
	cfg->up.def       = GetKeyName(Qt::Key_Up);
	cfg->r1.def       = GetKeyName(Qt::Key_E);
	cfg->r2.def       = GetKeyName(Qt::Key_T);
	cfg->r3.def       = GetKeyName(Qt::Key_G);
	cfg->l1.def       = GetKeyName(Qt::Key_Q);
	cfg->l2.def       = GetKeyName(Qt::Key_R);
	cfg->l3.def       = GetKeyName(Qt::Key_F);

	// apply defaults
	cfg->from_default();
}

void keyboard_pad_handler::Key(const u32 code, bool pressed, u16 value)
{
    auto pad = binding.get();
	for (Button& button : pad->m_buttons)
	{
        if (button.m_keyCode == code)
		    button.m_value = pressed ? value : 0;
	}
}

int keyboard_pad_handler::GetModifierCode(QKeyEvent* e)
{
	switch (e->key())
	{
	case Qt::Key_Control:
	case Qt::Key_Alt:
	case Qt::Key_AltGr:
	case Qt::Key_Shift:
	case Qt::Key_Meta:
	case Qt::Key_NumLock:
		return 0;
	default:
		break;
	}

	if (e->modifiers() == Qt::ControlModifier)
		return Qt::ControlModifier;
	else if (e->modifiers() == Qt::AltModifier)
		return Qt::AltModifier;
	else if (e->modifiers() == Qt::MetaModifier)
		return Qt::MetaModifier;
	else if (e->modifiers() == Qt::ShiftModifier)
		return Qt::ShiftModifier;
	else if (e->modifiers() == Qt::KeypadModifier)
		return Qt::KeypadModifier;

	return 0;
}

bool keyboard_pad_handler::eventFilter(QObject* target, QEvent* ev)
{
	// !m_target is for future proofing when gsrender isn't automatically initialized on load.
	// !m_target->isVisible() is a hack since currently a guiless application will STILL inititialize a gsrender (providing a valid target)
	switch (ev->type())
	{
	case QEvent::KeyPress:
		keyPressEvent(static_cast<QKeyEvent*>(ev));
		break;
	case QEvent::KeyRelease:
		keyReleaseEvent(static_cast<QKeyEvent*>(ev));
		break;
	case QEvent::MouseButtonPress:
		mousePressEvent(static_cast<QMouseEvent*>(ev));
		break;
	case QEvent::MouseButtonRelease:
		mouseReleaseEvent(static_cast<QMouseEvent*>(ev));
		break;
	default:
		break;
	}
	return false;
}

/* Sets the target window for the event handler, and also installs an event filter on the target. */
void keyboard_pad_handler::SetTargetWindow(QWindow* target)
{
	m_target = target;

	if (m_target != nullptr)
	{
		m_target->installEventFilter(this);
	}	
	else
	{
		QApplication::instance()->installEventFilter(this);
		// If this is hit, it probably means that some refactoring occurs because currently a gsframe is created in Load.
		// We still want events so filter from application instead since target is null.
		LOG_ERROR(GENERAL, "Trying to set pad handler to a null target window.");
	}
}

void keyboard_pad_handler::ClearTargetWindow(QWindow* target) {
    if (target == nullptr)
        QApplication::instance()->removeEventFilter(this);
    else
        m_target->removeEventFilter(this);
}

void keyboard_pad_handler::processKeyEvent(QKeyEvent* event, bool pressed)
{
	if (event->isAutoRepeat())
	{
		event->ignore();
		return;
	}

	auto handleKey = [this, pressed, event]()
	{
		const QString name = qstr(GetKeyName(event));
		QStringList list = GetKeyNames(event);
		if (list.isEmpty())
			return;

		bool is_num_key = list.contains("Num");
		if (is_num_key)
			list.removeAll("Num");

		// TODO: Edge case: switching numlock keeps numpad keys pressed due to now different modifier

		// Handle every possible key combination, for example: ctrl+A -> {ctrl, A, ctrl+A}
		for (const auto& keyname : list)
		{
			// skip the 'original keys' when handling numpad keys
			if (is_num_key && !keyname.contains("Num"))
				continue;
			// skip held modifiers when handling another key
			if (keyname != name && list.count() > 1 && (keyname == "Alt" || keyname == "AltGr" || keyname == "Ctrl" || keyname == "Meta" || keyname == "Shift"))
				continue;
			Key(GetKeyCode(keyname), pressed);
		}
	};

	// We need to ignore keys when using rpcs3 keyboard shortcuts
	int key = event->key();
	switch (key)
	{
	case Qt::Key_Escape:
		break;
	case Qt::Key_L:
	case Qt::Key_Return:
		if (event->modifiers() != Qt::AltModifier)
			handleKey();
		break;
	case Qt::Key_P:
	case Qt::Key_S:
	case Qt::Key_R:
	case Qt::Key_E:
		if (event->modifiers() != Qt::ControlModifier)
			handleKey();
		break;
	default:
		handleKey();
		break;
	}
	event->ignore();
}

void keyboard_pad_handler::keyPressEvent(QKeyEvent* event)
{
	processKeyEvent(event, 1);
}

void keyboard_pad_handler::keyReleaseEvent(QKeyEvent* event)
{
	processKeyEvent(event, 0);
}

void keyboard_pad_handler::mousePressEvent(QMouseEvent* event)
{
	Key(event->button(), 1);
	event->ignore();
}

void keyboard_pad_handler::mouseReleaseEvent(QMouseEvent* event)
{
	Key(event->button(), 0, 0);
	event->ignore();
}

std::vector<std::string> keyboard_pad_handler::ListDevices()
{
	std::vector<std::string> list_devices;
	list_devices.push_back("Keyboard");
	return list_devices;
}

std::string keyboard_pad_handler::GetMouseName(const QMouseEvent* event)
{
	return GetMouseName(event->button());
}

std::string keyboard_pad_handler::GetMouseName(u32 button)
{
	auto it = mouse_list.find(button);
	if (it != mouse_list.end())
		return it->second;
	return "";
}

QStringList keyboard_pad_handler::GetKeyNames(const QKeyEvent* keyEvent)
{
	QStringList list;

	if (keyEvent->modifiers() & Qt::ShiftModifier)
	{
		list.append("Shift");
		list.append(QKeySequence(keyEvent->key() | Qt::ShiftModifier).toString(QKeySequence::NativeText));
	}
	if (keyEvent->modifiers() & Qt::AltModifier)
	{
		list.append("Alt");
		list.append(QKeySequence(keyEvent->key() | Qt::AltModifier).toString(QKeySequence::NativeText));
	}
	if (keyEvent->modifiers() & Qt::ControlModifier)
	{
		list.append("Ctrl");
		list.append(QKeySequence(keyEvent->key() | Qt::ControlModifier).toString(QKeySequence::NativeText));
	}
	if (keyEvent->modifiers() & Qt::MetaModifier)
	{
		list.append("Meta");
		list.append(QKeySequence(keyEvent->key() | Qt::MetaModifier).toString(QKeySequence::NativeText));
	}
	if (keyEvent->modifiers() & Qt::KeypadModifier)
	{
		list.append("Num"); // helper object, not used as actual key
		list.append(QKeySequence(keyEvent->key() | Qt::KeypadModifier).toString(QKeySequence::NativeText));
	}

	switch (keyEvent->key())
	{
	case Qt::Key_Alt:
		list.append("Alt");
		break;
	case Qt::Key_AltGr:
		list.append("AltGr");
		break;
	case Qt::Key_Shift:
		list.append("Shift");
		break;
	case Qt::Key_Control:
		list.append("Ctrl");
		break;
	case Qt::Key_Meta:
		list.append("Meta");
		break;
	default:
		list.append(QKeySequence(keyEvent->key()).toString(QKeySequence::NativeText));
		break;
	}

	list.removeDuplicates();
	return list;
}

std::string keyboard_pad_handler::GetKeyName(const QKeyEvent* keyEvent)
{
	switch (keyEvent->key())
	{
	case Qt::Key_Alt:
		return "Alt";
	case Qt::Key_AltGr:
		return "AltGr";
	case Qt::Key_Shift:
		return "Shift";
	case Qt::Key_Control:
		return "Ctrl";
	case Qt::Key_Meta:
		return "Meta";
	case Qt::Key_NumLock:
		return sstr(QKeySequence(keyEvent->key()).toString(QKeySequence::NativeText));
	default:
		break;
	}
	return sstr(QKeySequence(keyEvent->key() | keyEvent->modifiers()).toString(QKeySequence::NativeText));
}

std::string keyboard_pad_handler::GetKeyName(const u32& keyCode)
{
	return sstr(QKeySequence(keyCode).toString(QKeySequence::NativeText));
}

u32 keyboard_pad_handler::GetKeyCode(const std::string& keyName)
{
	return GetKeyCode(qstr(keyName));
}

u32 keyboard_pad_handler::GetKeyCode(const QString& keyName)
{
	if (keyName.isEmpty())
		return 0;
	else if (keyName == "Alt")
		return Qt::Key_Alt;
	else if (keyName == "AltGr")
		return Qt::Key_AltGr;
	else if (keyName == "Shift")
		return Qt::Key_Shift;
	else if (keyName == "Ctrl")
		return Qt::Key_Control;
	else if (keyName == "Meta")
		return Qt::Key_Meta;

	QKeySequence seq(keyName);
	u32 keyCode = 0;

	if (seq.count() == 1)
		keyCode = seq[0];
	else
		LOG_NOTICE(GENERAL, "GetKeyCode(%s): seq.count() = %d", sstr(keyName), seq.count());

	return keyCode;
}

s32 keyboard_pad_handler::EnableGetDevice(const std::string&) {
	return 0;
}

std::shared_ptr<Pad> keyboard_pad_handler::GetDeviceData(u32 deviceNumber) {
    if (binding == nullptr || deviceNumber != 0)
        return nullptr;
    return binding;
        
}

void keyboard_pad_handler::ThreadProc()
{
}
