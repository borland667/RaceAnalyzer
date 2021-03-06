/*
 * scriptPanel.h
 *
 *  Created on: Apr 30, 2009
 *      Author: brent
 */

#ifndef SCRIPTPANEL_H_
#define SCRIPTPANEL_H_

#include "wx/wxprec.h"
#include "wx/stc/stc.h"
#include "raceAnalyzerConfigBase.h"
#include "commonEvents.h"
#include "comm.h"
#include "configuration/baseConfigPanel.h"

class ScriptPanel : public BaseConfigPanel{

	public:
		ScriptPanel();
		ScriptPanel(wxWindow *parent,
					ConfigPanelParams config,
					wxWindowID id = -1,
					const wxPoint &pos = wxDefaultPosition,
					const wxSize &size = wxDefaultSize,
					long style = wxTAB_TRAVERSAL,
					const wxString &name = "panel"
					);


		~ScriptPanel();

		void OnConfigUpdated();
		void InitComponents();


		//event handlers
	private:

		void OnReadScript(wxCommandEvent &event);
		void OnWriteScript(wxCommandEvent &event);
		void OnRunScript(wxCommandEvent &event);
		void OnScriptChanged(wxStyledTextEvent &event);
		void StyleEditor(void);
		void InitOptions(void);


		wxStyledTextCtrl *m_scriptCtrl;

		static const int LUA_LEXER_ID = 15;
		enum{
			ID_SCRIPT_WINDOW = wxID_HIGHEST + 10000,
			ID_BUTTON_READ,
			ID_BUTTON_WRITE,
			ID_BUTTON_RUN
		};

	DECLARE_EVENT_TABLE()
};




#endif /* SCRIPTPANEL_H_ */
