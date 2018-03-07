#include "stdafx.h"
#include "overlays.h"
#include "GSRender.h"

namespace rsx
{
	namespace overlays
	{
		//Singleton instance declaration
		fontmgr* fontmgr::m_instance = nullptr;

		void user_interface::close()
		{
			if(const auto handler = fxm::get<PadThread>())
				handler->OverlayInUse(false);

			//Force unload
			exit = true;
			if (auto rsxthr = fxm::get<GSRender>())
				rsxthr->shell_close_dialog();

			if (on_close)
				on_close(return_code);
		}

		void user_interface::refresh()
		{
			if (auto rsxthr = fxm::get<GSRender>())
			{
				rsxthr->native_ui_flip_request.store(true);
			}
		}
	}
}
