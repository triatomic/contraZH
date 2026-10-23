// WBQtScriptEditDialogClassic.h -- the original tabbed script editor, shown while "New design" is off.
#ifndef WB_QT_SCRIPT_EDIT_DIALOG_CLASSIC_H
#define WB_QT_SCRIPT_EDIT_DIALOG_CLASSIC_H

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTabWidget;

namespace Ui { class WBQtScriptEditListTabClassic; }	// generated from WBQtScriptEditListTabClassic.ui
namespace Ui { class WBQtScriptEditDialogClassic; }	// generated from WBQtScriptEditDialogClassic.ui

// One list page (Script Conditions / Actions if true / Actions if false) -- the three MFC pages
// are structural twins (list + button column + Smart Copy + comment), so one widget serves all
// three, parameterized by mode.
class WBQtScriptEditListTabClassic : public QWidget
{
	Q_OBJECT
public:
	enum Mode
	{
		ModeConditions,
		ModeActionsTrue,
		ModeActionsFalse
	};

	WBQtScriptEditListTabClassic(void *script, Mode mode, QWidget *parent = 0);
	virtual ~WBQtScriptEditListTabClassic();

	// Rebuild the list from the script (== the MFC page's loadList()); selects selectRow
	// (clamped; -1 keeps the current row) and re-seeds the Smart Copy checkbox + comment.
	void reload(int selectRow);

private slots:
	void onSelectionChanged();
	void onNew();
	void onEdit();
	void onCopy();
	void onCopyClipboard();		// Ctrl+C: stash to the cross-script clipboard
	void onPasteClipboard();	// Ctrl+V: paste from the cross-script clipboard
	void onDelete();
	void onOr();
	void onMoveToOther();
	void onMoveUp();
	void onMoveDown();
	void onSmartCopyToggled(bool checked);
	void onCommentChanged();

private:
	int  currentRow() const;
	void updateButtonStates();

	Ui::WBQtScriptEditListTabClassic *m_ui;	// owns the static widget tree (WBQtScriptEditListTabClassic.ui)

	void *m_script;
	Mode m_mode;
	bool m_updating;

	QListWidget *m_list;
	QCheckBox *m_smartCopyCheck;
	QPushButton *m_newButton;
	QPushButton *m_editButton;
	QPushButton *m_copyButton;
	QPushButton *m_deleteButton;
	QPushButton *m_orButton;			// conditions only
	QPushButton *m_moveToOtherButton;	// actions only
	QPushButton *m_moveUpButton;
	QPushButton *m_moveDownButton;
	QPlainTextEdit *m_commentEdit;
};

class WBQtScriptEditDialogClassic : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtScriptEditDialogClassic(void *script, QWidget *parent = 0);
	virtual ~WBQtScriptEditDialogClassic();

	// Open on `tab` (0=Properties, 1=Conditions, 2=Actions if true, 3=Actions if false) with
	// `row` selected in that tab's list -- the [Missing] link jump (WBQtScriptEdit_SetInitialFocus).
	void applyInitialFocus(int tab, int row);

private slots:
	void onTabChanged(int index);
	void onNameChanged(const QString &text);
	void onCommentChanged();
	void onEveryFrame();
	void onEverySecond();
	void onSecondsChanged(int value);

private:
	void wirePropertiesTab();	// binds + connects the Script Properties page (tree lives in the .ui)
	void seedProperties();

	Ui::WBQtScriptEditDialogClassic *m_ui;	// owns the static widget tree (WBQtScriptEditDialogClassic.ui)

	void *m_script;
	bool m_updating;

	QTabWidget *m_tabs;

	// Script Properties page
	QLineEdit *m_nameEdit;
	QCheckBox *m_subroutineCheck;
	QCheckBox *m_activeCheck;
	QCheckBox *m_oneShotCheck;
	QCheckBox *m_easyCheck;
	QCheckBox *m_normalCheck;
	QCheckBox *m_hardCheck;
	QRadioButton *m_everyFrameRadio;
	QRadioButton *m_everySecondRadio;
	QSpinBox *m_secondsSpin;
	QPlainTextEdit *m_commentEdit;

	WBQtScriptEditListTabClassic *m_conditionsTab;
	WBQtScriptEditListTabClassic *m_trueTab;
	WBQtScriptEditListTabClassic *m_falseTab;
};

#endif // WB_QT_SCRIPT_EDIT_DIALOG_CLASSIC_H
