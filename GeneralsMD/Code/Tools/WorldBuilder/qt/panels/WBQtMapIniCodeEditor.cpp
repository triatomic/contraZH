// WBQtMapIniCodeEditor.cpp -- see WBQtMapIniCodeEditor.h.
#include "WBQtMapIniCodeEditor.h"
#include "WBQtMapIniEditorDialog.h"	// WBQtMapIniHighlighter
#include "WBQtMapIniReport.h"

#include <QEvent>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>

#include <algorithm>

namespace
{
	const int kFoldColumn = 14;

	bool headerLess(const QPair<int, int> &range, int headerLine)
	{
		return range.first < headerLine;
	}
}

QString WBQtMapIniStatusText(int status)
{
	switch (status)
	{
		case WBQT_MAPINI_BLOCK_OVERRIDDEN:	return "overridden";
		case WBQT_MAPINI_BLOCK_NEW:			return "new";
		case WBQT_MAPINI_BLOCK_DROPPED:		return "dropped -- not applied";
		case WBQT_MAPINI_BLOCK_IGNORED:		return "not loaded in WorldBuilder";
		default:							return "loaded";
	}
}

QColor WBQtMapIniStatusColour(int status)
{
	switch (status)
	{
		case WBQT_MAPINI_BLOCK_OVERRIDDEN:	return QColor(55, 105, 165);
		case WBQT_MAPINI_BLOCK_NEW:			return QColor(125, 85, 165);
		case WBQT_MAPINI_BLOCK_DROPPED:		return QColor(175, 55, 55);
		case WBQT_MAPINI_BLOCK_IGNORED:		return QColor(105, 105, 105);
		default:							return QColor(55, 130, 75);
	}
}

class WBQtMapIniCodeEditor::Gutter : public QWidget
{
public:
	explicit Gutter(WBQtMapIniCodeEditor *view) : QWidget(view), m_view(view) {}
	virtual QSize sizeHint() const { return QSize(m_view->gutterWidth(), 0); }

protected:
	virtual void paintEvent(QPaintEvent *event) { m_view->paintGutter(event); }
	virtual void mousePressEvent(QMouseEvent *event) { m_view->gutterPressed(event->pos()); }

private:
	WBQtMapIniCodeEditor *m_view;
};

WBQtMapIniCodeEditor::WBQtMapIniCodeEditor(QWidget *parent)
	: QPlainTextEdit(parent),
	  m_gutter(NULL),
	  m_firstLine(1),
	  m_folding(false),
	  m_rangesStale(false)
{
	m_gutter = new Gutter(this);
	updateGutterGeometry();

	connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(onBlockCountChanged(int)));
	connect(this, SIGNAL(updateRequest(QRect,int)), this, SLOT(onUpdateRequest(QRect,int)));
	connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(onCursorPositionChanged()));
}

WBQtMapIniCodeEditor *WBQtMapIniCodeEditor::createReadOnly(const QString &source, int firstLine,
	QWidget *parent)
{
	WBQtMapIniCodeEditor *view = new WBQtMapIniCodeEditor(parent);
	view->setReadOnly(true);
	view->setLineWrapMode(QPlainTextEdit::NoWrap);
	view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	view->setFirstLineNumber(firstLine);

	// Checks go off before the text arrives, so the first highlight pass skips them too.
	WBQtMapIniHighlighter *highlighter = new WBQtMapIniHighlighter(view->document());
	highlighter->setCheckNames(false);
	highlighter->setCheckSyntax(false);
	view->setPlainText(source);
	return view;
}

void WBQtMapIniCodeEditor::onBlockCountChanged(int count)
{
	Q_UNUSED(count);
	// Fold ranges are line numbers, so they are wrong until the owner sends new ones.
	m_rangesStale = true;
	updateGutterGeometry();
}

void WBQtMapIniCodeEditor::onUpdateRequest(const QRect &rect, int dy)
{
	if (dy != 0)
	{
		m_gutter->scroll(0, dy);
	}
	else
	{
		m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());
	}
}

void WBQtMapIniCodeEditor::onCursorPositionChanged()
{
	// Find, undo and jumps can put the cursor inside a folded block; open it.
	const QTextBlock block = textCursor().block();
	if (!block.isVisible())
	{
		revealLine(block.blockNumber());
	}
}

void WBQtMapIniCodeEditor::setFirstLineNumber(int number)
{
	m_firstLine = number;
	updateGutterGeometry();
	m_gutter->update();
}

void WBQtMapIniCodeEditor::setFoldingEnabled(bool enabled)
{
	m_folding = enabled;
	updateGutterGeometry();
	m_gutter->update();
}

int WBQtMapIniCodeEditor::numberWidth() const
{
	const int lastLine = m_firstLine + document()->blockCount() - 1;
	const int digits = QString::number(lastLine > 0 ? lastLine : 1).length();
	return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

int WBQtMapIniCodeEditor::gutterWidth() const
{
	return numberWidth() + (m_folding ? kFoldColumn : 0);
}

void WBQtMapIniCodeEditor::updateGutterGeometry()
{
	setViewportMargins(gutterWidth(), 0, 0, 0);
	const QRect area = contentsRect();
	m_gutter->setGeometry(QRect(area.left(), area.top(), gutterWidth(), area.height()));
}

void WBQtMapIniCodeEditor::changeEvent(QEvent *event)
{
	QPlainTextEdit::changeEvent(event);
	if (event->type() == QEvent::FontChange)
	{
		updateGutterGeometry();
	}
}

void WBQtMapIniCodeEditor::resizeEvent(QResizeEvent *event)
{
	QPlainTextEdit::resizeEvent(event);
	const QRect area = contentsRect();
	m_gutter->setGeometry(QRect(area.left(), area.top(), gutterWidth(), area.height()));
}

int WBQtMapIniCodeEditor::foldRangeAt(int headerLine) const
{
	if (m_rangesStale)
	{
		return -1;
	}
	// The ranges arrive in file order, so the headers are sorted.
	QList<QPair<int, int> >::const_iterator it =
		std::lower_bound(m_foldRanges.constBegin(), m_foldRanges.constEnd(), headerLine, headerLess);
	if (it == m_foldRanges.constEnd() || it->first != headerLine || it->second <= headerLine)
	{
		return -1;
	}
	return (int)(it - m_foldRanges.constBegin());
}

bool WBQtMapIniCodeEditor::isFolded(int headerLine) const
{
	if (foldRangeAt(headerLine) < 0)
	{
		return false;
	}
	const QTextBlock next = document()->findBlockByNumber(headerLine + 1);
	return next.isValid() && !next.isVisible();
}

void WBQtMapIniCodeEditor::setLinesVisible(int first, int last, bool visible)
{
	QTextBlock block = document()->findBlockByNumber(first);
	if (!block.isValid())
	{
		return;
	}

	// The cursor may not stay inside text that is about to vanish; park it on the line above.
	const int cursorLine = textCursor().blockNumber();
	if (!visible && first > 0 && cursorLine >= first && cursorLine <= last)
	{
		QTextCursor cursor(document()->findBlockByNumber(first - 1));
		cursor.movePosition(QTextCursor::EndOfBlock);
		setTextCursor(cursor);
	}

	const int from = block.position();
	int to = from;
	for (; block.isValid() && block.blockNumber() <= last; block = block.next())
	{
		block.setVisible(visible);
		to = block.position() + block.length();
	}
	document()->markContentsDirty(from, to - from);
	viewport()->update();
	m_gutter->update();
}

void WBQtMapIniCodeEditor::applyVisibility(const QVector<bool> &visible)
{
	const int cursorLine = textCursor().blockNumber();
	if (cursorLine < visible.size() && !visible.at(cursorLine))
	{
		int header = cursorLine;
		while (header > 0 && !visible.at(header))
		{
			--header;
		}
		QTextCursor cursor(document()->findBlockByNumber(header));
		cursor.movePosition(QTextCursor::EndOfBlock);
		setTextCursor(cursor);
	}

	int from = -1;
	int to = -1;
	int line = 0;
	for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next(), ++line)
	{
		const bool want = (line < visible.size()) ? visible.at(line) : true;
		if (block.isVisible() != want)
		{
			block.setVisible(want);
			if (from < 0)
			{
				from = block.position();
			}
			to = block.position() + block.length();
		}
	}
	if (from >= 0)
	{
		document()->markContentsDirty(from, to - from);
		viewport()->update();
		m_gutter->update();
	}
}

void WBQtMapIniCodeEditor::setFoldRanges(const QList<QPair<int, int> > &ranges)
{
	// A header whose next line is hidden was folded; keep it so, and show everything else.
	QVector<bool> visible(document()->blockCount(), true);
	for (int i = 0; i < ranges.size(); ++i)
	{
		const int header = ranges.at(i).first;
		const int last = ranges.at(i).second;
		const QTextBlock next = document()->findBlockByNumber(header + 1);
		if (last <= header || !next.isValid() || next.isVisible())
		{
			continue;
		}
		for (int l = header + 1; l <= last && l < visible.size(); ++l)
		{
			visible[l] = false;
		}
	}
	m_foldRanges = ranges;
	m_rangesStale = false;
	applyVisibility(visible);
	m_gutter->update();
}

void WBQtMapIniCodeEditor::setFolded(int headerLine, bool folded)
{
	const int index = foldRangeAt(headerLine);
	if (index >= 0)
	{
		setLinesVisible(headerLine + 1, m_foldRanges.at(index).second, !folded);
	}
}

void WBQtMapIniCodeEditor::setAllFolded(bool folded)
{
	if (m_rangesStale)
	{
		return;
	}
	QVector<bool> visible(document()->blockCount(), true);
	if (folded)
	{
		for (int i = 0; i < m_foldRanges.size(); ++i)
		{
			for (int l = m_foldRanges.at(i).first + 1; l <= m_foldRanges.at(i).second && l < visible.size(); ++l)
			{
				visible[l] = false;
			}
		}
	}
	applyVisibility(visible);
}

void WBQtMapIniCodeEditor::revealLine(int line)
{
	// The hidden run around the line is its fold; showing it needs no ranges, which may be stale.
	QTextBlock first = document()->findBlockByNumber(line);
	if (!first.isValid() || first.isVisible())
	{
		return;
	}
	QTextBlock last = first;
	while (first.previous().isValid() && !first.previous().isVisible())
	{
		first = first.previous();
	}
	while (last.next().isValid() && !last.next().isVisible())
	{
		last = last.next();
	}
	setLinesVisible(first.blockNumber(), last.blockNumber(), true);
}

void WBQtMapIniCodeEditor::scrollLineToTop(int line)
{
	const QTextBlock block = document()->findBlockByNumber(line);
	if (!block.isValid())
	{
		return;
	}
	setTextCursor(QTextCursor(block));
	// Scrolling back from the end leaves the cursor's line on the top row.
	verticalScrollBar()->setValue(verticalScrollBar()->maximum());
	ensureCursorVisible();
}

void WBQtMapIniCodeEditor::paintGutter(QPaintEvent *event)
{
	QPainter painter(m_gutter);
	painter.fillRect(event->rect(), palette().color(QPalette::Window));
	const QColor dim = palette().color(QPalette::Disabled, QPalette::Text);
	const int numbers = numberWidth();
	const int lineHeight = fontMetrics().height();

	QTextBlock block = firstVisibleBlock();
	int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
	while (block.isValid() && top <= event->rect().bottom())
	{
		if (!block.isVisible())
		{
			block = block.next();
			continue;
		}
		const int bottom = top + qRound(blockBoundingRect(block).height());
		if (bottom >= event->rect().top())
		{
			const int line = block.blockNumber();
			painter.setPen(dim);
			painter.drawText(0, top, numbers - 6, lineHeight, Qt::AlignRight,
				QString::number(m_firstLine + line));

			if (m_folding && foldRangeAt(line) >= 0)
			{
				const qreal cx = numbers + kFoldColumn / 2.0;
				const qreal cy = top + lineHeight / 2.0;
				QPainterPath arrow;
				if (isFolded(line))
				{
					arrow.moveTo(cx - 2.5, cy - 4.0);
					arrow.lineTo(cx + 3.0, cy);
					arrow.lineTo(cx - 2.5, cy + 4.0);
				}
				else
				{
					arrow.moveTo(cx - 4.0, cy - 2.5);
					arrow.lineTo(cx + 4.0, cy - 2.5);
					arrow.lineTo(cx, cy + 3.0);
				}
				arrow.closeSubpath();
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.fillPath(arrow, dim);
				painter.setRenderHint(QPainter::Antialiasing, false);
			}
		}
		top = bottom;
		block = block.next();
	}
}

void WBQtMapIniCodeEditor::gutterPressed(const QPoint &pos)
{
	if (!m_folding || pos.x() < numberWidth())
	{
		return;
	}
	const int line = cursorForPosition(QPoint(1, pos.y())).blockNumber();
	if (foldRangeAt(line) >= 0)
	{
		setFolded(line, !isFolded(line));
	}
}

void WBQtMapIniCodeEditor::paintEvent(QPaintEvent *event)
{
	QPlainTextEdit::paintEvent(event);
	if (!m_folding || m_rangesStale || m_foldRanges.isEmpty())
	{
		return;
	}

	// A folded header ends in a "... N lines" tag counting its hidden body.
	QPainter painter(viewport());
	painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
	const QFontMetrics metrics = fontMetrics();
	const QPointF offset = contentOffset();
	for (QTextBlock block = firstVisibleBlock(); block.isValid(); block = block.next())
	{
		if (!block.isVisible())
		{
			continue;
		}
		const QRectF rect = blockBoundingGeometry(block).translated(offset);
		if (rect.top() > event->rect().bottom())
		{
			break;
		}
		const int line = block.blockNumber();
		if (!isFolded(line))
		{
			continue;
		}
		const QTextLayout *layout = block.layout();
		const qreal textWidth = (layout != NULL && layout->lineCount() > 0)
			? layout->lineAt(0).naturalTextWidth() : 0.0;
		const int hidden = m_foldRanges.at(foldRangeAt(line)).second - line;
		const QString label = QString("... %1 lines").arg(hidden);
		const QRectF box(rect.left() + document()->documentMargin() + textWidth + 10.0, rect.top() + 1.0,
			metrics.horizontalAdvance(label) + 10.0, metrics.height() - 2.0);
		painter.drawRoundedRect(box, 3.0, 3.0);
		painter.drawText(box, Qt::AlignCenter, label);
	}
}
