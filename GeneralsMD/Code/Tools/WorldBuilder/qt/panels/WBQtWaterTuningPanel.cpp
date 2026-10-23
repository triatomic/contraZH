// WBQtWaterTuningPanel.cpp -- see WBQtWaterTuningPanel.h.
#include "WBQtWaterTuningPanel.h"
#include "ui_WBQtWaterTuningPanel.h"
#include "WBQtWaterTuningBridge.h"
#include "WBQtMapIniEditorBridge.h"
#include "WBQtScrubSpinBox.h"
#include "WBQtWindowPos.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

WBQtWaterTuningPanel *WBQtWaterTuningPanel::s_instance = NULL;

// Defined in WBQtBridge.cpp: the main window when inverted, else an invisible
// QWinWidget bridge rooted in the MFC frame. Never hide() the result.
QWidget *WBQt_CreateOwnerBridgeWidget(void *frameHwnd);

namespace
{
	QWidget *s_owner = NULL;	// owner for the floating panel (created on first open)

	const int kSaveDelayMs = 400;
	const int kSaveRetryMs = 1500;

	// What a ticked ShaderWaterDeepColor starts from when Water.ini leaves it unset.
	const float kDeepColorStart[3] = { 40.0f, 85.0f, 95.0f };

	const char *const kChannelNames[3] = { "red", "green", "blue" };

	enum
	{
		COL_LABEL = 0,
		COL_MINUS,
		COL_SPIN,
		COL_PLUS,
		COL_RESET
	};

	int decimalsOf(double value)
	{
		const QString text = QString::number(value, 'g', 6);
		if (text.contains('e'))
		{
			return 6;
		}
		const int dot = text.indexOf('.');
		return (dot < 0) ? 0 : text.size() - dot - 1;
	}

	//----------------------------------------------------------------------------------------
	// A WaterTransparency or GameData block of a map.ini, as text.
	//----------------------------------------------------------------------------------------

	struct Block
	{
		bool found;
		int bodyStart;	// first character after the header line
		int bodyEnd;	// first character of the End line
	};

	Block findBlock(const QString &text, const QString &name)
	{
		const QRegularExpression header("^[ \\t]*" + QRegularExpression::escape(name) + "\\b[^\\n]*\\n",
			QRegularExpression::MultilineOption);
		static const QRegularExpression word("^[ \\t]*(\\w+)[ \\t]*(?:;[^\\n]*)?\\r?$",
			QRegularExpression::MultilineOption);

		Block block = { false, 0, 0 };
		const QRegularExpressionMatch head = header.match(text);
		if (!head.hasMatch())
		{
			return block;
		}

		// A line of one word other than End opens a nested block, which has an End of its own.
		int depth = 0;
		QRegularExpressionMatchIterator lines = word.globalMatch(text, head.capturedEnd());
		while (lines.hasNext())
		{
			const QRegularExpressionMatch line = lines.next();
			if (line.captured(1) != "End")
			{
				++depth;
				continue;
			}
			if (depth == 0)
			{
				block.found = true;
				block.bodyStart = head.capturedEnd();
				block.bodyEnd = line.capturedStart();
				return block;
			}
			--depth;
		}
		return block;
	}

	// Captures: 1 the "Key = " lead, 2 the value, 3 the blanks before a comment or the line end.
	QRegularExpression keyPattern(const QString &key)
	{
		return QRegularExpression("^([ \\t]*" + QRegularExpression::escape(key)
			+ "[ \\t]*=[ \\t]*)([^;\\r\\n]*?)([ \\t]*)(?=;|\\r?$)",
			QRegularExpression::MultilineOption);
	}

	// The key's value text, or a null string when the block does not set it.
	QString readValue(const QString &body, const QString &key)
	{
		const QRegularExpressionMatch m = keyPattern(key).match(body);
		if (!m.hasMatch())
		{
			return QString();
		}
		const QString value = m.captured(2);
		return value.isNull() ? QString("") : value;
	}

	void writeValue(QString &body, const QString &key, const QString &value, const QString &nl)
	{
		const QRegularExpressionMatch m = keyPattern(key).match(body);
		if (m.hasMatch())
		{
			// Padding keeps a trailing comment in its column.
			QString pad;
			const int after = m.capturedEnd(3);
			if (after < body.size() && body.at(after) == ';')
			{
				const int width = m.capturedLength(2) + m.capturedLength(3) - value.size();
				pad = QString(qMax(1, width), ' ');
			}
			body.replace(m.capturedStart(2), after - m.capturedStart(2), value + pad);
			return;
		}

		// A new key takes the indent of the block's last line.
		static const QRegularExpression indented("^([ \\t]+)\\S[^\\n]*\\n?\\z",
			QRegularExpression::MultilineOption);
		const QRegularExpressionMatch last = indented.match(body);
		const QString indent = last.hasMatch() ? last.captured(1) : QString("  ");
		if (!body.isEmpty() && !body.endsWith('\n'))
		{
			body += nl;
		}
		body += indent + key + " = " + value + nl;
	}

	void removeValue(QString &body, const QString &key)
	{
		const QRegularExpression line("^[ \\t]*" + QRegularExpression::escape(key)
			+ "[ \\t]*=[^\\n]*\\n?", QRegularExpression::MultilineOption);
		const QRegularExpressionMatch m = line.match(body);
		if (m.hasMatch())
		{
			body.remove(m.capturedStart(), m.capturedLength());
		}
	}

	// Writes one block's pending keys into the file text. False when the block is absent and no key needs it.
	bool editBlock(QString &text, const QString &name, const QMap<QString, QString> &pending, const QString &nl)
	{
		bool anyWrite = false;
		QMap<QString, QString>::const_iterator it;
		for (it = pending.constBegin(); it != pending.constEnd(); ++it)
		{
			if (!it.value().isNull())
			{
				anyWrite = true;
			}
		}

		Block block = findBlock(text, name);
		if (!block.found)
		{
			if (!anyWrite)
			{
				return false;
			}
			if (!text.isEmpty())
			{
				text += text.endsWith('\n') ? nl : nl + nl;
			}
			text += name + nl;
			block.bodyStart = text.size();
			block.bodyEnd = text.size();
			text += "End" + nl;
		}

		QString body = text.mid(block.bodyStart, block.bodyEnd - block.bodyStart);
		for (it = pending.constBegin(); it != pending.constEnd(); ++it)
		{
			if (it.value().isNull())
			{
				removeValue(body, it.key());
			}
			else
			{
				writeValue(body, it.key(), it.value(), nl);
			}
		}
		text.replace(block.bodyStart, block.bodyEnd - block.bodyStart, body);
		return true;
	}

	bool parseValue(int kind, const QString &raw, float value[3])
	{
		const QString text = raw.trimmed();
		switch (kind)
		{
			case WBQT_WATER_FLOAT:
			{
				bool ok = false;
				const float parsed = text.section(QRegularExpression("\\s"), 0, 0).toFloat(&ok);
				if (ok)
				{
					value[0] = parsed;
				}
				return ok;
			}
			case WBQT_WATER_BOOL:
			{
				if (text.compare("yes", Qt::CaseInsensitive) == 0)
				{
					value[0] = 1.0f;
					return true;
				}
				if (text.compare("no", Qt::CaseInsensitive) == 0)
				{
					value[0] = 0.0f;
					return true;
				}
				return false;
			}
			case WBQT_WATER_COLOR:
			{
				static const QRegularExpression rgb("R:\\s*(\\d+)\\s+G:\\s*(\\d+)\\s+B:\\s*(\\d+)");
				const QRegularExpressionMatch m = rgb.match(text);
				if (!m.hasMatch())
				{
					return false;
				}
				for (int c = 0; c < 3; ++c)
				{
					value[c] = m.captured(c + 1).toFloat();
				}
				return true;
			}
		}
		return false;
	}

	// A text value is its first word, which is all a file name or None is.
	QString firstWord(const QString &raw)
	{
		return raw.trimmed().section(QRegularExpression("\\s"), 0, 0);
	}

	QStringList tableNames()
	{
		char buf[4096];
		WBQtWaterTuning_ListTables(buf, sizeof(buf));
		return QString::fromLocal8Bit(buf).split('\n', QString::SkipEmptyParts);
	}

	QToolButton *makeButton(QWidget *parent, const QString &text, int row)
	{
		QToolButton *button = new QToolButton(parent);
		button->setText(text);
		button->setFixedWidth(24);
		button->setFocusPolicy(Qt::NoFocus);
		button->setProperty("row", row);
		return button;
	}
}

WBQtWaterTuningPanel::Row::Row()
	: index(0),
	  kind(WBQT_WATER_FLOAT),
	  lo(0.0f),
	  hi(1.0f),
	  step(1.0f),
	  advanced(false),
	  inFile(false),
	  label(NULL),
	  check(NULL),
	  combo(NULL),
	  reset(NULL)
{
	for (int c = 0; c < 3; ++c)
	{
		value[c] = 0.0f;
		spin[c] = NULL;
		minus[c] = NULL;
		plus[c] = NULL;
	}
}

WBQtWaterTuningPanel::WBQtWaterTuningPanel(QWidget *owner)
	: QWidget(owner, Qt::Tool),
	  m_ui(new Ui::WBQtWaterTuningPanel),
	  m_saveTimer(new QTimer(this)),
	  m_updating(false)
{
	// The shell and its two tabs live in WBQtWaterTuningPanel.ui; the rows come from the bridge's key table.
	m_ui->setupUi(this);
	WBQtWindowPos_Track(this, "WaterTuning");

	buildRows();
	onAdvancedToggled(m_ui->advancedCheck->isChecked());

	m_saveTimer->setSingleShot(true);
	connect(m_saveTimer, SIGNAL(timeout()), this, SLOT(onSaveTimer()));
	connect(m_ui->advancedCheck, SIGNAL(toggled(bool)), this, SLOT(onAdvancedToggled(bool)));

	s_instance = this;
}

WBQtWaterTuningPanel::~WBQtWaterTuningPanel()
{
	if (s_instance == this)
	{
		s_instance = NULL;
	}
	delete m_ui;
}

void WBQtWaterTuningPanel::addStepper(Row &row, int rowIndex, int channel, QGridLayout *grid, int gridRow, const QString &tip)
{
	QWidget *host = grid->parentWidget();

	QToolButton *minus = makeButton(host, QString(QChar(0x2212)), rowIndex);
	QToolButton *plus = makeButton(host, "+", rowIndex);
	minus->setProperty("chan", channel);
	minus->setProperty("dir", -1);
	plus->setProperty("chan", channel);
	plus->setProperty("dir", 1);
	minus->setAutoRepeat(true);
	plus->setAutoRepeat(true);

	WBQtScrubSpinBox *spin = new WBQtScrubSpinBox(host);
	spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
	spin->setKeyboardTracking(false);
	spin->setAlignment(Qt::AlignRight);
	spin->setMinimumWidth(72);
	spin->setToolTip(tip);
	spin->setProperty("row", rowIndex);
	spin->setProperty("chan", channel);

	grid->addWidget(minus, gridRow, COL_MINUS);
	grid->addWidget(spin, gridRow, COL_SPIN);
	grid->addWidget(plus, gridRow, COL_PLUS);

	connect(minus, SIGNAL(clicked()), this, SLOT(onStepClicked()));
	connect(plus, SIGNAL(clicked()), this, SLOT(onStepClicked()));
	connect(spin, SIGNAL(valueChanged(double)), this, SLOT(onSpinChanged(double)));

	row.minus[channel] = minus;
	row.plus[channel] = plus;
	row.spin[channel] = spin;
	row.widgets << minus << spin << plus;
}

// A scrolling grid of rows on its own tab, like the Water tab in WBQtWaterTuningPanel.ui.
QGridLayout *WBQtWaterTuningPanel::addTab(const QString &title)
{
	QWidget *page = new QWidget(m_ui->tabs);
	QVBoxLayout *layout = new QVBoxLayout(page);
	QScrollArea *scroll = new QScrollArea(page);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidgetResizable(true);
	QWidget *host = new QWidget(scroll);
	QGridLayout *grid = new QGridLayout(host);
	grid->setColumnStretch(COL_LABEL, 1);
	scroll->setWidget(host);
	layout->addWidget(scroll);
	m_ui->tabs->addTab(page, title);
	return grid;
}

void WBQtWaterTuningPanel::buildRows()
{
	// The Water tab's grid, then one tab per GameData group as the groups appear.
	QVector<QGridLayout *> grids;
	QVector<int> gridRows;
	grids << m_ui->rowsGrid;
	gridRows << 0;
	QString group;

	const int count = WBQtWaterTuning_Count();
	m_rows.reserve(count);
	for (int i = 0; i < count; ++i)
	{
		WBQtWaterTuningDesc desc;
		if (!WBQtWaterTuning_GetDesc(i, &desc))
		{
			continue;
		}
		const int r = m_rows.size();
		const QString tip = QString::fromLatin1(desc.help);

		Row row;
		row.index = i;
		row.kind = desc.kind;
		row.key = QString::fromLatin1(desc.key);
		row.block = QString::fromLatin1(desc.block);
		row.lo = desc.lo;
		row.hi = desc.hi;
		row.step = desc.step;
		row.advanced = desc.advanced != 0;

		const bool water = (row.block == "WaterTransparency");
		if (!water && desc.group != NULL && group != desc.group)
		{
			group = QString::fromLatin1(desc.group);
			grids << addTab(group);
			gridRows << 0;
		}
		QGridLayout *grid = water ? grids[0] : grids.last();
		QWidget *host = grid->parentWidget();
		int &gridRow = water ? gridRows[0] : gridRows.last();

		row.reset = makeButton(host, QString(QChar(0x21BA)), r);
		row.reset->setToolTip(tr("Remove the key from map.ini, so the map follows %1")
			.arg(water ? "Water.ini" : "GameData.ini"));
		connect(row.reset, SIGNAL(clicked()), this, SLOT(onResetClicked()));
		grid->addWidget(row.reset, gridRow, COL_RESET);
		row.widgets << row.reset;

		if (desc.kind == WBQT_WATER_FLOAT)
		{
			row.label = new QLabel(row.key, host);
			row.label->setToolTip(tip);
			grid->addWidget(row.label, gridRow, COL_LABEL);
			row.widgets << row.label;

			addStepper(row, r, 0, grid, gridRow, tip);
			// Wider than lo to hi, so a typed or loaded value is never clamped.
			row.spin[0]->setRange((desc.lo < 0.0f) ? desc.lo * 10.0 - 10.0 : 0.0, desc.hi * 10.0 + 10.0);
			row.spin[0]->setSingleStep(desc.step);
			row.spin[0]->setDecimals(decimalsOf(desc.step));
			++gridRow;
		}
		else if (desc.kind == WBQT_WATER_TEXT)
		{
			row.label = new QLabel(row.key, host);
			row.label->setToolTip(tip);
			grid->addWidget(row.label, gridRow, COL_LABEL);
			row.widgets << row.label;

			// Any name can be typed, since the list only has the files found on disk.
			row.combo = new QComboBox(host);
			row.combo->setEditable(true);
			row.combo->setInsertPolicy(QComboBox::NoInsert);
			row.combo->setToolTip(tip);
			row.combo->setProperty("row", r);
			row.combo->lineEdit()->setProperty("row", r);
			row.combo->addItem("None");
			row.combo->addItems(tableNames());
			grid->addWidget(row.combo, gridRow, COL_MINUS, 1, COL_PLUS - COL_MINUS + 1);
			connect(row.combo, SIGNAL(activated(int)), this, SLOT(onTextChosen()));
			connect(row.combo->lineEdit(), SIGNAL(editingFinished()), this, SLOT(onTextChosen()));
			row.widgets << row.combo;
			++gridRow;
		}
		else
		{
			row.check = new QCheckBox(row.key, host);
			row.check->setToolTip(tip);
			row.check->setProperty("row", r);
			connect(row.check, SIGNAL(toggled(bool)), this, SLOT(onCheckToggled(bool)));
			grid->addWidget(row.check, gridRow, COL_LABEL, 1, COL_RESET - COL_LABEL);
			row.widgets << row.check;
			++gridRow;

			if (desc.kind == WBQT_WATER_COLOR)
			{
				for (int c = 0; c < 3; ++c)
				{
					QLabel *channel = new QLabel(QString("    ") + kChannelNames[c], host);
					channel->setToolTip(tip);
					grid->addWidget(channel, gridRow, COL_LABEL);
					row.widgets << channel;

					addStepper(row, r, c, grid, gridRow, tip);
					row.spin[c]->setRange(0.0, 255.0);
					row.spin[c]->setSingleStep(1.0);
					row.spin[c]->setDecimals(0);
					++gridRow;
				}
			}
		}
		m_rows.append(row);
	}
	for (int g = 0; g < grids.size(); ++g)
	{
		grids[g]->setRowStretch(gridRows[g], 1);
	}
}

QString WBQtWaterTuningPanel::formatValue(const Row &row) const
{
	switch (row.kind)
	{
		case WBQT_WATER_BOOL:
			return (row.value[0] >= 0.5f) ? "Yes" : "No";
		case WBQT_WATER_COLOR:
			return QString("R:%1 G:%2 B:%3")
				.arg(qBound(0, qRound(row.value[0]), 255))
				.arg(qBound(0, qRound(row.value[1]), 255))
				.arg(qBound(0, qRound(row.value[2]), 255));
		case WBQT_WATER_TEXT:
			return row.text.isEmpty() ? QString("None") : row.text;
		default:
			return QString::number(row.value[0], 'g', 6);
	}
}

void WBQtWaterTuningPanel::setStatus(const QString &text)
{
	m_ui->statusLabel->setText(text);
}

// Put a row's value and its "set in map.ini" state on its widgets.
void WBQtWaterTuningPanel::showRow(int r)
{
	const bool wasUpdating = m_updating;
	m_updating = true;

	Row &row = m_rows[r];
	QWidget *title = (row.label != NULL) ? static_cast<QWidget *>(row.label) : row.check;
	QFont font = title->font();
	font.setBold(row.inFile);
	title->setFont(font);
	row.reset->setEnabled(row.inFile);

	switch (row.kind)
	{
		case WBQT_WATER_FLOAT:
		{
			// A loaded value keeps the decimals it was written with.
			row.spin[0]->setDecimals(qMax(decimalsOf(row.step), decimalsOf(row.value[0])));
			row.spin[0]->setValue(row.value[0]);
			break;
		}
		case WBQT_WATER_BOOL:
		{
			row.check->setChecked(row.value[0] >= 0.5f);
			break;
		}
		case WBQT_WATER_COLOR:
		{
			row.check->setChecked(row.inFile);
			for (int c = 0; c < 3; ++c)
			{
				row.spin[c]->setValue((row.value[c] < 0.0f) ? kDeepColorStart[c] : row.value[c]);
				row.spin[c]->setEnabled(row.inFile);
				row.minus[c]->setEnabled(row.inFile);
				row.plus[c]->setEnabled(row.inFile);
			}
			break;
		}
		case WBQT_WATER_TEXT:
		{
			row.combo->setEditText(formatValue(row));
			break;
		}
	}

	m_updating = wasUpdating;
}

void WBQtWaterTuningPanel::loadBase(Row &row)
{
	if (row.kind == WBQT_WATER_TEXT)
	{
		char buf[256];
		WBQtWaterTuning_GetBaseText(row.index, buf, sizeof(buf));
		row.text = QString::fromLocal8Bit(buf);
		return;
	}
	WBQtWaterTuning_GetBase(row.index, row.value);
}

void WBQtWaterTuningPanel::pushLive(const Row &row)
{
	if (row.kind == WBQT_WATER_TEXT)
	{
		WBQtWaterTuning_SetLiveText(row.index, formatValue(row).toLocal8Bit().constData());
		return;
	}
	WBQtWaterTuning_SetLive(row.index, row.value);
}

// The user changed a row: show it in the 3D view and queue it for map.ini.
void WBQtWaterTuningPanel::markChanged(int r)
{
	Row &row = m_rows[r];
	row.inFile = true;
	showRow(r);
	pushLive(row);
	m_pending[row.key] = formatValue(row);
	m_saveTimer->start(kSaveDelayMs);
}

// Take the key out of map.ini and return the row to the Water.ini value.
void WBQtWaterTuningPanel::resetRow(int r)
{
	Row &row = m_rows[r];
	loadBase(row);
	row.inFile = false;
	showRow(r);
	pushLive(row);
	m_pending[row.key] = QString();
	m_saveTimer->start(kSaveDelayMs);
}

void WBQtWaterTuningPanel::openOn(const QString &iniPath)
{
	flush();
	if (iniPath != m_path)
	{
		m_pending.clear();
	}
	m_path = iniPath;
	reseed();
	setStatus(QDir::toNativeSeparators(m_path));
}

void WBQtWaterTuningPanel::reseed()
{
	QString text;
	QFile file(m_path);
	if (!m_path.isEmpty() && file.open(QIODevice::ReadOnly))
	{
		text = QString::fromLatin1(file.readAll());
		file.close();
	}

	QMap<QString, QString> bodies;
	for (int r = 0; r < m_rows.size(); ++r)
	{
		const QString &name = m_rows[r].block;
		if (!bodies.contains(name))
		{
			const Block block = findBlock(text, name);
			bodies[name] = block.found ? text.mid(block.bodyStart, block.bodyEnd - block.bodyStart) : QString();
		}
	}

	// Edits a failed save left pending win over the file, and stay queued.
	for (int r = 0; r < m_rows.size(); ++r)
	{
		Row &row = m_rows[r];
		loadBase(row);
		const QString raw = m_pending.contains(row.key) ? m_pending.value(row.key) : readValue(bodies.value(row.block), row.key);
		if (row.kind == WBQT_WATER_TEXT)
		{
			row.inFile = !raw.isNull() && !firstWord(raw).isEmpty();
			if (row.inFile)
			{
				row.text = firstWord(raw);
			}
		}
		else
		{
			row.inFile = !raw.isNull() && parseValue(row.kind, raw, row.value);
		}
		showRow(r);
		pushLive(row);
	}
	if (!m_pending.isEmpty())
	{
		m_saveTimer->start(kSaveRetryMs);
	}
}

bool WBQtWaterTuningPanel::flush()
{
	m_saveTimer->stop();
	if (m_pending.isEmpty() || m_path.isEmpty())
	{
		m_pending.clear();
		return true;
	}

	const QByteArray localPath = m_path.toLocal8Bit();
	if (WBQtMapIniEditor_HasUnsavedChanges(localPath.constData()))
	{
		setStatus(tr("Not saved: map.ini has unsaved edits in the map.ini editor."));
		return false;
	}

	QString text;
	QFile file(m_path);
	if (file.exists())
	{
		if (!file.open(QIODevice::ReadOnly))
		{
			setStatus(tr("Not saved: could not read %1.").arg(QDir::toNativeSeparators(m_path)));
			return false;
		}
		text = QString::fromLatin1(file.readAll());
		file.close();
	}
	const QString nl = (text.isEmpty() || text.contains("\r\n")) ? "\r\n" : "\n";

	QMap<QString, QMap<QString, QString> > byBlock;
	for (int r = 0; r < m_rows.size(); ++r)
	{
		const Row &row = m_rows[r];
		if (m_pending.contains(row.key))
		{
			byBlock[row.block][row.key] = m_pending.value(row.key);
		}
	}

	bool edited = false;
	QMap<QString, QMap<QString, QString> >::const_iterator it;
	for (it = byBlock.constBegin(); it != byBlock.constEnd(); ++it)
	{
		if (editBlock(text, it.key(), it.value(), nl))
		{
			edited = true;
		}
	}
	if (!edited)
	{
		m_pending.clear();
		return true;
	}

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		setStatus(tr("Not saved: could not write %1.").arg(QDir::toNativeSeparators(m_path)));
		return false;
	}
	file.write(text.toLatin1());
	file.close();

	m_pending.clear();
	WBQtWaterTuning_NoteSaved();
	WBQtMapIniEditor_ReloadIfOpen(localPath.constData());
	setStatus(tr("Saved to %1.").arg(QDir::toNativeSeparators(m_path)));
	return true;
}

void WBQtWaterTuningPanel::mapChanged()
{
	flush();
	m_pending.clear();
	m_path.clear();
	hide();
}

void WBQtWaterTuningPanel::hideEvent(QHideEvent *event)
{
	flush();
	QWidget::hideEvent(event);
}

void WBQtWaterTuningPanel::onSpinChanged(double value)
{
	if (m_updating)
	{
		return;
	}
	const int r = sender()->property("row").toInt();
	const int c = sender()->property("chan").toInt();
	m_rows[r].value[c] = (float)value;
	markChanged(r);
}

void WBQtWaterTuningPanel::onStepClicked()
{
	const int r = sender()->property("row").toInt();
	const int c = sender()->property("chan").toInt();
	const int dir = sender()->property("dir").toInt();
	const Row &row = m_rows[r];
	WBQtScrubSpinBox *spin = row.spin[c];

	// A value already past lo or hi may step back in, but not further out.
	const double now = spin->value();
	double next = now + dir * spin->singleStep();
	if (dir > 0 && next > row.hi)
	{
		next = qMax(now, (double)row.hi);
	}
	if (dir < 0 && next < row.lo)
	{
		next = qMin(now, (double)row.lo);
	}
	spin->setValue(next);
}

void WBQtWaterTuningPanel::onCheckToggled(bool on)
{
	if (m_updating)
	{
		return;
	}
	const int r = sender()->property("row").toInt();
	Row &row = m_rows[r];
	if (row.kind == WBQT_WATER_BOOL)
	{
		row.value[0] = on ? 1.0f : 0.0f;
		markChanged(r);
		return;
	}

	// The colour's box says whether map.ini sets the key at all.
	if (!on)
	{
		resetRow(r);
		return;
	}
	for (int c = 0; c < 3; ++c)
	{
		row.value[c] = (float)row.spin[c]->value();
	}
	markChanged(r);
}

void WBQtWaterTuningPanel::onTextChosen()
{
	if (m_updating)
	{
		return;
	}
	const int r = sender()->property("row").toInt();
	Row &row = m_rows[r];
	const QString text = firstWord(row.combo->currentText());
	if (text.isEmpty() || (row.inFile && text == row.text))
	{
		return;
	}
	row.text = text;
	markChanged(r);
}

void WBQtWaterTuningPanel::onResetClicked()
{
	resetRow(sender()->property("row").toInt());
}

void WBQtWaterTuningPanel::onAdvancedToggled(bool on)
{
	for (int r = 0; r < m_rows.size(); ++r)
	{
		const Row &row = m_rows[r];
		if (!row.advanced)
		{
			continue;
		}
		for (int w = 0; w < row.widgets.size(); ++w)
		{
			row.widgets[w]->setVisible(on);
		}
	}
}

void WBQtWaterTuningPanel::onSaveTimer()
{
	if (!flush() && isVisible())
	{
		m_saveTimer->start(kSaveRetryMs);
	}
}

// --- Open / map change / refresh hooks (the Qt side of WBQtWaterTuningBridge.h) ---------------

extern "C" void WBQtWaterTuning_Open(void *frameHwnd, const char *iniPath)
{
	if (frameHwnd == NULL || iniPath == NULL)
	{
		return;
	}
	if (s_owner == NULL)
	{
		s_owner = WBQt_CreateOwnerBridgeWidget(frameHwnd);
	}
	WBQtWaterTuningPanel *panel = WBQtWaterTuningPanel::instance();
	if (panel == NULL)
	{
		panel = new WBQtWaterTuningPanel(s_owner);
	}
	panel->openOn(QString::fromLocal8Bit(iniPath));
	// Show WITHOUT activating (== the MFC SW_SHOWNA) so the viewport keeps focus.
	panel->setAttribute(Qt::WA_ShowWithoutActivating);
	panel->show();
	panel->raise();
}

extern "C" void WBQtWaterTuning_MapChanged(void)
{
	WBQtWaterTuningPanel *panel = WBQtWaterTuningPanel::instance();
	if (panel != NULL)
	{
		panel->mapChanged();
	}
}

extern "C" void WBQtWaterTuning_PushRefresh(void)
{
	WBQtWaterTuningPanel *panel = WBQtWaterTuningPanel::instance();
	if (panel != NULL && panel->isVisible())
	{
		panel->flush();
		panel->reseed();
	}
}
