// WBQtTeamSheetDialogClassic.h -- the original team template sheet, opened by the original Team Builder.
#ifndef WB_QT_TEAMSHEET_DIALOG_CLASSIC_H
#define WB_QT_TEAMSHEET_DIALOG_CLASSIC_H

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace Ui { class WBQtTeamSheetDialogClassic; }	// generated from WBQtTeamSheetDialogClassic.ui

class WBQtTeamSheetDialogClassic : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtTeamSheetDialogClassic(QWidget *parent = 0);
	virtual ~WBQtTeamSheetDialogClassic();

private slots:
	// Re-apply the missing-unit red tint to whichever member combo changed.
	void onUnitTypeChanged();

private:
	// wire the .ui widgets of each tab to their hidden page controls
	void setupIdentityTab();
	void setupReinforcementTab();
	void setupBehaviorTab();
	void setupGenericTab();
	// Re-seed the 16 generic-script combos from the compacted hook chain and hide the rows
	// past the first empty slot (== TeamGeneric::_dictToScripts).
	void refreshGenericScripts();

	// binding helpers (each wires the widget to the hidden page control); the
	// widget-creating overloads serve the dynamic member/script row loops
	QLineEdit *bindEdit(int page, int ctrlId, QWidget *parent, int notify);
	void bindEdit(int page, int ctrlId, QLineEdit *edit, int notify);
	void bindCheck(int page, int ctrlId, QCheckBox *check);
	QComboBox *bindCombo(int page, int ctrlId, const QStringList &items, QWidget *parent, int notify);
	void bindCombo(int page, int ctrlId, const QStringList &items, QComboBox *combo, int notify);
	QStringList readComboItems(int page, int ctrlId) const;

	Ui::WBQtTeamSheetDialogClassic *m_ui;	// owns the static widget tree (WBQtTeamSheetDialogClassic.ui)

	QLineEdit *m_nameEdit;
	QComboBox *m_unitCombos[7];
	QComboBox *m_genericCombos[16];		// the 16 generic-script combos (for live compaction)
	QLabel    *m_genericLabels[16];		// their row labels (hidden together with the combos)
};

#endif // WB_QT_TEAMSHEET_DIALOG_CLASSIC_H
