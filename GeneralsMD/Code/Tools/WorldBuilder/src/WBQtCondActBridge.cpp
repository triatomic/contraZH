// WBQtCondActBridge.cpp -- MFC side of the Qt condition/action editor seam (Tier 2b). Plain MFC
// TU (no Qt include). Serves both Condition and ScriptAction (selected by isAction), mirroring
// how EditCondition / EditAction are twins: the template catalog comes from TheScriptEngine,
// the sentence model from getUiStrings()/getParameter(i)->getUiText(), warnings from
// EditParameter::getWarningText/getInfoText, and parameter editing pops the (still MFC)
// EditParameter modal exactly like the rich-edit link click did. Whole body guarded by
// RTS_HAS_QT so the OFF build compiles it to an empty object.
#include "StdAfx.h"
#include "resource.h"				// IDD_EDIT_PARAMETER (used in EditParameter.h's IDD enum)
#include "Lib/BaseType.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/ScriptEngine.h"
#include "EditParameter.h"
#include "qt/panels/WBQtCondActBridge.h"

#define SCRIPT_DIALOG_SECTION "ScriptDialog"

#ifdef RTS_HAS_QT

static void copyOut(const AsciiString &str, char *buf, int cap)
{
	if (buf == NULL || cap <= 0)
	{
		return;
	}
	strncpy(buf, str.str(), cap - 1);
	buf[cap - 1] = 0;
}

// ================= the template catalog =================

extern "C" int WBQtCondActData_GetTemplateCount(int isAction)
{
	return isAction ? ScriptAction::NUM_ITEMS : Condition::NUM_ITEMS;
}

extern "C" void WBQtCondActData_GetTemplateName(int isAction, int i, char *buf, int cap)
{
	if (isAction)
	{
		copyOut(TheScriptEngine->getActionTemplate(i)->getName(), buf, cap);
	}
	else
	{
		copyOut(TheScriptEngine->getConditionTemplate(i)->getName(), buf, cap);
	}
}

extern "C" void WBQtCondActData_GetTemplateName2(int isAction, int i, char *buf, int cap)
{
	if (isAction)
	{
		copyOut(TheScriptEngine->getActionTemplate(i)->getName2(), buf, cap);
	}
	else
	{
		copyOut(TheScriptEngine->getConditionTemplate(i)->getName2(), buf, cap);
	}
}

extern "C" void WBQtCondActData_GetTemplateHelp(int isAction, int i, char *buf, int cap)
{
	if (isAction)
	{
		copyOut(TheScriptEngine->getActionTemplate(i)->getHelpText(), buf, cap);
	}
	else
	{
		copyOut(TheScriptEngine->getConditionTemplate(i)->getHelpText(), buf, cap);
	}
}

// ================= the edited item =================

extern "C" int WBQtCondActData_GetType(void *item, int isAction)
{
	if (isAction)
	{
		return static_cast<ScriptAction *>(item)->getActionType();
	}
	return static_cast<Condition *>(item)->getConditionType();
}

extern "C" void WBQtCondActData_SetType(void *item, int isAction, int type)
{
	if (isAction)
	{
		static_cast<ScriptAction *>(item)->setActionType((enum ScriptAction::ScriptActionType)type);
	}
	else
	{
		static_cast<Condition *>(item)->setConditionType((enum Condition::ConditionType)type);
	}
}

extern "C" void WBQtCondActData_ClearWarningFlag(void *item, int isAction)
{
	if (isAction)
	{
		static_cast<ScriptAction *>(item)->setWarnings(false);
	}
	else
	{
		static_cast<Condition *>(item)->setWarnings(false);
	}
}

// ================= the parameter sentence =================

extern "C" int WBQtCondActData_GetUiStringCount(void *item, int isAction)
{
	AsciiString strings[MAX_PARMS];
	if (isAction)
	{
		return static_cast<ScriptAction *>(item)->getUiStrings(strings);
	}
	return static_cast<Condition *>(item)->getUiStrings(strings);
}

extern "C" void WBQtCondActData_GetUiString(void *item, int isAction, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	AsciiString strings[MAX_PARMS];
	int count;
	if (isAction)
	{
		count = static_cast<ScriptAction *>(item)->getUiStrings(strings);
	}
	else
	{
		count = static_cast<Condition *>(item)->getUiStrings(strings);
	}
	if (i >= 0 && i < count)
	{
		copyOut(strings[i], buf, cap);
	}
}

static Parameter *qtParameterAt(void *item, int isAction, int i)
{
	if (isAction)
	{
		ScriptAction *pAction = static_cast<ScriptAction *>(item);
		if (i < 0 || i >= pAction->getNumParameters())
		{
			return NULL;
		}
		return pAction->getParameter(i);
	}
	Condition *pCondition = static_cast<Condition *>(item);
	if (i < 0 || i >= pCondition->getNumParameters())
	{
		return NULL;
	}
	return pCondition->getParameter(i);
}

extern "C" int WBQtCondActData_GetParameterCount(void *item, int isAction)
{
	if (isAction)
	{
		return static_cast<ScriptAction *>(item)->getNumParameters();
	}
	return static_cast<Condition *>(item)->getNumParameters();
}

extern "C" void WBQtCondActData_GetParameterText(void *item, int isAction, int i, char *buf, int cap)
{
	if (buf != NULL && cap > 0)
	{
		buf[0] = 0;
	}
	Parameter *param = qtParameterAt(item, isAction, i);
	if (param != NULL)
	{
		copyOut(param->getUiText(), buf, cap);
	}
}

extern "C" void WBQtCondAct_EditParameter(void *item, int isAction, int i)
{
	Parameter *param = qtParameterAt(item, isAction, i);
	if (param != NULL)
	{
		// == the EN_LINK WM_LBUTTONDOWN handler. EditParameter's modals resolve their owner
		// via GetActiveWindow, which is the Qt dialog while it runs.
		if (isAction && param->getParameterType() == Parameter::COMMANDBUTTON_ABILITY)
		{
			// == EditAction's COMMANDBUTTON_ABILITY special case: the ability list is built
			// from the unit this action targets -- the action's FIRST parameter -- so pass
			// its name through, or the picker can't resolve the unit's command set.
			Parameter *unitParam = qtParameterAt(item, isAction, 0);
			if (unitParam != NULL)
			{
				EditParameter::edit(param, 0, unitParam->getString());
				return;
			}
		}
		EditParameter::edit(param, 0);
	}
}

extern "C" void WBQtCondActData_GetWarnings(void *item, int isAction, char *warnBuf, int warnCap, char *infoBuf, int infoCap)
{
	// == formatConditionText/formatActionText's warning sweep (both pass isAction=false to
	// getWarningText -- kept identical).
	AsciiString warningText;
	AsciiString informationText;
	int count = WBQtCondActData_GetParameterCount(item, isAction);
	for (int i = 0; i < count; i++)
	{
		Parameter *param = qtParameterAt(item, isAction, i);
		if (param != NULL)
		{
			warningText.concat(EditParameter::getWarningText(param, false));
			informationText.concat(EditParameter::getInfoText(param));
		}
	}
	copyOut(warningText, warnBuf, warnCap);
	copyOut(informationText, infoBuf, infoCap);
}

// Does THIS parameter have a warning? The sentence renders such parameters red, so the one that
// is wrong can be spotted without reading the warning panel and matching it up by name.
extern "C" int WBQtCondActData_ParameterHasWarning(void *item, int isAction, int i)
{
	Parameter *param = qtParameterAt(item, isAction, i);
	if (param == NULL)
	{
		return 0;
	}
	return EditParameter::getWarningText(param, false).isEmpty() ? 0 : 1;
}

extern "C" void WBQtCondActData_GetParameterWarning(void *item, int isAction, int i, char *buf, int cap)
{
	Parameter *param = qtParameterAt(item, isAction, i);
	copyOut((param != NULL) ? EditParameter::getWarningText(param, false) : AsciiString::TheEmptyString,
		buf, cap);
}

static int familyOf(Parameter::ParameterType type)
{
	switch (type)
	{
		case Parameter::TEAM:
		case Parameter::UNIT:
		case Parameter::OBJECT_TYPE:
		case Parameter::OBJECT_TYPE_LIST:
		case Parameter::KIND_OF_PARAM:
		case Parameter::BRIDGE:
		case Parameter::BUILDABLE:
		case Parameter::SPECIAL_POWER:
		case Parameter::SCIENCE:
		case Parameter::SCIENCE_AVAILABILITY:
		case Parameter::UPGRADE:
		case Parameter::COMMAND_BUTTON:
		case Parameter::COMMANDBUTTON_ABILITY:
		case Parameter::COMMANDBUTTON_ALL_ABILITIES:
		case Parameter::ATTACK_PRIORITY_SET:
		case Parameter::OBJECT_STATUS:
		case Parameter::OBJECT_PANEL_FLAG:
		case Parameter::REVEALNAME:
		case Parameter::EMOTICON:
		case Parameter::FACTION_NAME:
			return WBQT_PARAM_THING;
		case Parameter::SIDE:
		case Parameter::RELATION:
		case Parameter::TEAM_STATE:
		case Parameter::AI_MOOD:
			return WBQT_PARAM_PLAYER;
		case Parameter::WAYPOINT:
		case Parameter::WAYPOINT_PATH:
		case Parameter::SKIRMISH_WAYPOINT_PATH:
		case Parameter::TRIGGER_AREA:
		case Parameter::COORD3D:
		case Parameter::BOUNDARY:
		case Parameter::SURFACES_ALLOWED:
			return WBQT_PARAM_PLACE;
		case Parameter::INT:
		case Parameter::REAL:
		case Parameter::ANGLE:
		case Parameter::PERCENT:
		case Parameter::COMPARISON:
		case Parameter::BOOLEAN:
		case Parameter::SHAKE_INTENSITY:
		case Parameter::COLOR:
		case Parameter::LEFT_OR_RIGHT:
			return WBQT_PARAM_NUMBER;
		case Parameter::TEXT_STRING:
		case Parameter::LOCALIZED_TEXT:
		case Parameter::SOUND:
		case Parameter::DIALOG:
		case Parameter::MUSIC:
		case Parameter::MOVIE:
		case Parameter::FONT_NAME:
		case Parameter::RADAR_EVENT_TYPE:
			return WBQT_PARAM_TEXT;
		case Parameter::SCRIPT:
		case Parameter::SCRIPT_SUBROUTINE:
		case Parameter::COUNTER:
		case Parameter::FLAG:
			return WBQT_PARAM_LOGIC;
		default:
			return WBQT_PARAM_OTHER;
	}
}

extern "C" int WBQtCondActData_GetParameterFamily(void *item, int isAction, int i)
{
	Parameter *param = qtParameterAt(item, isAction, i);
	return (param != NULL) ? familyOf(param->getParameterType()) : WBQT_PARAM_OTHER;
}

extern "C" int WBQtCondActData_GetTemplateFamilies(int isAction, int i, int *out, int cap)
{
	const Template *tmpl = isAction
		? static_cast<const Template *>(TheScriptEngine->getActionTemplate(i))
		: static_cast<const Template *>(TheScriptEngine->getConditionTemplate(i));
	if (tmpl == NULL || out == NULL)
	{
		return 0;
	}
	int count = tmpl->getNumParameters();
	if (count > cap)
	{
		count = cap;
	}
	for (int p = 0; p < count; p++)
	{
		out[p] = familyOf(tmpl->getParameterType(p));
	}
	return count;
}

// ================= picker preferences =================

extern "C" int WBQtCondAct_GetCompress(void)
{
	return ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "CompressScripts", 1) ? 1 : 0;
}

extern "C" void WBQtCondAct_SetCompress(int enabled)
{
	::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "CompressScripts", enabled ? 1 : 0);
}

static const char *savedListKey(int isAction, int favorites)
{
	if (favorites)
	{
		return isAction ? "FavoriteActions" : "FavoriteConditions";
	}
	return isAction ? "RecentActions" : "RecentConditions";
}

extern "C" void WBQtCondAct_GetSavedList(int isAction, int favorites, char *buf, int cap)
{
	CString value = ::AfxGetApp()->GetProfileString(SCRIPT_DIALOG_SECTION, savedListKey(isAction, favorites), "");
	copyOut(AsciiString((const char *)value), buf, cap);
}

extern "C" void WBQtCondAct_SetSavedList(int isAction, int favorites, const char *paths)
{
	::AfxGetApp()->WriteProfileString(SCRIPT_DIALOG_SECTION, savedListKey(isAction, favorites),
		paths != NULL ? paths : "");
}

extern "C" int WBQtCondAct_GetNotesOpen(void)
{
	return ::AfxGetApp()->GetProfileInt(SCRIPT_DIALOG_SECTION, "PickerNotesOpen", 1) ? 1 : 0;
}

extern "C" void WBQtCondAct_SetNotesOpen(int open)
{
	::AfxGetApp()->WriteProfileInt(SCRIPT_DIALOG_SECTION, "PickerNotesOpen", open ? 1 : 0);
}

#endif // RTS_HAS_QT
