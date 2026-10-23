// WBQtMapIniCodeEditor.h -- the map.ini code view shared by the map.ini editor and the loader's report.
#ifndef WB_QT_MAPINI_CODE_EDITOR_H
#define WB_QT_MAPINI_CODE_EDITOR_H

#include <QColor>
#include <QList>
#include <QPair>
#include <QPlainTextEdit>
#include <QString>
#include <QVector>

// Badge text and colour for a WBQT_MAPINI_BLOCK_* status, shared by the report and the editor.
QString WBQtMapIniStatusText(int status);
QColor WBQtMapIniStatusColour(int status);

// A plain-text editor with a line-number gutter and, when enabled, fold arrows on top-level blocks.
class WBQtMapIniCodeEditor : public QPlainTextEdit
{
	Q_OBJECT
public:
	explicit WBQtMapIniCodeEditor(QWidget *parent = 0);

	// A read-only, map.ini-coloured view of `source` numbered from `firstLine`.
	static WBQtMapIniCodeEditor *createReadOnly(const QString &source, int firstLine, QWidget *parent);

	// The number beside the first line; a block excerpt shows its map.ini line numbers.
	void setFirstLineNumber(int number);

	void setFoldingEnabled(bool enabled);
	// Foldable blocks as (header line, last line), 0-based; surviving folds stay folded.
	void setFoldRanges(const QList<QPair<int, int> > &ranges);
	bool isFolded(int headerLine) const;
	void setFolded(int headerLine, bool folded);
	void setAllFolded(bool folded);
	// Unfold whatever hides this line.
	void revealLine(int line);
	// Put the cursor on this line and scroll it to the top of the view.
	void scrollLineToTop(int line);

	int gutterWidth() const;
	void paintGutter(QPaintEvent *event);
	void gutterPressed(const QPoint &pos);

protected:
	virtual void changeEvent(QEvent *event);
	virtual void resizeEvent(QResizeEvent *event);
	virtual void paintEvent(QPaintEvent *event);

private slots:
	void onBlockCountChanged(int count);
	void onUpdateRequest(const QRect &rect, int dy);
	void onCursorPositionChanged();

private:
	class Gutter;

	int foldRangeAt(int headerLine) const;		///< index into m_foldRanges, or -1
	int numberWidth() const;
	void updateGutterGeometry();
	void setLinesVisible(int first, int last, bool visible);
	// Show or hide each line by index (missing entries show), relaying out only what changed.
	void applyVisibility(const QVector<bool> &visible);

	Gutter *m_gutter;
	int m_firstLine;
	bool m_folding;
	bool m_rangesStale;		///< lines were added or removed since the last setFoldRanges
	QList<QPair<int, int> > m_foldRanges;
};

#endif // WB_QT_MAPINI_CODE_EDITOR_H
