// WBQtScriptEditDialog.h -- the Qt script editor, run via WBQtScriptEdit_Run(); engine access is MFC-side.
#ifndef WB_QT_SCRIPT_EDIT_DIALOG_H
#define WB_QT_SCRIPT_EDIT_DIALOG_H

#include <QDialog>
#include <QList>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QToolButton;

namespace Ui { class WBQtScriptEditDialog; }	// generated from WBQtScriptEditDialog.ui

// One collapsible list section: IF (conditions), THEN (actions if true) or ELSE (actions if false).
class WBQtScriptEditSection : public QWidget
{
	Q_OBJECT
public:
	enum Mode
	{
		ModeConditions,
		ModeActionsTrue,
		ModeActionsFalse
	};

	WBQtScriptEditSection(void *script, Mode mode, QWidget *parent = 0);

	// Rebuild from the script, selecting selectRow (clamped; -1 keeps the current row).
	void reload(int selectRow);
	void setExpanded(bool expanded);
	void focusList();
	// Move a row by repeated bridge moves, so a drag reorders the script itself.
	void moveRow(int from, int to);

signals:
	// The other action list changed (Move to THEN / ELSE) and needs a reload.
	void otherListChanged();

protected:
	virtual bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onToggle();
	void onSelectionChanged();
	void onNew();
	void onEdit();
	void onCopy();
	void onCopyClipboard();
	void onPasteClipboard();
	void onDelete();
	void onOr();
	void onMoveToOther();
	void onMoveUp();
	void onMoveDown();
	void onNoteChanged();
	void onDropped();		///< queued from a drop, once the drag has finished

private:
	int currentRow() const;
	int isFalse() const;
	void updateButtonStates();
	void apply(int newRow);		///< reload when a bridge command changed something

	void *m_script;
	Mode m_mode;
	bool m_updating;

	QToolButton *m_toggle;
	QWidget *m_body;
	QListWidget *m_list;
	QPlainTextEdit *m_note;			///< the conditions note; NULL on the action sections
	QToolButton *m_editButton;
	QToolButton *m_copyButton;
	QToolButton *m_deleteButton;
	QToolButton *m_orButton;		///< conditions only
	QToolButton *m_otherButton;		///< actions only
	QToolButton *m_upButton;
	QToolButton *m_downButton;
};

class WBQtScriptEditDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtScriptEditDialog(void *script, QWidget *parent = 0);
	virtual ~WBQtScriptEditDialog();

	// Focus `tab` (0=Properties, 1=IF, 2=THEN, 3=ELSE) with `row` selected -- the [Missing] link jump.
	void applyInitialFocus(int tab, int row);

private slots:
	void onNameChanged(const QString &text);
	void onCommentChanged();
	void onActionNoteChanged();
	void onFlagToggled(bool on);
	void onEvalChanged(int index);
	void onSecondsChanged(int value);
	void onSmartCopyToggled(bool on);
	void onTrueListChanged();
	void onFalseListChanged();

private:
	void seedProperties();
	QList<QToolButton *> flagChips() const;	///< the six flag chips, each tagged with its flag

	Ui::WBQtScriptEditDialog *m_ui;	// owns the static widget tree (WBQtScriptEditDialog.ui)

	void *m_script;
	bool m_updating;

	WBQtScriptEditSection *m_conditions;
	WBQtScriptEditSection *m_actionsTrue;
	WBQtScriptEditSection *m_actionsFalse;
};

#endif // WB_QT_SCRIPT_EDIT_DIALOG_H
