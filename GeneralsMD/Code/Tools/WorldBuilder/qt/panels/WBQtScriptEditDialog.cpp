// WBQtScriptEditDialog.cpp -- see WBQtScriptEditDialog.h.
#include "WBQtScriptEditDialog.h"
#include "ui_WBQtScriptEditDialog.h"
#include "WBQtScriptEditBridge.h"
#include "WBQtScriptWindow.h"
#include "WBQtScriptEditDialogClassic.h"

// The script editor's "New design" setting (WBQtScriptBridge.cpp).
extern "C" int WBQtScript_GetNewDesign(void);

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include <qt_windows.h>

// Stage 1 phase 3: modal-dialog parent (active modal if nested, else main window). WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

namespace
{
	const int kLabelCap = 1024;
	const int kCommentCap = 8192;

	// Row data: kind (1 item, 0 IF/OR header), a warning flag, the OR group, and the gray prefix.
	const int kKindRole = Qt::UserRole;
	const int kWarningRole = Qt::UserRole + 1;
	const int kGroupRole = Qt::UserRole + 2;
	const int kPrefixRole = Qt::UserRole + 3;

	const QColor kWarningColour(200, 70, 70);

	QString bridgeText(void *script, int field)
	{
		char buf[kCommentCap];
		buf[0] = 0;
		WBQtScriptEditData_GetText(script, field, buf, sizeof(buf));
		return QString::fromLocal8Bit(buf);
	}

	// Size a note to its text, between minLines and maxLines.
	void fitNote(QPlainTextEdit *note, int minLines, int maxLines)
	{
		const int lines = qBound(minLines, (int)note->document()->documentLayout()->documentSize().height(), maxLines);
		note->setFixedHeight(lines * note->fontMetrics().lineSpacing()
			+ qRound(note->document()->documentMargin() * 2.0) + note->frameWidth() * 2);
	}

	// IF/OR headers as dividers, OR groups as tinted bands, AND or number prefixes, warnings in red.
	class RowDelegate : public QStyledItemDelegate
	{
	public:
		explicit RowDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

		virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
			const QModelIndex &index) const
		{
			QStyleOptionViewItem opt(option);
			initStyleOption(&opt, index);
			const QString text = opt.text;
			opt.text.clear();
			const QWidget *widget = option.widget;
			QStyle *style = widget ? widget->style() : QApplication::style();
			const QRect r = option.rect;
			const QFontMetrics metrics(opt.font);

			painter->save();
			if (index.data(kKindRole).toInt() == 0)
			{
				const int y = r.center().y();
				painter->setPen(option.palette.color(QPalette::Mid));
				painter->drawLine(r.left() + 4, y, r.right() - 4, y);
				QFont bold = opt.font;
				bold.setBold(true);
				const QRect pill(r.left() + 12, y - metrics.height() / 2,
					QFontMetrics(bold).horizontalAdvance(text) + 16, metrics.height());
				painter->setRenderHint(QPainter::Antialiasing, true);
				painter->setPen(Qt::NoPen);
				painter->setBrush(text == "OR" ? QColor(160, 100, 40) : QColor(55, 105, 165));
				painter->drawRoundedRect(pill, 3.0, 3.0);
				painter->setPen(Qt::white);
				painter->setFont(bold);
				painter->drawText(pill, Qt::AlignCenter, text);
				painter->restore();
				return;
			}

			const QVariant group = index.data(kGroupRole);
			if (group.isValid() && (group.toInt() % 2) == 1)
			{
				painter->fillRect(r, QColor(255, 255, 255, 14));
			}
			style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

			const bool warn = index.data(kWarningRole).toBool();
			QColor bar = warn ? kWarningColour : QColor(55, 105, 165);
			if (group.isValid() && (group.toInt() % 2) == 1 && !warn)
			{
				bar = QColor(55, 130, 75);
			}
			painter->fillRect(QRect(r.left(), r.top() + 1, 3, r.height() - 2), bar);

			// The prefix column has a fixed width, so the text lines up row to row.
			const QString prefix = index.data(kPrefixRole).toString();
			const int prefixWidth = group.isValid()
				? metrics.horizontalAdvance("AND") + 14 : metrics.horizontalAdvance("999.") + 8;
			const int x = r.left() + 10;
			if (!prefix.isEmpty())
			{
				painter->setPen(option.palette.color(QPalette::Disabled, QPalette::Text));
				painter->drawText(QRect(x, r.top(), prefixWidth, r.height()),
					Qt::AlignVCenter | Qt::AlignLeft, prefix);
			}
			const bool selected = (option.state & QStyle::State_Selected) != 0;
			painter->setPen(warn ? QColor(235, 95, 95)
				: option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));
			const QRect textRect(x + prefixWidth, r.top(), r.right() - x - prefixWidth - 4, r.height());
			painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
				metrics.elidedText(text, Qt::ElideRight, textRect.width()));
			painter->restore();
		}

		virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
		{
			QSize size = QStyledItemDelegate::sizeHint(option, index);
			size.setHeight(size.height() + 6);
			return size;
		}
	};

	// A list whose drops reorder the script through the section instead of moving list items.
	class DragList : public QListWidget
	{
	public:
		DragList(WBQtScriptEditSection *section, QWidget *parent)
			: QListWidget(parent), m_section(section), m_from(-1), m_to(-1)
		{
			setDragDropMode(QAbstractItemView::InternalMove);
			setDefaultDropAction(Qt::MoveAction);
		}

		int takeFrom() { const int v = m_from; m_from = -1; return v; }
		int takeTo() { const int v = m_to; m_to = -1; return v; }

	protected:
		virtual void dropEvent(QDropEvent *event)
		{
			QListWidgetItem *target = itemAt(event->pos());
			m_from = currentRow();
			m_to = (target != NULL) ? row(target) : count() - 1;
			// Nothing moves in the widget itself; the section rebuilds it once the drag is over.
			event->setDropAction(Qt::IgnoreAction);
			event->accept();
			QMetaObject::invokeMethod(m_section, "onDropped", Qt::QueuedConnection);
		}

	private:
		WBQtScriptEditSection *m_section;
		int m_from;
		int m_to;
	};

	// The key with exactly these Ctrl/Alt/Shift modifiers; the keypad flag is ignored.
	bool keyIs(const QKeyEvent *key, int code, Qt::KeyboardModifiers mods)
	{
		const Qt::KeyboardModifiers held = key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
		return key->key() == code && held == mods;
	}

	QToolButton *makeToolButton(const QString &text, const QString &tip, QWidget *parent)
	{
		QToolButton *button = new QToolButton(parent);
		button->setText(text);
		button->setToolTip(tip);
		button->setAutoRaise(true);
		return button;
	}
}

// ===================== WBQtScriptEditSection =====================

WBQtScriptEditSection::WBQtScriptEditSection(void *script, Mode mode, QWidget *parent)
	: QWidget(parent),
	m_script(script),
	m_mode(mode),
	m_updating(false),
	m_note(NULL),
	m_orButton(NULL),
	m_otherButton(NULL)
{
	QVBoxLayout *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(2);

	QHBoxLayout *header = new QHBoxLayout();
	header->setSpacing(2);
	m_toggle = new QToolButton(this);
	m_toggle->setAutoRaise(true);
	m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	QFont bold = m_toggle->font();
	bold.setBold(true);
	m_toggle->setFont(bold);
	header->addWidget(m_toggle);
	header->addStretch(1);

	QToolButton *newButton = makeToolButton("New", "New (Ins)", this);
	m_editButton = makeToolButton("Edit", "Edit (Enter or double-click)", this);
	m_copyButton = makeToolButton("Copy", "Duplicate (Ctrl+D)", this);
	m_deleteButton = makeToolButton("Delete", "Delete (Del)", this);
	header->addWidget(newButton);
	header->addWidget(m_editButton);
	header->addWidget(m_copyButton);
	header->addWidget(m_deleteButton);
	if (m_mode == ModeConditions)
	{
		m_orButton = makeToolButton("OR", "Start a new OR group", this);
		header->addWidget(m_orButton);
	}
	else
	{
		m_otherButton = makeToolButton((m_mode == ModeActionsTrue) ? "To ELSE" : "To THEN",
			(m_mode == ModeActionsTrue) ? "Move to the actions if false" : "Move to the actions if true", this);
		header->addWidget(m_otherButton);
	}
	m_upButton = makeToolButton("Up", "Move up (Alt+Up, or drag)", this);
	m_downButton = makeToolButton("Down", "Move down (Alt+Down, or drag)", this);
	header->addWidget(m_upButton);
	header->addWidget(m_downButton);
	outer->addLayout(header);

	m_body = new QWidget(this);
	QVBoxLayout *bodyLayout = new QVBoxLayout(m_body);
	bodyLayout->setContentsMargins(0, 0, 0, 0);
	bodyLayout->setSpacing(4);
	DragList *list = new DragList(this, m_body);
	m_list = list;
	m_list->setItemDelegate(new RowDelegate(m_list));
	m_list->setMinimumHeight(90);
	m_list->installEventFilter(this);
	bodyLayout->addWidget(m_list);
	if (m_mode == ModeConditions)
	{
		m_note = new QPlainTextEdit(m_body);
		m_note->setPlaceholderText("Conditions note");
		bodyLayout->addWidget(m_note);
		connect(m_note, SIGNAL(textChanged()), this, SLOT(onNoteChanged()));
	}
	outer->addWidget(m_body, 1);

	connect(m_toggle, SIGNAL(clicked()), this, SLOT(onToggle()));
	connect(m_list, SIGNAL(currentRowChanged(int)), this, SLOT(onSelectionChanged()));
	connect(m_list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onEdit()));
	connect(newButton, SIGNAL(clicked()), this, SLOT(onNew()));
	connect(m_editButton, SIGNAL(clicked()), this, SLOT(onEdit()));
	connect(m_copyButton, SIGNAL(clicked()), this, SLOT(onCopy()));
	connect(m_deleteButton, SIGNAL(clicked()), this, SLOT(onDelete()));
	if (m_orButton != NULL)
	{
		connect(m_orButton, SIGNAL(clicked()), this, SLOT(onOr()));
	}
	if (m_otherButton != NULL)
	{
		connect(m_otherButton, SIGNAL(clicked()), this, SLOT(onMoveToOther()));
	}
	connect(m_upButton, SIGNAL(clicked()), this, SLOT(onMoveUp()));
	connect(m_downButton, SIGNAL(clicked()), this, SLOT(onMoveDown()));

	// Ctrl+C / Ctrl+V: the cross-script clipboard, only while this list has focus.
	QShortcut *copySc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_C), m_list);
	copySc->setContext(Qt::WidgetShortcut);
	connect(copySc, SIGNAL(activated()), this, SLOT(onCopyClipboard()));
	QShortcut *pasteSc = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_V), m_list);
	pasteSc->setContext(Qt::WidgetShortcut);
	connect(pasteSc, SIGNAL(activated()), this, SLOT(onPasteClipboard()));

	setExpanded(true);
	// The first condition sits under the IF header, on row 1.
	reload((m_mode == ModeConditions) ? 1 : 0);
}

int WBQtScriptEditSection::currentRow() const
{
	return m_list->currentRow();
}

int WBQtScriptEditSection::isFalse() const
{
	return (m_mode == ModeActionsFalse) ? 1 : 0;
}

void WBQtScriptEditSection::reload(int selectRow)
{
	m_updating = true;
	if (selectRow < 0)
	{
		selectRow = m_list->currentRow();
	}
	m_list->clear();
	char buf[kLabelCap];
	int items = 0;
	if (m_mode == ModeConditions)
	{
		const int count = WBQtScriptEditData_GetConditionRowCount(m_script);
		int group = -1;
		for (int i = 0; i < count; i++)
		{
			const int kind = WBQtScriptEditData_GetConditionRow(m_script, i, buf, sizeof(buf));
			QString label = QString::fromLocal8Bit(buf);
			QListWidgetItem *item = new QListWidgetItem(m_list);
			item->setData(kKindRole, kind);
			if (kind == 0)
			{
				++group;
				item->setText(label.contains("OR") ? "OR" : "IF");
				item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
				continue;
			}
			++items;
			const bool isAnd = label.startsWith("  *AND* ");
			label = isAnd ? label.mid(8) : label.trimmed();
			item->setText(label);
			item->setToolTip(label);
			item->setData(kGroupRole, group);
			item->setData(kPrefixRole, isAnd ? "AND" : "");
			item->setData(kWarningRole, WBQtScriptEditData_GetConditionRowWarning(m_script, i) != 0);
		}
	}
	else
	{
		const int count = WBQtScriptEditData_GetActionCount(m_script, isFalse());
		for (int i = 0; i < count; i++)
		{
			WBQtScriptEditData_GetActionLabel(m_script, isFalse(), i, buf, sizeof(buf));
			const QString label = QString::fromLocal8Bit(buf);
			QListWidgetItem *item = new QListWidgetItem(label, m_list);
			item->setToolTip(label);
			item->setData(kKindRole, 1);
			item->setData(kPrefixRole, QString("%1.").arg(i + 1));
			item->setData(kWarningRole, WBQtScriptEditData_GetActionWarning(m_script, isFalse(), i) != 0);
		}
		items = count;
	}
	if (m_list->count() > 0)
	{
		m_list->setCurrentRow(qBound(0, selectRow, m_list->count() - 1));
	}

	static const char *const kTitles[] = { "IF", "THEN", "ELSE" };
	static const char *const kSubtitles[] = { "conditions", "actions if true", "actions if false" };
	m_toggle->setText(QString("%1   %2 (%3)").arg(kTitles[m_mode]).arg(kSubtitles[m_mode]).arg(items));

	if (m_note != NULL)
	{
		const QString note = bridgeText(m_script, WB_QT_SCRIPTEDIT_TEXT_CONDITION_COMMENT);
		if (m_note->toPlainText() != note)
		{
			m_note->setPlainText(note);
		}
		fitNote(m_note, 1, 6);
	}
	m_updating = false;
	updateButtonStates();
}

void WBQtScriptEditSection::setExpanded(bool expanded)
{
	m_body->setVisible(expanded);
	m_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
	// A collapsed section shrinks to its header and gives its height to the open ones.
	setSizePolicy(QSizePolicy::Preferred, expanded ? QSizePolicy::Expanding : QSizePolicy::Maximum);
}

void WBQtScriptEditSection::focusList()
{
	m_list->setFocus();
}

void WBQtScriptEditSection::onToggle()
{
	setExpanded(!m_body->isVisible());
}

void WBQtScriptEditSection::updateButtonStates()
{
	const int row = currentRow();
	bool isItem = false;
	if (row >= 0)
	{
		QListWidgetItem *item = m_list->item(row);
		isItem = (item != NULL && item->data(kKindRole).toInt() == 1);
	}
	m_editButton->setEnabled(isItem);
	m_copyButton->setEnabled(isItem);
	if (m_mode == ModeConditions)
	{
		// A header row can be deleted, which merges its OR group into the one above.
		m_deleteButton->setEnabled(row >= 0);
	}
	else
	{
		m_deleteButton->setEnabled(isItem);
		m_otherButton->setEnabled(isItem);
		m_upButton->setEnabled(isItem && row > 0);
		m_downButton->setEnabled(isItem && row < m_list->count() - 1);
	}
}

bool WBQtScriptEditSection::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_list && event->type() == QEvent::KeyPress)
	{
		QKeyEvent *key = static_cast<QKeyEvent *>(event);
		if (keyIs(key, Qt::Key_Return, Qt::NoModifier) || keyIs(key, Qt::Key_Enter, Qt::NoModifier))
		{
			onEdit();
			return true;
		}
		if (keyIs(key, Qt::Key_Delete, Qt::NoModifier))
		{
			onDelete();
			return true;
		}
		if (keyIs(key, Qt::Key_Insert, Qt::NoModifier))
		{
			onNew();
			return true;
		}
		if (keyIs(key, Qt::Key_D, Qt::ControlModifier))
		{
			onCopy();
			return true;
		}
		if (keyIs(key, Qt::Key_Up, Qt::AltModifier))
		{
			onMoveUp();
			return true;
		}
		if (keyIs(key, Qt::Key_Down, Qt::AltModifier))
		{
			onMoveDown();
			return true;
		}
	}
	return QWidget::eventFilter(watched, event);
}

void WBQtScriptEditSection::onSelectionChanged()
{
	if (!m_updating)
	{
		updateButtonStates();
	}
}

void WBQtScriptEditSection::apply(int newRow)
{
	if (newRow >= 0)
	{
		reload(newRow);
	}
}

void WBQtScriptEditSection::onNew()
{
	apply((m_mode == ModeConditions)
		? WBQtScriptEdit_ConditionNew(m_script, currentRow())
		: WBQtScriptEdit_ActionNew(m_script, isFalse(), currentRow()));
}

void WBQtScriptEditSection::onEdit()
{
	apply((m_mode == ModeConditions)
		? WBQtScriptEdit_ConditionEdit(m_script, currentRow())
		: WBQtScriptEdit_ActionEdit(m_script, isFalse(), currentRow()));
}

void WBQtScriptEditSection::onCopy()
{
	apply((m_mode == ModeConditions)
		? WBQtScriptEdit_ConditionCopy(m_script, currentRow())
		: WBQtScriptEdit_ActionCopy(m_script, isFalse(), currentRow()));
}

void WBQtScriptEditSection::onCopyClipboard()
{
	// Stashes the selected item; the list itself does not change.
	if (m_mode == ModeConditions)
	{
		WBQtScriptEdit_ConditionCopyToClipboard(m_script, currentRow());
	}
	else
	{
		WBQtScriptEdit_ActionCopyToClipboard(m_script, isFalse(), currentRow());
	}
}

void WBQtScriptEditSection::onPasteClipboard()
{
	apply((m_mode == ModeConditions)
		? WBQtScriptEdit_ConditionPasteFromClipboard(m_script, currentRow())
		: WBQtScriptEdit_ActionPasteFromClipboard(m_script, isFalse(), currentRow()));
}

void WBQtScriptEditSection::onDelete()
{
	const int newRow = (m_mode == ModeConditions)
		? WBQtScriptEdit_ConditionDelete(m_script, currentRow())
		: WBQtScriptEdit_ActionDelete(m_script, isFalse(), currentRow());
	if (newRow >= 0)
	{
		reload(newRow);
	}
	else if (currentRow() >= 0)
	{
		// Deleting the last remaining row leaves nothing to select; still rebuild.
		reload(0);
	}
}

void WBQtScriptEditSection::onOr()
{
	apply(WBQtScriptEdit_ConditionOr(m_script, currentRow()));
}

void WBQtScriptEditSection::onMoveToOther()
{
	const int newRow = WBQtScriptEdit_ActionMoveToOther(m_script, isFalse(), currentRow());
	if (newRow >= 0)
	{
		reload(newRow);
		emit otherListChanged();
	}
}

void WBQtScriptEditSection::onMoveUp()
{
	apply((m_mode == ModeConditions)
		? WBQtScriptEdit_ConditionMoveUp(m_script, currentRow())
		: WBQtScriptEdit_ActionMoveUp(m_script, isFalse(), currentRow()));
}

void WBQtScriptEditSection::onMoveDown()
{
	apply((m_mode == ModeConditions)
		? WBQtScriptEdit_ConditionMoveDown(m_script, currentRow())
		: WBQtScriptEdit_ActionMoveDown(m_script, isFalse(), currentRow()));
}

void WBQtScriptEditSection::onDropped()
{
	DragList *list = static_cast<DragList *>(m_list);
	moveRow(list->takeFrom(), list->takeTo());
}

void WBQtScriptEditSection::moveRow(int from, int to)
{
	if (from < 0 || to < 0 || from == to)
	{
		return;
	}
	int row = from;
	// Each bridge move is one step; the cap stops a move the bridge refuses from looping.
	for (int step = 0; row != to && step < m_list->count() * 2; ++step)
	{
		const int next = (to > row)
			? ((m_mode == ModeConditions) ? WBQtScriptEdit_ConditionMoveDown(m_script, row)
				: WBQtScriptEdit_ActionMoveDown(m_script, isFalse(), row))
			: ((m_mode == ModeConditions) ? WBQtScriptEdit_ConditionMoveUp(m_script, row)
				: WBQtScriptEdit_ActionMoveUp(m_script, isFalse(), row));
		if (next < 0 || next == row)
		{
			break;
		}
		row = next;
	}
	reload(row);
}

void WBQtScriptEditSection::onNoteChanged()
{
	fitNote(m_note, 1, 6);
	if (m_updating)
	{
		return;
	}
	const QByteArray text = m_note->toPlainText().toLocal8Bit();
	WBQtScriptEditData_SetText(m_script, WB_QT_SCRIPTEDIT_TEXT_CONDITION_COMMENT, text.constData());
}

// ===================== WBQtScriptEditDialog =====================

WBQtScriptEditDialog::WBQtScriptEditDialog(void *script, QWidget *parent)
	: QDialog(parent),
	m_ui(new Ui::WBQtScriptEditDialog),
	m_script(script),
	m_updating(false)
{
	setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowMaximizeButtonHint);
	m_ui->setupUi(this);

	// The flag chips: checkable, outlined when off and filled when on.
	m_ui->activeChip->setProperty("scriptFlag", WB_QT_SCRIPTEDIT_FLAG_ACTIVE);
	m_ui->subroutineChip->setProperty("scriptFlag", WB_QT_SCRIPTEDIT_FLAG_SUBROUTINE);
	m_ui->oneShotChip->setProperty("scriptFlag", WB_QT_SCRIPTEDIT_FLAG_ONE_SHOT);
	m_ui->easyChip->setProperty("scriptFlag", WB_QT_SCRIPTEDIT_FLAG_EASY);
	m_ui->normalChip->setProperty("scriptFlag", WB_QT_SCRIPTEDIT_FLAG_NORMAL);
	m_ui->hardChip->setProperty("scriptFlag", WB_QT_SCRIPTEDIT_FLAG_HARD);
	const QList<QToolButton *> chips = flagChips();
	for (int i = 0; i < chips.size(); ++i)
	{
		chips.at(i)->setCheckable(true);
		chips.at(i)->setStyleSheet(
			"QToolButton { border: 1px solid palette(mid); border-radius: 10px; padding: 2px 10px; }"
			"QToolButton:checked { background-color: #37699f; border-color: #37699f; color: white; }");
		connect(chips.at(i), SIGNAL(toggled(bool)), this, SLOT(onFlagToggled(bool)));
	}

	m_conditions = new WBQtScriptEditSection(m_script, WBQtScriptEditSection::ModeConditions, m_ui->sectionsHost);
	m_actionsTrue = new WBQtScriptEditSection(m_script, WBQtScriptEditSection::ModeActionsTrue, m_ui->sectionsHost);
	m_actionsFalse = new WBQtScriptEditSection(m_script, WBQtScriptEditSection::ModeActionsFalse, m_ui->sectionsHost);
	m_ui->sectionsLayout->addWidget(m_conditions);
	m_ui->sectionsLayout->addWidget(m_actionsTrue);
	m_ui->sectionsLayout->addWidget(m_actionsFalse);

	connect(m_actionsTrue, SIGNAL(otherListChanged()), this, SLOT(onFalseListChanged()));
	connect(m_actionsFalse, SIGNAL(otherListChanged()), this, SLOT(onTrueListChanged()));
	connect(m_ui->nameEdit, SIGNAL(textChanged(QString)), this, SLOT(onNameChanged(QString)));
	connect(m_ui->commentEdit, SIGNAL(textChanged()), this, SLOT(onCommentChanged()));
	connect(m_ui->actionNoteEdit, SIGNAL(textChanged()), this, SLOT(onActionNoteChanged()));
	connect(m_ui->evalCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onEvalChanged(int)));
	connect(m_ui->secondsSpin, SIGNAL(valueChanged(int)), this, SLOT(onSecondsChanged(int)));
	connect(m_ui->smartCopyCheck, SIGNAL(toggled(bool)), this, SLOT(onSmartCopyToggled(bool)));
	connect(m_ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(m_ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	seedProperties();
	resize(880, 760);
}

WBQtScriptEditDialog::~WBQtScriptEditDialog()
{
	delete m_ui;
}

void WBQtScriptEditDialog::applyInitialFocus(int tab, int row)
{
	WBQtScriptEditSection *section = NULL;
	if (tab == 1)
	{
		section = m_conditions;
	}
	else if (tab == 2)
	{
		section = m_actionsTrue;
	}
	else if (tab == 3)
	{
		section = m_actionsFalse;
	}
	if (section == NULL)
	{
		m_ui->nameEdit->setFocus();
		return;
	}
	section->setExpanded(true);
	section->reload(row);
	section->focusList();
}

void WBQtScriptEditDialog::seedProperties()
{
	m_updating = true;
	const QString name = bridgeText(m_script, WB_QT_SCRIPTEDIT_TEXT_NAME);
	m_ui->nameEdit->setText(name);
	setWindowTitle(name.isEmpty() ? QString("Script") : name);
	m_ui->commentEdit->setPlainText(bridgeText(m_script, WB_QT_SCRIPTEDIT_TEXT_COMMENT));
	m_ui->actionNoteEdit->setPlainText(bridgeText(m_script, WB_QT_SCRIPTEDIT_TEXT_ACTION_COMMENT));
	fitNote(m_ui->commentEdit, 2, 5);
	fitNote(m_ui->actionNoteEdit, 1, 6);

	const QList<QToolButton *> chips = flagChips();
	for (int i = 0; i < chips.size(); ++i)
	{
		chips.at(i)->setChecked(WBQtScriptEditData_GetFlag(m_script, chips.at(i)->property("scriptFlag").toInt()) != 0);
	}

	const int delay = WBQtScriptEditData_GetDelaySeconds(m_script);
	m_ui->evalCombo->setCurrentIndex(delay > 0 ? 1 : 0);
	if (delay > 0)
	{
		m_ui->secondsSpin->setValue(delay);
	}
	m_ui->secondsSpin->setEnabled(delay > 0);
	m_ui->smartCopyCheck->setChecked(WBQtScriptEdit_GetSmartCopy() != 0);
	m_updating = false;
}

QList<QToolButton *> WBQtScriptEditDialog::flagChips() const
{
	QList<QToolButton *> chips;
	chips << m_ui->activeChip << m_ui->subroutineChip << m_ui->oneShotChip
		<< m_ui->easyChip << m_ui->normalChip << m_ui->hardChip;
	return chips;
}

void WBQtScriptEditDialog::onFlagToggled(bool on)
{
	if (!m_updating)
	{
		WBQtScriptEditData_SetFlag(m_script, sender()->property("scriptFlag").toInt(), on ? 1 : 0);
	}
}

void WBQtScriptEditDialog::onNameChanged(const QString &text)
{
	if (m_updating)
	{
		return;
	}
	const QByteArray name = text.toLocal8Bit();
	WBQtScriptEditData_SetText(m_script, WB_QT_SCRIPTEDIT_TEXT_NAME, name.constData());
	setWindowTitle(text);
}

void WBQtScriptEditDialog::onCommentChanged()
{
	fitNote(m_ui->commentEdit, 2, 5);
	if (m_updating)
	{
		return;
	}
	const QByteArray text = m_ui->commentEdit->toPlainText().toLocal8Bit();
	WBQtScriptEditData_SetText(m_script, WB_QT_SCRIPTEDIT_TEXT_COMMENT, text.constData());
}

void WBQtScriptEditDialog::onActionNoteChanged()
{
	fitNote(m_ui->actionNoteEdit, 1, 6);
	if (m_updating)
	{
		return;
	}
	const QByteArray text = m_ui->actionNoteEdit->toPlainText().toLocal8Bit();
	WBQtScriptEditData_SetText(m_script, WB_QT_SCRIPTEDIT_TEXT_ACTION_COMMENT, text.constData());
}

void WBQtScriptEditDialog::onEvalChanged(int index)
{
	m_ui->secondsSpin->setEnabled(index == 1);
	if (!m_updating)
	{
		WBQtScriptEditData_SetDelaySeconds(m_script, (index == 1) ? m_ui->secondsSpin->value() : 0);
	}
}

void WBQtScriptEditDialog::onSecondsChanged(int value)
{
	if (m_updating)
	{
		return;
	}
	// Typing a delay switches to per-seconds evaluation.
	if (m_ui->evalCombo->currentIndex() != 1)
	{
		m_ui->evalCombo->setCurrentIndex(1);
	}
	WBQtScriptEditData_SetDelaySeconds(m_script, value);
}

void WBQtScriptEditDialog::onSmartCopyToggled(bool on)
{
	if (!m_updating)
	{
		WBQtScriptEdit_SetSmartCopy(on ? 1 : 0);
	}
}

void WBQtScriptEditDialog::onTrueListChanged()
{
	m_actionsTrue->reload(-1);
}

void WBQtScriptEditDialog::onFalseListChanged()
{
	m_actionsFalse->reload(-1);
}

// ===================== the modal entry point =====================

namespace
{
	// One-shot initial section/row for the next Run (WBQtScriptEdit_SetInitialFocus); -1 = none.
	int s_initialFocusTab = -1;
	int s_initialFocusRow = -1;
}

extern "C" void WBQtScriptEdit_SetInitialFocus(int tab, int row)
{
	s_initialFocusTab = tab;
	s_initialFocusRow = row;
}

// Either design runs the same way: modal, focused, and owning the MFC sub-modals the bridge pops.
template <class Dialog>
static int runScriptEdit(Dialog &dlg, int initialTab, int initialRow)
{
	dlg.setWindowModality(Qt::ApplicationModal);
	if (initialTab >= 0)
	{
		dlg.applyInitialFocus(initialTab, initialRow);
	}
	WBQtScriptEdit_SetModalOwner(reinterpret_cast<void *>(dlg.winId()));
	const int rc = dlg.exec();
	WBQtScriptEdit_SetModalOwner(NULL);
	return (rc == QDialog::Accepted) ? 1 : 0;
}

extern "C" int WBQtScriptEdit_Run(void *script, void * /*frameHwnd*/)
{
	// Snapshot + clear the one-shot focus first, so it can never leak into a later Run.
	const int initialTab = s_initialFocusTab;
	const int initialRow = s_initialFocusRow;
	s_initialFocusTab = -1;
	s_initialFocusRow = -1;

	if (script == NULL)
	{
		return 0;
	}
	// Transient to the Qt Script window when it is up, else the main window.
	QWidget *owner = WBQtScriptWindow::instance();
	if (owner == NULL || !owner->isVisible())
	{
		owner = WBQt_DialogParent();
	}
	if (WBQtScript_GetNewDesign() == 0)
	{
		WBQtScriptEditDialogClassic classic(script, owner);
		return runScriptEdit(classic, initialTab, initialRow);
	}
	WBQtScriptEditDialog dlg(script, owner);
	return runScriptEdit(dlg, initialTab, initialRow);
}
