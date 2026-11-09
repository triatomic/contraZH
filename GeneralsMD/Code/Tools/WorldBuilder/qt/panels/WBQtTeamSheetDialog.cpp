// WBQtTeamSheetDialog.cpp -- see WBQtTeamSheetDialog.h. Control IDs come from the WB resource.h
// (pure #defines, Qt-safe; the res dir is on the qt lib include path).
#include "WBQtTeamSheetDialog.h"
#include "ui_WBQtTeamSheetDialog.h"
#include "WBQtComboStyle.h"
#include "WBQtTeamsBridge.h"
#include "../WBQtWindowPos.h"
#include "resource.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
	const int kTextCap = 1024;
	const int kScriptCap = 16384;
	const char *const kNone = "<none>";		// == NONE_STRING, the empty slot in every page combo

	const int kMinIds[7] = { IDC_MIN_UNIT1, IDC_MIN_UNIT2, IDC_MIN_UNIT3, IDC_MIN_UNIT4, IDC_MIN_UNIT5, IDC_MIN_UNIT6, IDC_MIN_UNIT7 };
	const int kMaxIds[7] = { IDC_MAX_UNIT1, IDC_MAX_UNIT2, IDC_MAX_UNIT3, IDC_MAX_UNIT4, IDC_MAX_UNIT5, IDC_MAX_UNIT6, IDC_MAX_UNIT7 };
	const int kTypeIds[7] = { IDC_UNIT_TYPE1, IDC_UNIT_TYPE2, IDC_UNIT_TYPE3, IDC_UNIT_TYPE4, IDC_UNIT_TYPE5, IDC_UNIT_TYPE6, IDC_UNIT_TYPE7 };
	const int kPickIds[7] = { IDC_UNIT_TYPE1_BUTTON, IDC_UNIT_TYPE2_BUTTON, IDC_UNIT_TYPE3_BUTTON, IDC_UNIT_TYPE4_BUTTON, IDC_UNIT_TYPE5_BUTTON, IDC_UNIT_TYPE6_BUTTON, IDC_UNIT_TYPE7_BUTTON };
	const int kGenericIds[16] = {
		IDC_TeamGeneric_Script1, IDC_TeamGeneric_Script2, IDC_TeamGeneric_Script3,
		IDC_TeamGeneric_Script4, IDC_TeamGeneric_Script5, IDC_TeamGeneric_Script6,
		IDC_TeamGeneric_Script7, IDC_TeamGeneric_Script8, IDC_TeamGeneric_Script9,
		IDC_TeamGeneric_Script10, IDC_TeamGeneric_Script11, IDC_TeamGeneric_Script12,
		IDC_TeamGeneric_Script13, IDC_TeamGeneric_Script14, IDC_TeamGeneric_Script15,
		IDC_TeamGeneric_Script16
	};

	QString pageText(int page, int ctrlId)
	{
		char buf[kTextCap];
		buf[0] = 0;
		WBQtTeamPage_GetText(page, ctrlId, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}

	bool isEmptySlot(const QString &text)
	{
		return text.isEmpty() || text == kNone;
	}

	// Ensure the combo's current (possibly "[???] ..." placeholder) text is selectable.
	void seedComboCurrent(QComboBox *combo, const QString &current)
	{
		if (!current.isEmpty() && combo->findText(current) < 0)
		{
			combo->addItem(current);
		}
		combo->setCurrentIndex(combo->findText(current));
	}

	// The bridge renders a unit type with no template as "[???] name"; tint those red like a red script.
	void tintIfMissing(QComboBox *combo)
	{
		const bool missing = combo->currentText().startsWith("[???] ");
		QPalette pal = combo->palette();
		const QPalette &base = QApplication::palette(combo);
		pal.setColor(QPalette::Text, missing ? QColor(200, 60, 60) : base.color(QPalette::Text));
		pal.setColor(QPalette::ButtonText, missing ? QColor(200, 60, 60) : base.color(QPalette::ButtonText));
		combo->setPalette(pal);
	}

	QToolButton *smallButton(const QString &text, const QString &tip, QWidget *parent)
	{
		QToolButton *button = new QToolButton(parent);
		button->setText(text);
		button->setToolTip(tip);
		button->setAutoRaise(true);
		return button;
	}
}

void WBQtShowTeamScript(QWidget *parent, const QString &name)
{
	if (isEmptySlot(name))
	{
		return;
	}
	static char buf[kScriptCap];
	WBQtTeamsData_GetScriptText(name.toLocal8Bit().constData(), buf, sizeof(buf));
	const QString text = QString::fromLocal8Bit(buf);
	if (text.isEmpty())
	{
		QMessageBox::information(parent, name, QString("No script named \"%1\" exists.").arg(name));
		return;
	}
	// Read-only: the team dialogs are modal, so the script editor can't open behind them.
	QDialog view(parent);
	view.setWindowTitle(name);
	view.setWindowFlags(view.windowFlags() & ~Qt::WindowContextHelpButtonHint);
	QVBoxLayout *layout = new QVBoxLayout(&view);
	QPlainTextEdit *edit = new QPlainTextEdit(QString(text).remove('\r'), &view);
	edit->setReadOnly(true);
	edit->setLineWrapMode(QPlainTextEdit::NoWrap);
	layout->addWidget(edit);
	QDialogButtonBox *close = new QDialogButtonBox(QDialogButtonBox::Close, &view);
	QObject::connect(close, SIGNAL(rejected()), &view, SLOT(reject()));
	layout->addWidget(close);
	view.resize(720, 420);
	view.exec();
}

WBQtTeamSheetDialog::WBQtTeamSheetDialog(QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtTeamSheetDialog)
{
	setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowMaximizeButtonHint);
	m_ui->setupUi(this);

	setupSections();
	setupIdentity();
	setupMembers();
	setupProduction();
	setupReinforcement();
	setupBehavior();
	setupGeneric();

	// Check boxes read as toggle chips.
	const QList<QCheckBox *> checks = m_ui->page->findChildren<QCheckBox *>();
	for (int i = 0; i < checks.size(); ++i)
	{
		checks.at(i)->setStyleSheet(
			"QCheckBox { border: 1px solid palette(mid); border-radius: 9px; padding: 1px 9px; }"
			"QCheckBox::indicator { width: 0px; height: 0px; }"
			"QCheckBox:checked { background-color: #37699f; border-color: #37699f; color: white; }"
			"QCheckBox:disabled { color: gray; }");
	}

	// Run after every combo exists, so the popup bound covers the code-built ones too.
	WBQtComboStyle::applyPopupScrollRecursive(this);

	// Edits apply live to the Teams dialog's working copy; OK only closes.
	connect(m_ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));

	updateWarnings();
	resize(820, 760);	// default only: a saved size overrides this on show
	// Modal, so size only; the tracker clamps a stored size up to the layout minimum.
	WBQtWindowPos_TrackSize(this, "TeamSheetNew");
}

WBQtTeamSheetDialog::~WBQtTeamSheetDialog()
{
	delete m_ui;
}

// ---- binding helpers ----

QStringList WBQtTeamSheetDialog::readComboItems(int page, int ctrlId) const
{
	QStringList items;
	char buf[kTextCap];
	const int count = WBQtTeamPage_ComboCount(page, ctrlId);
	for (int i = 0; i < count; i++)
	{
		buf[0] = 0;
		WBQtTeamPage_ComboItem(page, ctrlId, i, buf, sizeof(buf));
		items.append(QString::fromLocal8Bit(buf));
	}
	return items;
}

void WBQtTeamSheetDialog::bindEdit(int page, int ctrlId, QLineEdit *edit, int notify)
{
	edit->setText(pageText(page, ctrlId));
	connect(edit, &QLineEdit::editingFinished, edit, [edit, page, ctrlId, notify]()
	{
		const QByteArray text = edit->text().toLocal8Bit();
		WBQtTeamPage_SetText(page, ctrlId, text.constData(), notify);
		// The handler may reject or normalize (the team-name rename validation); read back.
		const QString stored = pageText(page, ctrlId);
		if (stored != edit->text())
		{
			edit->setText(stored);
		}
	});
}

void WBQtTeamSheetDialog::bindSpin(int page, int ctrlId, QSpinBox *spin, int notify)
{
	spin->setValue(pageText(page, ctrlId).toInt());
	connect(spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), spin,
		[page, ctrlId, notify](int value)
	{
		WBQtTeamPage_SetText(page, ctrlId, QByteArray::number(value).constData(), notify);
	});
}

void WBQtTeamSheetDialog::bindCheck(int page, int ctrlId, QCheckBox *check)
{
	check->setChecked(WBQtTeamPage_GetCheck(page, ctrlId) != 0);
	check->setEnabled(WBQtTeamPage_IsEnabled(page, ctrlId) != 0);
	connect(check, &QCheckBox::toggled, check, [page, ctrlId](bool on)
	{
		WBQtTeamPage_SetCheck(page, ctrlId, on ? 1 : 0);
	});
}

void WBQtTeamSheetDialog::bindCombo(int page, int ctrlId, const QStringList &items, QComboBox *combo, int notify)
{
	// Measuring every entry of a catalog combo made the sheet crawl to open; cap the width instead.
	combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	combo->setMinimumContentsLength(24);
	combo->addItems(items);
	seedComboCurrent(combo, pageText(page, ctrlId));
	combo->setEnabled(WBQtTeamPage_IsEnabled(page, ctrlId) != 0);
	connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), combo,
		[combo, page, ctrlId, notify](int index)
	{
		const QByteArray text = combo->itemText(index).toLocal8Bit();
		WBQtTeamPage_ComboSelectText(page, ctrlId, text.constData(), notify);
	});
}

// ---- sections ----

void WBQtTeamSheetDialog::setupSections()
{
	QToolButton *headers[] = { m_ui->identityHeader, m_ui->membersHeader, m_ui->productionHeader,
		m_ui->reinforcementHeader, m_ui->behaviorHeader, m_ui->genericHeader };
	QWidget *bodies[] = { m_ui->identityBody, m_ui->membersBody, m_ui->productionBody,
		m_ui->reinforcementBody, m_ui->behaviorBody, m_ui->genericBody };
	for (int i = 0; i < 6; ++i)
	{
		QFont bold = headers[i]->font();
		bold.setBold(true);
		headers[i]->setFont(bold);
		headers[i]->setArrowType(Qt::DownArrow);
		m_sectionBodies.insert(headers[i], bodies[i]);
		connect(headers[i], SIGNAL(toggled(bool)), this, SLOT(onSectionToggled(bool)));
	}
}

void WBQtTeamSheetDialog::onSectionToggled(bool open)
{
	QToolButton *header = qobject_cast<QToolButton *>(sender());
	if (header != NULL)
	{
		header->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
		m_sectionBodies.value(header)->setVisible(open);
	}
}

void WBQtTeamSheetDialog::setupIdentity()
{
	const int page = WB_QT_TEAMPAGE_IDENTITY;
	bindEdit(page, IDC_TEAM_NAME, m_ui->nameEdit, WB_QT_TEAMNOTIFY_KILLFOCUS);
	bindSpin(page, IDC_MAX, m_ui->maxSpin, WB_QT_TEAMNOTIFY_CHANGE);
	bindCombo(page, IDC_TEAMOWNER, readComboItems(page, IDC_TEAMOWNER), m_ui->ownerCombo, WB_QT_TEAMNOTIFY_SELENDOK);
	bindCombo(page, IDC_HOME_WAYPOINT, readComboItems(page, IDC_HOME_WAYPOINT), m_ui->homeWaypointCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	WBQtComboStyle::applySearchable(m_ui->homeWaypointCombo, true);
	bindEdit(page, IDC_DESCRIPTION, m_ui->descriptionEdit, WB_QT_TEAMNOTIFY_CHANGE);
	bindCheck(page, IDC_AUTO_REINFORCE, m_ui->autoReinforceCheck);
	bindCheck(page, IDC_AI_RECRUITABLE, m_ui->aiRecruitableCheck);
	bindCheck(page, IDC_TEAM_SINGLETON, m_ui->singletonCheck);
	connect(m_ui->homeWaypointCombo, SIGNAL(activated(int)), this, SLOT(updateWarnings()));
}

// ---- members ----

void WBQtTeamSheetDialog::setupMembers()
{
	const int page = WB_QT_TEAMPAGE_IDENTITY;
	QGridLayout *grid = m_ui->membersGrid;
	grid->addWidget(new QLabel("Unit", m_ui->membersBody), 0, 0);
	grid->addWidget(new QLabel("Min", m_ui->membersBody), 0, 2);
	grid->addWidget(new QLabel("Max", m_ui->membersBody), 0, 3);

	// The seven slot combos share the template catalog, so enumerate slot 1 once.
	const QStringList unitItems = readComboItems(page, IDC_UNIT_TYPE1);
	for (int i = 0; i < 7; i++)
	{
		m_memberAdded[i] = false;
		QComboBox *combo = new QComboBox(m_ui->membersBody);
		bindCombo(page, kTypeIds[i], unitItems, combo, WB_QT_TEAMNOTIFY_SELCHANGE);
		WBQtComboStyle::applySearchable(combo, true);
		tintIfMissing(combo);
		m_unitCombos[i] = combo;

		QToolButton *pick = smallButton("...", "Pick from the object catalog", m_ui->membersBody);
		QSpinBox *minSpin = new QSpinBox(m_ui->membersBody);
		QSpinBox *maxSpin = new QSpinBox(m_ui->membersBody);
		minSpin->setMaximum(999);
		maxSpin->setMaximum(999);
		bindSpin(page, kMinIds[i], minSpin, WB_QT_TEAMNOTIFY_CHANGE);
		bindSpin(page, kMaxIds[i], maxSpin, WB_QT_TEAMNOTIFY_CHANGE);
		m_minSpins[i] = minSpin;
		m_maxSpins[i] = maxSpin;
		QToolButton *remove = smallButton("Remove", "Remove this unit from the team", m_ui->membersBody);
		remove->setProperty("slot", i);

		grid->addWidget(combo, i + 1, 0);
		grid->addWidget(pick, i + 1, 1);
		grid->addWidget(minSpin, i + 1, 2);
		grid->addWidget(maxSpin, i + 1, 3);
		grid->addWidget(remove, i + 1, 4);
		m_memberWidgets[i][0] = combo;
		m_memberWidgets[i][1] = pick;
		m_memberWidgets[i][2] = minSpin;
		m_memberWidgets[i][3] = maxSpin;
		m_memberWidgets[i][4] = remove;

		const int typeId = kTypeIds[i];
		const int pickId = kPickIds[i];
		connect(pick, &QToolButton::clicked, pick, [this, combo, typeId, pickId, page]()
		{
			// Pops the Qt unit picker; the bridge writes the dict.
			WBQtTeamPage_ClickButton(page, pickId);
			seedComboCurrent(combo, pageText(page, typeId));
			tintIfMissing(combo);
			refreshMembers();
		});
		connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(onUnitTypeChanged()));
		connect(remove, SIGNAL(clicked()), this, SLOT(onRemoveUnit()));
		connect(minSpin, SIGNAL(valueChanged(int)), this, SLOT(updateWarnings()));
		connect(maxSpin, SIGNAL(valueChanged(int)), this, SLOT(updateWarnings()));
	}
	connect(m_ui->addUnitButton, SIGNAL(clicked()), this, SLOT(onAddUnit()));
	refreshMembers();
}

void WBQtTeamSheetDialog::refreshMembers()
{
	int shown = 0;
	int minTotal = 0;
	int maxTotal = 0;
	for (int i = 0; i < 7; i++)
	{
		const bool filled = !isEmptySlot(m_unitCombos[i]->currentText());
		const bool visible = filled || m_memberAdded[i];
		for (int w = 0; w < 5; ++w)
		{
			m_memberWidgets[i][w]->setVisible(visible);
		}
		if (visible)
		{
			++shown;
		}
		if (filled)
		{
			minTotal += m_minSpins[i]->value();
			maxTotal += m_maxSpins[i]->value();
		}
	}
	m_ui->addUnitButton->setEnabled(shown < 7);
	m_ui->totalLabel->setText(QString("Total: %1-%2 units").arg(minTotal).arg(maxTotal));
	updateWarnings();
}

void WBQtTeamSheetDialog::onUnitTypeChanged()
{
	QComboBox *combo = qobject_cast<QComboBox *>(sender());
	if (combo != NULL)
	{
		tintIfMissing(combo);
	}
	refreshMembers();
}

void WBQtTeamSheetDialog::onAddUnit()
{
	for (int i = 0; i < 7; i++)
	{
		if (isEmptySlot(m_unitCombos[i]->currentText()) && !m_memberAdded[i])
		{
			m_memberAdded[i] = true;
			refreshMembers();
			m_unitCombos[i]->setFocus();
			return;
		}
	}
}

void WBQtTeamSheetDialog::onRemoveUnit()
{
	const int i = sender()->property("slot").toInt();
	const int page = WB_QT_TEAMPAGE_IDENTITY;
	WBQtTeamPage_ComboSelectText(page, kTypeIds[i], kNone, WB_QT_TEAMNOTIFY_SELCHANGE);
	m_unitCombos[i]->blockSignals(true);
	seedComboCurrent(m_unitCombos[i], pageText(page, kTypeIds[i]));
	m_unitCombos[i]->blockSignals(false);
	tintIfMissing(m_unitCombos[i]);
	m_minSpins[i]->setValue(0);
	m_maxSpins[i]->setValue(0);
	m_memberAdded[i] = false;
	refreshMembers();
}

// ---- production, reinforcement, behavior ----

void WBQtTeamSheetDialog::setupProduction()
{
	const int page = WB_QT_TEAMPAGE_IDENTITY;
	bindCombo(page, IDC_PRODUCTION_CONDITION, readComboItems(page, IDC_PRODUCTION_CONDITION), m_ui->productionConditionCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	WBQtComboStyle::applySearchable(m_ui->productionConditionCombo, true);
	bindCheck(page, IDC_PRODUCTION_EXECUTEACTIONS, m_ui->executeActionsCheck);
	m_ui->prioritySpin->setRange(-99999, 99999);
	m_ui->increaseSpin->setRange(-99999, 99999);
	m_ui->decreaseSpin->setRange(-99999, 99999);
	bindSpin(page, IDC_PRODUCTION_PRIORITY, m_ui->prioritySpin, WB_QT_TEAMNOTIFY_CHANGE);
	bindSpin(page, IDC_PRIORITY_INCREASE, m_ui->increaseSpin, WB_QT_TEAMNOTIFY_CHANGE);
	bindSpin(page, IDC_PRIORITY_DECREASE, m_ui->decreaseSpin, WB_QT_TEAMNOTIFY_CHANGE);
	bindSpin(page, IDC_TEAM_BUILD_FRAMES, m_ui->framesSpin, WB_QT_TEAMNOTIFY_CHANGE);
	m_ui->conditionView->setProperty("combo", QVariant::fromValue<QObject *>(m_ui->productionConditionCombo));
	connect(m_ui->conditionView, SIGNAL(clicked()), this, SLOT(onViewScript()));
	connect(m_ui->productionConditionCombo, SIGNAL(activated(int)), this, SLOT(updateWarnings()));
	connect(m_ui->prioritySpin, SIGNAL(valueChanged(int)), this, SLOT(updateWarnings()));
}

void WBQtTeamSheetDialog::setupReinforcement()
{
	const int page = WB_QT_TEAMPAGE_REINFORCEMENT;
	bindCheck(page, IDC_DEPLOY_BY, m_ui->deployByCheck);
	bindCombo(page, IDC_TRANSPORT_COMBO, readComboItems(page, IDC_TRANSPORT_COMBO), m_ui->transportCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	// == the MFC IDC_TRANSPORT_COMBO (CBS_DROPDOWN): the only typable combo on this sheet.
	WBQtComboStyle::applyTypeToFilter(m_ui->transportCombo);
	bindCheck(page, IDC_TRANSPORTS_EXIT, m_ui->transportsExitCheck);

	// == TeamReinforcement's OnDeployBy: Deploy By gates the transport combo and Transports Exit.
	QComboBox *transport = m_ui->transportCombo;
	QCheckBox *transportsExit = m_ui->transportsExitCheck;
	connect(m_ui->deployByCheck, &QCheckBox::toggled, this, [page, transport, transportsExit](bool)
	{
		const bool enable = WBQtTeamPage_IsEnabled(page, IDC_TRANSPORT_COMBO) != 0;
		transport->setEnabled(enable);
		transportsExit->setEnabled(WBQtTeamPage_IsEnabled(page, IDC_TRANSPORTS_EXIT) != 0);
		if (!enable)
		{
			transport->setCurrentIndex(-1);	// == MFC unchecking: SetCurSel(-1) + clear dict
		}
	});
	bindCombo(page, IDC_WAYPOINT_COMBO, readComboItems(page, IDC_WAYPOINT_COMBO), m_ui->waypointCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	WBQtComboStyle::applySearchable(m_ui->waypointCombo, true);
	bindCheck(page, IDC_TEAM_STARTS_FULL, m_ui->teamStartsFullCheck);
	bindCombo(page, IDC_VETERANCY, readComboItems(page, IDC_VETERANCY), m_ui->veterancyCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
}

void WBQtTeamSheetDialog::setupBehavior()
{
	const int page = WB_QT_TEAMPAGE_BEHAVIOR;
	// One representative combo enumerates the shared subroutine-script list.
	const QStringList scriptItems = readComboItems(page, IDC_ON_CREATE_SCRIPT);
	QComboBox *combos[] = { m_ui->onCreateCombo, m_ui->onEnemySightedCombo, m_ui->onAllClearCombo,
		m_ui->onDestroyedCombo, m_ui->onIdleCombo, m_ui->onUnitDestroyedCombo };
	QToolButton *views[] = { m_ui->onCreateView, m_ui->onEnemySightedView, m_ui->onAllClearView,
		m_ui->onDestroyedView, m_ui->onIdleView, m_ui->onUnitDestroyedView };
	const int ids[] = { IDC_ON_CREATE_SCRIPT, IDC_ON_ENEMY_SIGHTED, IDC_ON_ALL_CLEAR,
		IDC_ON_DESTROYED, IDC_ON_IDLE_SCRIPT, IDC_ON_UNIT_DESTROYED_SCRIPT };
	for (int i = 0; i < 6; ++i)
	{
		bindCombo(page, ids[i], scriptItems, combos[i], WB_QT_TEAMNOTIFY_SELCHANGE);
		WBQtComboStyle::applySearchable(combos[i], true);
		views[i]->setProperty("combo", QVariant::fromValue<QObject *>(combos[i]));
		views[i]->setToolTip("Show this script");
		connect(views[i], SIGNAL(clicked()), this, SLOT(onViewScript()));
	}
	m_ui->conditionView->setToolTip("Show this script");
	bindSpin(page, IDC_PERCENT_DESTROYED, m_ui->percentSpin, WB_QT_TEAMNOTIFY_CHANGE);

	bindCheck(page, IDC_TRANSPORTS_RETURN, m_ui->transportsReturnCheck);
	bindCheck(page, IDC_AVOID_THREATS, m_ui->avoidThreatsCheck);
	bindCombo(page, IDC_ENEMY_INTERACTIONS, readComboItems(page, IDC_ENEMY_INTERACTIONS), m_ui->enemyInteractionsCombo, WB_QT_TEAMNOTIFY_SELCHANGE);
	bindCheck(page, IDC_ATTACK_COMMON_TARGET, m_ui->attackCommonTargetCheck);
}

void WBQtTeamSheetDialog::onViewScript()
{
	QComboBox *combo = qobject_cast<QComboBox *>(sender()->property("combo").value<QObject *>());
	if (combo != NULL)
	{
		WBQtShowTeamScript(this, combo->currentText());
	}
}

// ---- generic scripts ----

void WBQtTeamSheetDialog::setupGeneric()
{
	const int page = WB_QT_TEAMPAGE_GENERIC;
	// The 16 combos share the subroutine-script list; enumerate slot 1 once.
	const QStringList scriptItems = readComboItems(page, IDC_TeamGeneric_Script1);
	QGridLayout *grid = m_ui->genericGrid;
	for (int i = 0; i < 16; i++)
	{
		QLabel *label = new QLabel(QString("%1.").arg(i + 1), m_ui->genericBody);
		QComboBox *combo = new QComboBox(m_ui->genericBody);
		bindCombo(page, kGenericIds[i], scriptItems, combo, WB_QT_TEAMNOTIFY_SELCHANGE);
		WBQtComboStyle::applySearchable(combo, true);
		QToolButton *view = smallButton("View", "Show this script", m_ui->genericBody);
		view->setProperty("combo", QVariant::fromValue<QObject *>(combo));
		QToolButton *up = smallButton("Up", "Run this script earlier", m_ui->genericBody);
		QToolButton *down = smallButton("Down", "Run this script later", m_ui->genericBody);
		QToolButton *remove = smallButton("Remove", "Remove this script", m_ui->genericBody);
		up->setProperty("slot", i);
		up->setProperty("dir", -1);
		down->setProperty("slot", i);
		down->setProperty("dir", 1);
		remove->setProperty("slot", i);

		QWidget *row[6] = { label, combo, view, up, down, remove };
		for (int w = 0; w < 6; ++w)
		{
			grid->addWidget(row[w], i, w);
			m_genericWidgets[i][w] = row[w];
		}
		m_genericCombos[i] = combo;

		// The bridge compacts the chain after a change; re-seed so the rows show its order.
		connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
			[this](int) { refreshGenericScripts(); });
		connect(view, SIGNAL(clicked()), this, SLOT(onViewScript()));
		connect(up, SIGNAL(clicked()), this, SLOT(onGenericMove()));
		connect(down, SIGNAL(clicked()), this, SLOT(onGenericMove()));
		connect(remove, SIGNAL(clicked()), this, SLOT(onGenericRemove()));
	}
	refreshGenericScripts();
}

void WBQtTeamSheetDialog::refreshGenericScripts()
{
	// == _dictToScripts: the filled slots, then the first empty one to add with, then nothing.
	const int filled = WBQtTeamGeneric_FilledCount();
	const int page = WB_QT_TEAMPAGE_GENERIC;
	for (int i = 0; i < 16; i++)
	{
		const bool visible = (i <= filled);
		for (int w = 0; w < 6; ++w)
		{
			m_genericWidgets[i][w]->setVisible(visible);
		}
		if (!visible)
		{
			continue;
		}
		// A display sync, not an edit, so the combo must not re-notify.
		m_genericCombos[i]->blockSignals(true);
		seedComboCurrent(m_genericCombos[i], pageText(page, kGenericIds[i]));
		m_genericCombos[i]->blockSignals(false);
		const bool isFilled = (i < filled);
		m_genericWidgets[i][2]->setEnabled(isFilled);
		m_genericWidgets[i][3]->setEnabled(isFilled && i > 0);
		m_genericWidgets[i][4]->setEnabled(isFilled && i < filled - 1);
		m_genericWidgets[i][5]->setEnabled(isFilled);
	}
}

void WBQtTeamSheetDialog::onGenericMove()
{
	const int i = sender()->property("slot").toInt();
	const int j = i + sender()->property("dir").toInt();
	if (j < 0 || j >= 16)
	{
		return;
	}
	const int page = WB_QT_TEAMPAGE_GENERIC;
	// Both slots stay filled while they trade places, so the chain never compacts mid-swap.
	const QByteArray first = pageText(page, kGenericIds[i]).toLocal8Bit();
	const QByteArray second = pageText(page, kGenericIds[j]).toLocal8Bit();
	WBQtTeamPage_ComboSelectText(page, kGenericIds[i], second.constData(), WB_QT_TEAMNOTIFY_SELCHANGE);
	WBQtTeamPage_ComboSelectText(page, kGenericIds[j], first.constData(), WB_QT_TEAMNOTIFY_SELCHANGE);
	refreshGenericScripts();
}

void WBQtTeamSheetDialog::onGenericRemove()
{
	const int i = sender()->property("slot").toInt();
	WBQtTeamPage_ComboSelectText(WB_QT_TEAMPAGE_GENERIC, kGenericIds[i], kNone, WB_QT_TEAMNOTIFY_SELCHANGE);
	refreshGenericScripts();
}

// ---- warnings ----

void WBQtTeamSheetDialog::updateWarnings()
{
	QStringList warnings;
	bool hasMembers = false;
	for (int i = 0; i < 7; i++)
	{
		const QString type = m_unitCombos[i]->currentText();
		if (isEmptySlot(type))
		{
			continue;
		}
		hasMembers = true;
		if (m_minSpins[i]->value() > m_maxSpins[i]->value())
		{
			warnings.append(QString("%1 has a minimum of %2 but a maximum of %3.")
				.arg(type).arg(m_minSpins[i]->value()).arg(m_maxSpins[i]->value()));
		}
	}
	if (m_ui->prioritySpin->value() != 0 && isEmptySlot(m_ui->productionConditionCombo->currentText()))
	{
		warnings.append("A production priority is set, but there is no condition script to build the team.");
	}
	if (hasMembers && isEmptySlot(m_ui->homeWaypointCombo->currentText()))
	{
		warnings.append("No home position is set.");
	}

	QLabel *label = m_ui->warningsLabel;
	if (warnings.isEmpty())
	{
		label->hide();
		return;
	}
	label->setStyleSheet("QLabel { border-left: 4px solid #d7a03c; background-color: rgba(215, 160, 60, 40); padding: 6px 8px; }");
	label->setText(warnings.join("\n"));
	label->show();
}
