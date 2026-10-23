// WBQtTeamsDialog.cpp -- see WBQtTeamsDialog.h. Every action goes through the bridge, then the
// dialog re-reads the state it shows.
#include "WBQtTeamsDialog.h"
#include "ui_WBQtTeamsDialog.h"
#include "WBQtTeamsBridge.h"
#include "WBQtTeamSheetDialog.h"
#include "WBQtTeamsDialogClassic.h"
#include "WBQtPickUnitBridge.h"		// the shared "replaced missing" report
#include "../WBQtWindowPos.h"
#include "resource.h"				// IDC_TEAM_NAME, for the inline rename

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDropEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QTextBrowser>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>

#include <qt_windows.h>

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

namespace
{
	const int kTextCap = 1024;

	// Display column -> the bridge's row column (0 name, 1 on create, 2 trigger, 3 priority, 4 home, 5 index).
	const int kBridgeColumn[6] = { 0, 3, 2, 4, 1, 5 };

	// The bridge row an item stands for; rows can be hidden by the filter, so the index alone won't do.
	const int kRowRole = Qt::UserRole;

	QString teamField(int row, const char *key)
	{
		char buf[kTextCap];
		buf[0] = 0;
		WBQtTeamsData_GetTeamField(row, key, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}

	QString badge(const QString &text, const char *colour)
	{
		return QString("<span style=\"background-color:%1; color:white;\">&nbsp;%2&nbsp;</span> ")
			.arg(colour).arg(text.toHtmlEscaped());
	}

	QString scriptLink(const QString &name)
	{
		return QString("<a href=\"wbscript:%1\">%2</a>")
			.arg(QString::fromLatin1(QUrl::toPercentEncoding(name))).arg(name.toHtmlEscaped());
	}

	// A drop reorders through the dialog's queued move instead of moving tree items.
	class TeamTree : public QTreeWidget
	{
	public:
		TeamTree(QObject *owner, QWidget *parent) : QTreeWidget(parent), m_owner(owner), m_from(-1), m_to(-1)
		{
			setDragDropMode(QAbstractItemView::InternalMove);
			setDefaultDropAction(Qt::MoveAction);
		}

		int takeFrom() { const int v = m_from; m_from = -1; return v; }
		int takeTo() { const int v = m_to; m_to = -1; return v; }

	protected:
		virtual void dropEvent(QDropEvent *event)
		{
			QTreeWidgetItem *source = currentItem();
			QTreeWidgetItem *target = itemAt(event->pos());
			m_from = (source != NULL) ? source->data(0, kRowRole).toInt() : -1;
			m_to = (target != NULL) ? target->data(0, kRowRole).toInt() : -1;
			event->setDropAction(Qt::IgnoreAction);
			event->accept();
			QMetaObject::invokeMethod(m_owner, "onTeamDropped", Qt::QueuedConnection);
		}

	private:
		QObject *m_owner;
		int m_from;
		int m_to;
	};
}

WBQtTeamsDialog::WBQtTeamsDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtTeamsDialog),
	m_updating(false),
	m_hasMissingUnits(false),
	m_fixMissingAction(NULL)
{
	setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowMaximizeButtonHint);
	m_ui->setupUi(this);

	m_players = m_ui->players;

	// The .ui tree is swapped for one whose drops reorder through the bridge.
	TeamTree *tree = new TeamTree(this, m_ui->split);
	tree->setColumnCount(6);
	tree->setHeaderItem(m_ui->teams->headerItem()->clone());
	tree->setRootIsDecorated(false);
	tree->setUniformRowHeights(true);
	tree->setContextMenuPolicy(Qt::CustomContextMenu);
	m_ui->split->replaceWidget(1, tree);
	delete m_ui->teams;
	m_teams = tree;
	m_teams->header()->resizeSection(0, 200);
	m_teams->header()->resizeSection(1, 60);
	m_teams->header()->resizeSection(2, 160);
	m_teams->header()->resizeSection(3, 120);
	m_teams->header()->resizeSection(4, 160);
	m_teams->header()->resizeSection(5, 40);
	m_teams->installEventFilter(this);
	m_ui->split->setStretchFactor(0, 0);
	m_ui->split->setStretchFactor(1, 3);
	m_ui->split->setStretchFactor(2, 2);
	m_ui->split->setSizes(QList<int>() << 170 << 620 << 380);
	m_ui->card->document()->setDocumentMargin(10);

	QMenu *tools = new QMenu(m_ui->toolsButton);
	QAction *membersAction = tools->addAction("Select team members from the map");
	m_fixMissingAction = tools->addAction("Fix missing units");
	tools->addSeparator();
	QAction *exportAction = tools->addAction("Export teams...");
	QAction *importAction = tools->addAction("Import teams...");
	m_ui->toolsButton->setMenu(tools);

	connect(m_players, SIGNAL(currentRowChanged(int)), this, SLOT(onPlayerRowChanged(int)));
	connect(m_teams, SIGNAL(itemSelectionChanged()), this, SLOT(onTeamRowChanged()));
	connect(m_teams, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onTeamDoubleClicked(QTreeWidgetItem*,int)));
	connect(m_teams, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onTeamContextMenu(QPoint)));
	connect(m_teams, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(onTeamRenamed(QTreeWidgetItem*,int)));
	connect(m_ui->filterEdit, SIGNAL(textChanged(QString)), this, SLOT(onFilterChanged(QString)));
	connect(m_ui->card, SIGNAL(anchorClicked(QUrl)), this, SLOT(onCardLinkClicked(QUrl)));
	connect(m_ui->newButton, SIGNAL(clicked()), this, SLOT(onNewTeam()));
	connect(m_ui->editButton, SIGNAL(clicked()), this, SLOT(onEditTeam()));
	connect(m_ui->copyButton, SIGNAL(clicked()), this, SLOT(onCopyTeam()));
	connect(m_ui->deleteButton, SIGNAL(clicked()), this, SLOT(onDeleteTeam()));
	connect(m_ui->moveUpButton, SIGNAL(clicked()), this, SLOT(onMoveUpTeam()));
	connect(m_ui->moveDownButton, SIGNAL(clicked()), this, SLOT(onMoveDownTeam()));
	connect(membersAction, SIGNAL(triggered()), this, SLOT(onSelectTeamMembers()));
	connect(m_fixMissingAction, SIGNAL(triggered()), this, SLOT(onFixMissingUnits()));
	connect(exportAction, SIGNAL(triggered()), this, SLOT(onExportTeams()));
	connect(importAction, SIGNAL(triggered()), this, SLOT(onImportTeams()));
	connect(m_ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	m_ui->newDesignCheck->setChecked(WBQtTeams_GetNewDesign() != 0);
	connect(m_ui->newDesignCheck, SIGNAL(toggled(bool)), this, SLOT(onNewDesignToggled(bool)));
	connect(m_ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	refreshAll();
	resize(1180, 640);	// default only: a saved size overrides this on show
	// Modal, so size only -- it keeps centering fresh on each open.
	WBQtWindowPos_TrackSize(this, "TeamBuilderNew");
}

void WBQtTeamsDialog::onNewDesignToggled(bool on)
{
	WBQtTeams_SetNewDesign(on ? 1 : 0);
}

WBQtTeamsDialog::~WBQtTeamsDialog()
{
	delete m_ui;
}

void WBQtTeamsDialog::refreshAll()
{
	refreshPlayers();
	refreshTeamsTable();
	refreshMissingUnits();
	refreshButtons();
}

// Re-scanned only after an action that could change a slot, never on selection changes.
void WBQtTeamsDialog::refreshMissingUnits()
{
	m_hasMissingUnits = (WBQtTeamsData_HasMissingUnits() != 0);
}

void WBQtTeamsDialog::refreshPlayers()
{
	m_updating = true;
	const int curPlayer = WBQtTeamsData_GetPlayerIndex();
	m_players->clear();
	char buf[kTextCap];
	const int playerCount = WBQtTeamsData_GetPlayerCount();
	for (int i = 0; i < playerCount; i++)
	{
		buf[0] = 0;
		WBQtTeamsData_GetPlayerName(i, buf, sizeof(buf));
		const int teams = WBQtTeamsData_GetPlayerTeamCount(i);
		new QListWidgetItem(QString("%1   (%2)").arg(QString::fromLocal8Bit(buf)).arg(teams), m_players);
	}
	if (curPlayer >= 0 && curPlayer < playerCount)
	{
		m_players->setCurrentRow(curPlayer);
	}
	m_updating = false;
}

void WBQtTeamsDialog::refreshTeamsTable()
{
	m_updating = true;
	char buf[kTextCap];
	m_teams->clear();
	const int rowCount = WBQtTeamsData_GetTeamRowCount();
	QTreeWidgetItem *selected = NULL;
	for (int row = 0; row < rowCount; row++)
	{
		QTreeWidgetItem *item = new QTreeWidgetItem(m_teams);
		item->setData(0, kRowRole, row);
		for (int col = 0; col < 6; col++)
		{
			buf[0] = 0;
			WBQtTeamsData_GetTeamRowText(row, kBridgeColumn[col], buf, sizeof(buf));
			item->setText(col, QString::fromLocal8Bit(buf));
		}
		if (WBQtTeamsData_GetTeamRowSelected(row) != 0)
		{
			selected = item;
		}
	}
	if (selected != NULL)
	{
		m_teams->setCurrentItem(selected);
		m_teams->scrollToItem(selected);
	}
	m_updating = false;
	onFilterChanged(m_ui->filterEdit->text());
	refreshCard();
}

void WBQtTeamsDialog::refreshButtons()
{
	m_ui->newButton->setEnabled(WBQtTeamsData_GetNewEnabled() != 0);
	m_ui->copyButton->setEnabled(WBQtTeamsData_GetCopyEnabled() != 0);
	m_ui->deleteButton->setEnabled(WBQtTeamsData_GetDeleteEnabled() != 0);
	m_ui->editButton->setEnabled(WBQtTeamsData_GetCopyEnabled() != 0);
	m_ui->moveUpButton->setEnabled(WBQtTeamsData_GetMoveEnabled() != 0);
	m_ui->moveDownButton->setEnabled(WBQtTeamsData_GetMoveEnabled() != 0);
	m_fixMissingAction->setEnabled(m_hasMissingUnits);
}

int WBQtTeamsDialog::currentTeamRow() const
{
	QTreeWidgetItem *item = m_teams->currentItem();
	return (item != NULL) ? item->data(0, kRowRole).toInt() : -1;
}

// The selected team: badges, description, members, production, reinforcement and its scripts.
void WBQtTeamsDialog::refreshCard()
{
	const int row = currentTeamRow();
	if (row < 0)
	{
		m_ui->card->clear();
		return;
	}
	QString html = QString("<div style=\"font-size:large; font-weight:bold;\">%1</div>")
		.arg(m_teams->currentItem()->text(0).toHtmlEscaped());

	QString badges;
	const QString owner = teamField(row, "teamOwner");
	if (!owner.isEmpty())
	{
		badges += badge(owner, "#37699f");
	}
	if (teamField(row, "teamIsAIRecruitable") == "1")
	{
		badges += badge("AI recruitable", "#37824b");
	}
	if (teamField(row, "teamIsSingleton") == "1")
	{
		badges += badge("once only", "#7d55a5");
	}
	if (teamField(row, "teamAutoReinforce") == "1")
	{
		badges += badge("auto reinforce", "#a0642a");
	}
	if (teamField(row, "teamIsBaseDefense") == "1")
	{
		badges += badge("base defense", "#466e78");
	}
	if (teamField(row, "teamIsPerimeterDefense") == "1")
	{
		badges += badge("perimeter defense", "#466e78");
	}
	const QString maxInstances = teamField(row, "teamMaxInstances");
	if (!maxInstances.isEmpty())
	{
		badges += badge("max " + maxInstances, "#696969");
	}
	html += "<p>" + badges + "</p>";

	const QString description = teamField(row, "teamDescription");
	if (!description.isEmpty())
	{
		html += "<p>" + description.toHtmlEscaped() + "</p>";
	}

	QString members;
	for (int i = 1; i <= 7; ++i)
	{
		const QString type = teamField(row, QString("teamUnitType%1").arg(i).toLatin1().constData());
		if (type.isEmpty())
		{
			continue;
		}
		const QString minCount = teamField(row, QString("teamUnitMinCount%1").arg(i).toLatin1().constData());
		const QString maxCount = teamField(row, QString("teamUnitMaxCount%1").arg(i).toLatin1().constData());
		const bool known = (WBQtTeamsData_IsTemplate(type.toLocal8Bit().constData()) != 0);
		members += badge(QString("%1  %2-%3").arg(type).arg(minCount).arg(maxCount), known ? "#3c4650" : "#af3737");
		members += "<br>";
	}
	html += "<p><b>Members</b><br>" + (members.isEmpty() ? QString("<span style=\"color:gray;\">(none)</span>") : members) + "</p>";

	QString facts;
	const QString home = teamField(row, "teamHome");
	if (!home.isEmpty())
	{
		facts += "Home: " + home.toHtmlEscaped() + "<br>";
	}
	const QString condition = teamField(row, "teamProductionCondition");
	if (!condition.isEmpty())
	{
		facts += QString("Built at priority %1 (+%2 on success, -%3 on failure) when %4<br>")
			.arg(teamField(row, "teamProductionPriority"))
			.arg(teamField(row, "teamProductionPrioritySuccessIncrease"))
			.arg(teamField(row, "teamProductionPriorityFailureDecrease"))
			.arg(scriptLink(condition));
	}
	const QString origin = teamField(row, "teamReinforcementOrigin");
	if (!origin.isEmpty())
	{
		facts += "Starts at: " + origin.toHtmlEscaped() + "<br>";
	}
	const QString transport = teamField(row, "teamTransport");
	if (!transport.isEmpty())
	{
		facts += "Deploys by: " + transport.toHtmlEscaped() + "<br>";
	}
	if (!facts.isEmpty())
	{
		html += "<p>" + facts + "</p>";
	}

	struct Trigger { const char *label; const char *key; };
	static const Trigger kTriggers[] = {
		{ "On create", "teamOnCreateScript" },
		{ "On enemy sighted", "teamEnemySightedScript" },
		{ "On all clear", "teamAllClearScript" },
		{ "On destroyed", "teamOnDestroyedScript" },
		{ "On idle", "teamOnIdleScript" },
		{ "On unit destroyed", "teamOnUnitDestroyedScript" }
	};
	QString scripts;
	for (int t = 0; t < 6; ++t)
	{
		const QString name = teamField(row, kTriggers[t].key);
		if (name.isEmpty())
		{
			continue;
		}
		QString label = kTriggers[t].label;
		if (t == 3)
		{
			label += QString(" (%1%)").arg(teamField(row, "teamDestroyedThreshold"));
		}
		scripts += label + ": " + scriptLink(name) + "<br>";
	}
	for (int g = 0; g < 16; ++g)
	{
		const QString name = teamField(row, QString("teamGenericScriptHook%1").arg(g).toLatin1().constData());
		if (name.isEmpty())
		{
			break;
		}
		scripts += "Generic: " + scriptLink(name) + "<br>";
	}
	if (!scripts.isEmpty())
	{
		html += "<p><b>Scripts</b><br>" + scripts + "</p>";
	}
	m_ui->card->setHtml(html);
}

void WBQtTeamsDialog::onCardLinkClicked(const QUrl &url)
{
	const QString full = url.toString(QUrl::FullyDecoded);
	if (!full.startsWith("wbscript:"))
	{
		return;
	}
	WBQtShowTeamScript(this, full.mid(9));
}

void WBQtTeamsDialog::onFilterChanged(const QString &text)
{
	const QString needle = text.trimmed();
	for (int i = 0; i < m_teams->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem *item = m_teams->topLevelItem(i);
		bool match = needle.isEmpty();
		for (int col = 0; col < 5 && !match; ++col)
		{
			match = item->text(col).contains(needle, Qt::CaseInsensitive);
		}
		item->setHidden(!match);
	}
}

void WBQtTeamsDialog::onPlayerRowChanged(int row)
{
	if (m_updating || row < 0)
	{
		return;
	}
	WBQtTeams_SelectPlayer(row);
	refreshTeamsTable();
	refreshButtons();
}

void WBQtTeamsDialog::onTeamRowChanged()
{
	if (m_updating)
	{
		return;
	}
	const int row = currentTeamRow();
	if (row >= 0)
	{
		WBQtTeams_SelectTeamRow(row);
		// Selecting a row changes no table content; only the enables and the card follow it.
		refreshButtons();
		refreshCard();
	}
}

void WBQtTeamsDialog::runTeamSheet()
{
	// Binds the four hidden Team* pages to the current team; refuses default teams.
	if (WBQtTeamSheet_Open() != 0)
	{
		WBQtTeamSheetDialog dlg(this);
		dlg.exec();
		WBQtTeamSheet_Close();
	}
}

void WBQtTeamsDialog::onTeamDoubleClicked(QTreeWidgetItem *item, int column)
{
	Q_UNUSED(column);
	if (item != NULL)
	{
		WBQtTeams_SelectTeamRow(item->data(0, kRowRole).toInt());
		onEditTeam();
	}
}

void WBQtTeamsDialog::onEditTeam()
{
	runTeamSheet();
	refreshAll();
}

void WBQtTeamsDialog::onRenameTeam()
{
	QTreeWidgetItem *item = m_teams->currentItem();
	if (item == NULL || WBQtTeamsData_GetCopyEnabled() == 0)
	{
		return;		// nothing selected, or the default team, which keeps its name
	}
	m_updating = true;
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	m_updating = false;
	m_teams->editItem(item, 0);
}

void WBQtTeamsDialog::onTeamRenamed(QTreeWidgetItem *item, int column)
{
	if (m_updating || item == NULL || column != 0)
	{
		return;
	}
	// The identity page's name commit validates the rename; refreshAll shows what it kept.
	WBQtTeams_SelectTeamRow(item->data(0, kRowRole).toInt());
	if (WBQtTeamSheet_Open() != 0)
	{
		const QByteArray name = item->text(0).toLocal8Bit();
		WBQtTeamPage_SetText(WB_QT_TEAMPAGE_IDENTITY, IDC_TEAM_NAME, name.constData(), WB_QT_TEAMNOTIFY_KILLFOCUS);
		WBQtTeamSheet_Close();
	}
	refreshAll();
}

void WBQtTeamsDialog::onTeamContextMenu(const QPoint &pos)
{
	QTreeWidgetItem *item = m_teams->itemAt(pos);
	if (item != NULL)
	{
		m_teams->setCurrentItem(item);
	}
	const bool editable = (item != NULL && WBQtTeamsData_GetCopyEnabled() != 0);

	QMenu menu(this);
	QAction *edit = menu.addAction("Edit\tEnter");
	QAction *rename = menu.addAction("Rename\tF2");
	QAction *copy = menu.addAction("Copy\tCtrl+D");
	QAction *remove = menu.addAction("Delete\tDel");
	menu.addSeparator();
	QAction *up = menu.addAction("Move Up\tAlt+Up");
	QAction *down = menu.addAction("Move Down\tAlt+Down");
	menu.addSeparator();
	QAction *members = menu.addAction("Select team members from the map");
	QAction *added = menu.addAction("New Team\tIns");
	edit->setEnabled(editable);
	rename->setEnabled(editable);
	copy->setEnabled(editable);
	remove->setEnabled(editable);
	up->setEnabled(editable);
	down->setEnabled(editable);
	added->setEnabled(WBQtTeamsData_GetNewEnabled() != 0);

	QAction *chosen = menu.exec(m_teams->viewport()->mapToGlobal(pos));
	if (chosen == edit) { onEditTeam(); }
	else if (chosen == rename) { onRenameTeam(); }
	else if (chosen == copy) { onCopyTeam(); }
	else if (chosen == remove) { onDeleteTeam(); }
	else if (chosen == up) { onMoveUpTeam(); }
	else if (chosen == down) { onMoveDownTeam(); }
	else if (chosen == members) { onSelectTeamMembers(); }
	else if (chosen == added) { onNewTeam(); }
}

bool WBQtTeamsDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_teams && event->type() == QEvent::KeyPress)
	{
		QKeyEvent *key = static_cast<QKeyEvent *>(event);
		const Qt::KeyboardModifiers mods = key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
		if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && mods == Qt::NoModifier)
		{
			onEditTeam();
			return true;
		}
		if (key->key() == Qt::Key_Delete && mods == Qt::NoModifier)
		{
			onDeleteTeam();
			return true;
		}
		if (key->key() == Qt::Key_Insert && mods == Qt::NoModifier)
		{
			onNewTeam();
			return true;
		}
		if (key->key() == Qt::Key_F2 && mods == Qt::NoModifier)
		{
			onRenameTeam();
			return true;
		}
		if (key->key() == Qt::Key_D && mods == Qt::ControlModifier)
		{
			onCopyTeam();
			return true;
		}
		if (key->key() == Qt::Key_Up && mods == Qt::AltModifier)
		{
			onMoveUpTeam();
			return true;
		}
		if (key->key() == Qt::Key_Down && mods == Qt::AltModifier)
		{
			onMoveDownTeam();
			return true;
		}
	}
	return QDialog::eventFilter(watched, event);
}

void WBQtTeamsDialog::onTeamDropped()
{
	TeamTree *tree = static_cast<TeamTree *>(m_teams);
	moveTeamRow(tree->takeFrom(), tree->takeTo());
}

void WBQtTeamsDialog::moveTeamRow(int from, int to)
{
	if (from < 0 || to < 0 || from == to)
	{
		return;
	}
	WBQtTeams_SelectTeamRow(from);
	// Each bridge move swaps the team with its neighbour and keeps it selected.
	for (int row = from; row != to && WBQtTeamsData_GetMoveEnabled() != 0; row += (to > row) ? 1 : -1)
	{
		if (to > row)
		{
			WBQtTeams_MoveDownTeam();
		}
		else
		{
			WBQtTeams_MoveUpTeam();
		}
	}
	refreshAll();
}

void WBQtTeamsDialog::onNewTeam()
{
	WBQtTeams_NewTeam();	// creates + selects the team (no sheet pop)
	runTeamSheet();			// == OnNewteam's OnEditTemplate on the fresh team
	refreshAll();
}

void WBQtTeamsDialog::onCopyTeam()
{
	WBQtTeams_CopyTeam();
	refreshAll();
}

void WBQtTeamsDialog::onDeleteTeam()
{
	WBQtTeams_DeleteTeam();	// may pop the in-use confirmation
	refreshAll();
}

void WBQtTeamsDialog::onSelectTeamMembers()
{
	WBQtTeams_SelectTeamMembers();	// pops the count info box + centers the 3D view
	refreshAll();
}

void WBQtTeamsDialog::onMoveUpTeam()
{
	WBQtTeams_MoveUpTeam();
	refreshAll();
}

void WBQtTeamsDialog::onMoveDownTeam()
{
	WBQtTeams_MoveDownTeam();
	refreshAll();
}

// Guess a replacement for each missing unit type, then show the report to review the guesses;
// like any edit here, OK commits them and Cancel discards them.
void WBQtTeamsDialog::onFixMissingUnits()
{
	const int found = WBQtTeams_ReplaceMissingUnits();
	if (found == 0)
	{
		QMessageBox::information(this, tr("Fix Missing Units"),
			tr("No missing unit types were found in the team templates."));
		return;
	}
	WBQtReplaceReport_Run(NULL);
	refreshAll();
}

void WBQtTeamsDialog::onExportTeams()
{
	WBQtTeams_ExportTeams();	// MFC file dialog + info boxes
	refreshAll();
}

void WBQtTeamsDialog::onImportTeams()
{
	WBQtTeams_ImportTeams();	// MFC file dialog; may pop fix-owner dialogs
	refreshAll();
}

// ===================== the modal entry point =====================

extern "C" int WBQtTeams_Run(void * /*frameHwnd*/)
{
	// Open may pop fix-team-owner modals (== the MFC OnInitDialog) before the window shows.
	WBQtTeamsData_Open();
	// Parented to the main window, ApplicationModal so the viewport is fenced too. The
	// "New design" setting picks the redesigned dialog or the original.
	int rc = 0;
	if (WBQtTeams_GetNewDesign() != 0)
	{
		WBQtTeamsDialog dlg(WBQt_DialogParent());
		dlg.setWindowModality(Qt::ApplicationModal);
		rc = (dlg.exec() == QDialog::Accepted) ? 1 : 0;
	}
	else
	{
		WBQtTeamsDialogClassic dlg(WBQt_DialogParent());
		dlg.setWindowModality(Qt::ApplicationModal);
		rc = (dlg.exec() == QDialog::Accepted) ? 1 : 0;
	}
	WBQtTeamsData_Close(rc);
	return rc;
}
