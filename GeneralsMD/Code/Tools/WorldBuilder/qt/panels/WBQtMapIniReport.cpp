// WBQtMapIniReport.cpp -- see WBQtMapIniReport.h.
#include "WBQtMapIniReport.h"

#ifdef RTS_HAS_QT

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStringList>
#include <QTextBlock>
#include <QToolButton>
#include <QVBoxLayout>

#include "WBQtMapIniCodeEditor.h"

// Stage 1 phase 3: the parent for a modal Qt dialog (active modal if nested, else the
// main window). Defined in WBQtBridge.cpp.
QWidget *WBQt_DialogParent(void);

// The dialog class lives in its own header (WBQtMapIniReportPrivate.h) so AUTOMOC generates
// its moc the standard way; the public WBQtMapIniReport.h stays a pure C facade for the MFC
// side. See the include below.
#include "WBQtMapIniReportPrivate.h"
#include "ui_WBQtMapIniReportDialog.h"

namespace
{
	struct StagedMessage
	{
		int kind;
		QString title;
		QString body;
	};

	QStringList s_summary;
	QList<StagedMessage> s_messages;
	QList<WBQtMapIniBlockData> s_blocks;

	// Taller blocks scroll inside their card; the pop-out shows them whole.
	const int kMaxInlineLines = 24;

	// A read-only view of one block: map.ini line numbers, editor colours, blanked lines in red.
	WBQtMapIniCodeEditor *makeCodeView(const WBQtMapIniBlockData &data, QWidget *parent)
	{
		WBQtMapIniCodeEditor *view = WBQtMapIniCodeEditor::createReadOnly(data.source, data.firstLine, parent);

		QList<QTextEdit::ExtraSelection> marks;
		for (int i = 0; i < data.blanked.size(); ++i)
		{
			QTextBlock block = view->document()->findBlockByNumber(data.blanked.at(i));
			if (!block.isValid())
			{
				continue;
			}
			QTextEdit::ExtraSelection mark;
			mark.format.setBackground(QColor(110, 40, 40));
			mark.format.setProperty(QTextFormat::FullWidthSelection, true);
			mark.cursor = QTextCursor(block);
			marks.append(mark);
		}
		view->setExtraSelections(marks);
		return view;
	}

	// Size a card's view to its text, up to maxLines; taller blocks scroll.
	void fitHeight(QPlainTextEdit *view, int maxLines)
	{
		int lines = view->document()->blockCount();
		if (lines > maxLines)
		{
			lines = maxLines;
		}
		const int textHeight = lines * view->fontMetrics().lineSpacing()
			+ qRound(view->document()->documentMargin() * 2.0);
		view->setFixedHeight(textHeight + view->frameWidth() * 2
			+ view->horizontalScrollBar()->sizeHint().height() + 2);
	}

	QString blockTitle(const WBQtMapIniBlockData &data)
	{
		return data.name.isEmpty() ? data.store : data.store + " " + data.name;
	}
}

// ===================== one block's card =====================

WBQtMapIniBlockCard::WBQtMapIniBlockCard(const WBQtMapIniBlockData &data, QWidget *parent)
	: QFrame(parent),
	  m_data(data),
	  m_toggle(NULL),
	  m_code(NULL)
{
	setFrameShape(QFrame::StyledPanel);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 4, 6, 6);
	layout->setSpacing(4);

	QHBoxLayout *header = new QHBoxLayout();
	header->setSpacing(8);

	m_toggle = new QToolButton(this);
	m_toggle->setAutoRaise(true);
	header->addWidget(m_toggle);

	QFont titleFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	titleFont.setBold(true);
	QLabel *title = new QLabel(blockTitle(data), this);
	title->setFont(titleFont);
	header->addWidget(title);

	QLabel *badge = new QLabel(WBQtMapIniStatusText(data.status), this);
	badge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 3px; padding: 1px 6px;")
		.arg(WBQtMapIniStatusColour(data.status).name()));
	header->addWidget(badge);

	if (!data.blanked.isEmpty())
	{
		QLabel *skipped = new QLabel(QString("%1 line(s) skipped").arg(data.blanked.size()), this);
		skipped->setStyleSheet("color: #e07070;");
		header->addWidget(skipped);
	}

	header->addStretch(1);

	QLabel *line = new QLabel(QString("line %1").arg(data.firstLine), this);
	line->setEnabled(false);
	header->addWidget(line);

	QToolButton *copy = new QToolButton(this);
	copy->setText("Copy");
	copy->setToolTip("Copy this block");
	copy->setAutoRaise(true);
	header->addWidget(copy);

	QToolButton *popOut = new QToolButton(this);
	popOut->setText("Pop out");
	popOut->setToolTip("Open this block in its own window");
	popOut->setAutoRaise(true);
	header->addWidget(popOut);

	layout->addLayout(header);

	m_code = makeCodeView(data, this);
	fitHeight(m_code, kMaxInlineLines);
	layout->addWidget(m_code);

	connect(m_toggle, SIGNAL(clicked()), this, SLOT(onToggle()));
	connect(copy, SIGNAL(clicked()), this, SLOT(onCopy()));
	connect(popOut, SIGNAL(clicked()), this, SLOT(onPopOut()));

	// Dropped blocks open by default, since they are what the user has to act on.
	setExpanded(data.status == WBQT_MAPINI_BLOCK_DROPPED);
}

void WBQtMapIniBlockCard::setExpanded(bool expanded)
{
	m_code->setVisible(expanded);
	m_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
}

bool WBQtMapIniBlockCard::titleMatches(const QString &needle) const
{
	return needle.isEmpty() || blockTitle(m_data).contains(needle, Qt::CaseInsensitive);
}

bool WBQtMapIniBlockCard::matches(const QString &needle) const
{
	return titleMatches(needle) || m_data.source.contains(needle, Qt::CaseInsensitive);
}

void WBQtMapIniBlockCard::mousePressEvent(QMouseEvent *event)
{
	// A click anywhere on the header row toggles, not only on the arrow.
	if (event->button() == Qt::LeftButton && (!m_code->isVisible() || event->pos().y() < m_code->y()))
	{
		onToggle();
		return;
	}
	QFrame::mousePressEvent(event);
}

void WBQtMapIniBlockCard::onToggle()
{
	setExpanded(!m_code->isVisible());
}

void WBQtMapIniBlockCard::onCopy()
{
	QApplication::clipboard()->setText(m_data.source);
}

void WBQtMapIniBlockCard::onPopOut()
{
	QDialog *window = new QDialog(this, Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
		| Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	window->setAttribute(Qt::WA_DeleteOnClose);
	window->setWindowTitle(QString("%1 -- map.ini line %2").arg(blockTitle(m_data)).arg(m_data.firstLine));

	QVBoxLayout *layout = new QVBoxLayout(window);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->addWidget(makeCodeView(m_data, window));

	window->resize(900, 640);
	window->show();
}

// ===================== the report dialog =====================

WBQtMapIniReportDialog::WBQtMapIniReportDialog(const QString &title, const QString &text,
	bool applyMode, const QStringList &summary, const QList<WBQtMapIniBlockData> &blocks,
	QWidget *parent)
	: QDialog(parent),
	  m_ui(new Ui::WBQtMapIniReportDialog),
	  m_content(NULL),
	  m_messageInsertAt(0),
	  m_rawText(text)
{
	// The static widget tree lives in WBQtMapIniReportDialog.ui; bind the members the
	// logic below uses, then wire what Designer can't express.
	m_ui->setupUi(this);
	setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowMaximizeButtonHint);
	setWindowTitle(title);

	m_content = m_ui->contentLayout;
	buildSummary(summary);
	m_messageInsertAt = m_content->count();
	buildBlocks(blocks);
	m_content->addStretch(1);

	// Buttons. Apply mode (open/Reload) offers OK (load) / Cancel (don't); informational
	// mode (Check) just closes. Copy is always available. The .ui holds Copy + the
	// stretch; the mode-dependent buttons are appended here.
	if (applyMode)
	{
		QPushButton *okButton = new QPushButton("OK (load map.ini)", this);
		okButton->setDefault(true);
		m_ui->buttonsLayout->addWidget(okButton);
		QPushButton *cancelButton = new QPushButton("Cancel", this);
		cancelButton->setAutoDefault(false);
		m_ui->buttonsLayout->addWidget(cancelButton);
		connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
		connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
	}
	else
	{
		QPushButton *closeBtn = new QPushButton("Close", this);
		closeBtn->setDefault(true);
		m_ui->buttonsLayout->addWidget(closeBtn);
		connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
	}

	connect(m_ui->filter, SIGNAL(textChanged(QString)), this, SLOT(onFilterChanged(QString)));
	connect(m_ui->expandBtn, SIGNAL(clicked()), this, SLOT(onExpandAll()));
	connect(m_ui->collapseBtn, SIGNAL(clicked()), this, SLOT(onCollapseAll()));
	connect(m_ui->copyBtn, SIGNAL(clicked()), this, SLOT(onCopy()));

	resize(900, 700);
}

WBQtMapIniReportDialog::~WBQtMapIniReportDialog()
{
	delete m_ui;
}

void WBQtMapIniReportDialog::buildSummary(const QStringList &summary)
{
	for (int i = 0; i < summary.size(); ++i)
	{
		QLabel *label = new QLabel(summary.at(i), m_ui->content);
		label->setWordWrap(true);
		label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		if (i == 0)
		{
			QFont f = label->font();
			f.setBold(true);
			f.setPointSizeF(f.pointSizeF() + 2.0);
			label->setFont(f);
		}
		m_content->addWidget(label);
	}
}

void WBQtMapIniReportDialog::addMessage(int kind, const QString &title, const QString &body)
{
	QColor accent(80, 140, 210);
	if (kind == WBQT_MAPINI_MSG_ERROR)
	{
		accent = QColor(200, 70, 70);
	}
	else if (kind == WBQT_MAPINI_MSG_WARNING)
	{
		accent = QColor(215, 160, 60);
	}

	// The object name scopes the style to the panel, so its labels keep their own look.
	QFrame *panel = new QFrame(m_ui->content);
	panel->setObjectName("mapIniMessage");
	panel->setStyleSheet(QString("QFrame#mapIniMessage { border-left: 4px solid %1; background-color: rgba(%2, %3, %4, 40); }")
		.arg(accent.name()).arg(accent.red()).arg(accent.green()).arg(accent.blue()));

	QVBoxLayout *layout = new QVBoxLayout(panel);
	layout->setContentsMargins(10, 6, 8, 6);
	layout->setSpacing(2);

	QLabel *heading = new QLabel(title, panel);
	QFont f = heading->font();
	f.setBold(true);
	heading->setFont(f);
	layout->addWidget(heading);

	if (!body.isEmpty())
	{
		QLabel *text = new QLabel(body, panel);
		text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		text->setTextInteractionFlags(Qt::TextSelectableByMouse);
		layout->addWidget(text);
	}

	m_content->insertWidget(m_messageInsertAt, panel);
	++m_messageInsertAt;
}

void WBQtMapIniReportDialog::buildBlocks(const QList<WBQtMapIniBlockData> &blocks)
{
	// Objects first, then the other stores in the order the file first uses them.
	QStringList order;
	for (int i = 0; i < blocks.size(); ++i)
	{
		if (blocks.at(i).store == "Object")
		{
			order.append("Object");
			break;
		}
	}
	for (int i = 0; i < blocks.size(); ++i)
	{
		if (!order.contains(blocks.at(i).store))
		{
			order.append(blocks.at(i).store);
		}
	}

	for (int s = 0; s < order.size(); ++s)
	{
		Group group;
		int count = 0;
		for (int i = 0; i < blocks.size(); ++i)
		{
			if (blocks.at(i).store == order.at(s))
			{
				++count;
			}
		}

		group.header = new QLabel(QString("%1  (%2)").arg(order.at(s)).arg(count), m_ui->content);
		QFont f = group.header->font();
		f.setBold(true);
		f.setPointSizeF(f.pointSizeF() + 1.0);
		group.header->setFont(f);
		group.header->setContentsMargins(0, 8, 0, 0);
		m_content->addWidget(group.header);

		for (int i = 0; i < blocks.size(); ++i)
		{
			if (blocks.at(i).store != order.at(s))
			{
				continue;
			}
			WBQtMapIniBlockCard *card = new WBQtMapIniBlockCard(blocks.at(i), m_ui->content);
			m_content->addWidget(card);
			group.cards.append(card);
		}
		m_groups.append(group);
	}
}

void WBQtMapIniReportDialog::onFilterChanged(const QString &text)
{
	const QString needle = text.trimmed();
	for (int g = 0; g < m_groups.size(); ++g)
	{
		int shown = 0;
		for (int c = 0; c < m_groups.at(g).cards.size(); ++c)
		{
			WBQtMapIniBlockCard *card = m_groups.at(g).cards.at(c);
			const bool match = card->matches(needle);
			card->setVisible(match);
			if (!match)
			{
				continue;
			}
			++shown;
			// A hit inside the source opens the block, so the match is on screen.
			if (!card->titleMatches(needle))
			{
				card->setExpanded(true);
			}
		}
		m_groups.at(g).header->setVisible(shown > 0);
	}
}

void WBQtMapIniReportDialog::onExpandAll()
{
	for (int g = 0; g < m_groups.size(); ++g)
	{
		for (int c = 0; c < m_groups.at(g).cards.size(); ++c)
		{
			m_groups.at(g).cards.at(c)->setExpanded(true);
		}
	}
}

void WBQtMapIniReportDialog::onCollapseAll()
{
	for (int g = 0; g < m_groups.size(); ++g)
	{
		for (int c = 0; c < m_groups.at(g).cards.size(); ++c)
		{
			m_groups.at(g).cards.at(c)->setExpanded(false);
		}
	}
}

void WBQtMapIniReportDialog::onCopy()
{
	QApplication::clipboard()->setText(m_rawText);
}

// ===================== the C facade =====================

extern "C" void WBQtMapIniReport_Clear(void)
{
	s_summary.clear();
	s_messages.clear();
	s_blocks.clear();
}

extern "C" void WBQtMapIniReport_AddSummary(const char *line)
{
	s_summary.append(QString::fromLocal8Bit(line ? line : ""));
}

extern "C" void WBQtMapIniReport_AddMessage(int kind, const char *title, const char *body)
{
	StagedMessage message;
	message.kind = kind;
	message.title = QString::fromLocal8Bit(title ? title : "");
	message.body = QString::fromLocal8Bit(body ? body : "");
	s_messages.append(message);
}

extern "C" void WBQtMapIniReport_AddBlock(const char *store, const char *name, int status, int firstLine,
	const char *source, const int *blankedLines, int blankedCount)
{
	WBQtMapIniBlockData data;
	data.store = QString::fromLocal8Bit(store ? store : "");
	data.name = QString::fromLocal8Bit(name ? name : "");
	data.status = status;
	data.firstLine = firstLine;
	data.source = QString::fromLocal8Bit(source ? source : "");
	for (int i = 0; i < blankedCount; ++i)
	{
		data.blanked.append(blankedLines[i]);
	}
	s_blocks.append(data);
}

extern "C" int WBQtMapIniReport_Show(const char *title, const char *text, int applyMode)
{
	if (qApp == NULL)
	{
		WBQtMapIniReport_Clear();
		return 0;	// Qt not up yet -- the caller falls back to the MFC dialog
	}
	WBQtMapIniReportDialog dlg(
		QString::fromLocal8Bit(title ? title : "Map.ini"),
		QString::fromLocal8Bit(text ? text : ""),
		applyMode != 0,
		s_summary,
		s_blocks,
		WBQt_DialogParent());
	for (int i = 0; i < s_messages.size(); ++i)
	{
		dlg.addMessage(s_messages.at(i).kind, s_messages.at(i).title, s_messages.at(i).body);
	}
	WBQtMapIniReport_Clear();

	dlg.setWindowModality(Qt::ApplicationModal);
	int rc = dlg.exec();
	return (rc == QDialog::Accepted) ? 2 : 1;	// 2 = OK/accepted, 1 = Cancel/closed
}

#else	// !RTS_HAS_QT

extern "C" void WBQtMapIniReport_Clear(void)
{
}

extern "C" void WBQtMapIniReport_AddSummary(const char * /*line*/)
{
}

extern "C" void WBQtMapIniReport_AddMessage(int /*kind*/, const char * /*title*/, const char * /*body*/)
{
}

extern "C" void WBQtMapIniReport_AddBlock(const char * /*store*/, const char * /*name*/, int /*status*/,
	int /*firstLine*/, const char * /*source*/, const int * /*blankedLines*/, int /*blankedCount*/)
{
}

extern "C" int WBQtMapIniReport_Show(const char * /*title*/, const char * /*text*/, int /*applyMode*/)
{
	return 0;	// no Qt -- caller uses the MFC dialog
}

#endif // RTS_HAS_QT
