// WBQtTeamsDialog.h -- the Qt Team Builder over the hidden MFC CTeamsDialog, which owns the model.
#ifndef WB_QT_TEAMS_DIALOG_H
#define WB_QT_TEAMS_DIALOG_H

#include <QDialog>

class QAction;
class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QUrl;

namespace Ui { class WBQtTeamsDialog; }	// generated from WBQtTeamsDialog.ui

// Players on the left, their teams in the middle, the selected team as a card on the right.
class WBQtTeamsDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WBQtTeamsDialog(QWidget *parent = 0);
	virtual ~WBQtTeamsDialog();

protected:
	virtual bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void onPlayerRowChanged(int row);
	void onTeamRowChanged();
	void onTeamDoubleClicked(QTreeWidgetItem *item, int column);
	void onTeamContextMenu(const QPoint &pos);
	void onTeamRenamed(QTreeWidgetItem *item, int column);
	void onTeamDropped();		///< queued from a drop, once the drag has finished
	void onFilterChanged(const QString &text);
	void onCardLinkClicked(const QUrl &url);
	void onNewTeam();
	void onEditTeam();
	void onRenameTeam();
	void onCopyTeam();
	void onDeleteTeam();
	void onSelectTeamMembers();
	void onMoveUpTeam();
	void onMoveDownTeam();
	void onExportTeams();
	void onImportTeams();
	void onFixMissingUnits();
	void onNewDesignToggled(bool on);

private:
	void refreshAll();
	void refreshPlayers();
	void refreshTeamsTable();
	void refreshMissingUnits();
	void refreshButtons();
	void refreshCard();
	void runTeamSheet();
	int currentTeamRow() const;		///< the bridge row of the current team, or -1
	void moveTeamRow(int from, int to);

	Ui::WBQtTeamsDialog *m_ui;	// owns the static widget tree (WBQtTeamsDialog.ui)

	bool m_updating;
	bool m_hasMissingUnits;		// cached by refreshMissingUnits; gates the Fix Missing action
	QListWidget *m_players;
	QTreeWidget *m_teams;
	QAction *m_fixMissingAction;
};

#endif // WB_QT_TEAMS_DIALOG_H
