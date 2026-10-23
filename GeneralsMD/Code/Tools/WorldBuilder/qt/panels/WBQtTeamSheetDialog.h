// WBQtTeamSheetDialog.h -- the Qt team template sheet; the hidden MFC Team* pages own the logic and
// write the team dict live, and this dialog drives their controls through WBQtTeamsBridge.h.
#ifndef WB_QT_TEAMSHEET_DIALOG_H
#define WB_QT_TEAMSHEET_DIALOG_H

#include <QDialog>
#include <QHash>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QToolButton;

namespace Ui { class WBQtTeamSheetDialog; }	// generated from WBQtTeamSheetDialog.ui

// Show the named script read-only, or say that there is none; shared with the Team Builder's card.
void WBQtShowTeamScript(QWidget *parent, const QString &name);

// One page of collapsible sections: identity, members, production, reinforcement, behavior, scripts.
class WBQtTeamSheetDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtTeamSheetDialog(QWidget *parent = 0);
	virtual ~WBQtTeamSheetDialog();

private slots:
	void onUnitTypeChanged();		///< re-tint the member combo that changed
	void onAddUnit();
	void onRemoveUnit();
	void onSectionToggled(bool open);
	void onViewScript();
	void onGenericMove();
	void onGenericRemove();
	void updateWarnings();

private:
	void setupSections();
	void setupIdentity();
	void setupMembers();
	void setupProduction();
	void setupReinforcement();
	void setupBehavior();
	void setupGeneric();
	// Show the filled member rows plus any added ones, and total their counts.
	void refreshMembers();
	// Re-seed the generic-script rows from the compacted hook chain, showing one empty slot to add with.
	void refreshGenericScripts();

	// binding helpers: each wires a widget to its hidden page control
	void bindEdit(int page, int ctrlId, QLineEdit *edit, int notify);
	void bindSpin(int page, int ctrlId, QSpinBox *spin, int notify);
	void bindCheck(int page, int ctrlId, QCheckBox *check);
	void bindCombo(int page, int ctrlId, const QStringList &items, QComboBox *combo, int notify);
	QStringList readComboItems(int page, int ctrlId) const;

	Ui::WBQtTeamSheetDialog *m_ui;	// owns the static widget tree (WBQtTeamSheetDialog.ui)

	QHash<QObject *, QWidget *> m_sectionBodies;	///< section header -> its body
	QComboBox *m_unitCombos[7];
	QSpinBox *m_minSpins[7];
	QSpinBox *m_maxSpins[7];
	QWidget *m_memberWidgets[7][5];		///< each member row: unit, pick, min, max, remove
	bool m_memberAdded[7];				///< an empty row the user opened with Add unit
	QComboBox *m_genericCombos[16];
	QWidget *m_genericWidgets[16][6];	///< each script row: label, combo, view, up, down, remove
};

#endif // WB_QT_TEAMSHEET_DIALOG_H
